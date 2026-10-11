#include "App.h"

#include <Arduino.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "app/AutoPowerOffController.h"
#include "app/BacklightController.h"
#include "app/BuildInfo.h"
#include "app/IdleTimer.h"
#include "app/menu/AppMenuCommands.h"
#include "app/menu/OssmMenuSources.h"
#include "app/menu/RemoteMenuSources.h"
#include "app/screens/BootScreen.h"
#include "app/screens/ConnectScreen.h"
#include "app/screens/DiagnosticsScreen.h"
#include "app/screens/OssmControlScreen.h"
#include "app/screens/OssmPatternsScreen.h"
#include "app/screens/ScanScreen.h"
#include "app/screens/WelcomeScreen.h"
#include "devices/ossm/OssmClient.h"
#include "devices/ossm/OssmConnectionController.h"
#include "devices/ossm/OssmControl.h"
#include "devices/ossm/OssmDiscovery.h"
#include "diagnostics/Log.h"
#include "diagnostics/SerialConsole.h"
#include "diagnostics/SystemInfo.h"
#include "menu/MenuController.h"
#include "menu/MenuPage.h"
#include "platform/LvglPort.h"
#include "platform/M5Platform.h"
#include "platform/RemoteInput.h"
#include "settings/SettingsStore.h"
#include "ui/MenuScreen.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr const char* kTag = "app";
constexpr std::uint32_t kBatteryUpdateIntervalMs = 10000;
constexpr std::uint32_t kExternalPowerPollIntervalMs = 1000;

enum class Screen : std::uint8_t {
    Boot,
    Welcome,
    Scan,
    Connect,
    OssmControl,
    OssmPatterns,
    // The quick actions from the control screen, the settings from the welcome screen.
    Menu,
    Diagnostics,
};

enum class ConnectionOrigin : std::uint8_t {
    Startup,
    Scan,
};

RemoteInput remoteInput;
ossm::OssmClient ossmClient;
ossm::OssmDiscovery ossmDiscovery;
OssmConnectionController ossmConnection(ossmClient);
OssmControl ossmControl(ossmClient);
SettingsStore settingsStore;
BacklightController backlightController;
AutoPowerOffController autoPowerOffController;
IdleTimer idleTimer;
BootScreen bootScreen;
WelcomeScreen welcomeScreen;
ScanScreen scanScreen(ossmDiscovery);
ConnectScreen connectScreen;
OssmControlScreen ossmControlScreen(ossmControl);
OssmPatternsScreen ossmPatternsScreen(ossmControl);
DiagnosticsScreen diagnosticsScreen(ossmClient);

// Defined below; the menu calls it after saving a setting.
void applySettings();

MenuScreen menuScreen;
OssmSettingsSource ossmSettings(settingsStore, applySettings);
RemoteSettingsSource remoteSettings(settingsStore, backlightController, applySettings);
MenuPage settingsPage;
OssmQuickActionsSource ossmQuickActions(settingsStore, ossmControl, applySettings);
RemoteQuickActionsSource remoteQuickActions(remoteSettings, settingsPage);
MenuPage quickActionsPage;
// The quick action tiles that holding the left or right button runs on the control screen.
MenuItem* leftHeldAction = nullptr;
MenuItem* rightHeldAction = nullptr;

Screen currentScreen = Screen::Boot;
// Where the menu was opened from, and where closing it returns to.
Screen menuReturnScreen = Screen::Welcome;
ConnectionOrigin connectionOrigin = ConnectionOrigin::Startup;
SavedOssmConnection connectionTarget{};
SavedOssmConnection startupConnection{};
bool hasConnectionTarget = false;
bool connectAfterBoot = false;
std::uint32_t lastBatteryUpdateAtMs = 0;
std::uint32_t lastExternalPowerPollAtMs = 0;
bool externalPowerPresent = false;
bool batteryRefreshPending = false;

SavedOssmConnection makeSavedConnection(const ossm::DiscoveredOssm& device) {
    SavedOssmConnection connection;
    connection.address = static_cast<std::uint64_t>(device.address);
    connection.addressType = device.address.getType();
    std::snprintf(connection.name, sizeof(connection.name), "%s", device.name);
    return connection;
}

bool hasInputActivity(const RemoteInputEvents& events) {
    if (events.mxPress || events.leftClick || events.rightClick || events.leftHold ||
        events.rightHold) {
        return true;
    }

    for (const std::int32_t steps : events.encoderSteps) {
        if (steps != 0) {
            return true;
        }
    }

    return false;
}

// Pushes the stored settings to the parts of the remote that use them, at startup and after a
// change. A new setting that affects the remote only needs a line here.
void applySettings() {
    backlightController.setBrightnessLevel(settingsStore.brightnessLevel());
    backlightController.setIdleDimTimeoutSeconds(
        static_cast<std::uint32_t>(settingsStore.idleDimTimeout()));
    autoPowerOffController.setTimeoutSeconds(
        static_cast<std::uint32_t>(settingsStore.idlePowerOffTimeout()));
    lvgl_port::setTouchscreenEnabled(settingsStore.touchscreenEnabled());
    ossmControlScreen.setDepthControlMode(settingsStore.depthControlMode());
    ossmControlScreen.setStrokeEncoderReversed(settingsStore.strokeEncoderReversed());
}

void updateBatteryIndicator(bool force = false) {
    const std::uint32_t now = millis();
    if (!force && now - lastBatteryUpdateAtMs < kBatteryUpdateIntervalMs) {
        return;
    }

    lastBatteryUpdateAtMs = now;
    const int batteryLevel = m5_platform::batteryLevelPercent();
    const bool batteryCharging = m5_platform::batteryCharging();
    welcomeScreen.setBatteryLevel(batteryLevel, batteryCharging);
    ossmControlScreen.setBatteryLevel(batteryLevel, batteryCharging);
}

// Reads the PMIC over I2C, so poll it at a fixed interval and share the cached
// value instead of reading it on every loop pass.
void updateExternalPower() {
    const std::uint32_t now = millis();
    if (now - lastExternalPowerPollAtMs < kExternalPowerPollIntervalMs) {
        return;
    }

    lastExternalPowerPollAtMs = now;
    const bool present = m5_platform::externalPowerPresent();
    // Plugging in or unplugging changes the charging state. Refresh the
    // battery on the following poll so the PMIC has had time to report it.
    const bool refreshBattery = batteryRefreshPending;
    batteryRefreshPending = present != externalPowerPresent;
    if (present && !externalPowerPresent) {
        // Wake the screen to show it is charging.
        idleTimer.reset(now);
    }
    externalPowerPresent = present;
    if (refreshBattery) {
        updateBatteryIndicator(true);
    }
}

// The menu belongs to the OSSM under control when it was opened from the control screen.
bool menuConnected() {
    return menuReturnScreen == Screen::OssmControl && ossmConnection.isReady();
}

void leaveScreen(Screen screen, Screen next) {
    switch (screen) {
        case Screen::Boot:
            bootScreen.leave();
            break;
        case Screen::Welcome:
            welcomeScreen.leave();
            break;
        case Screen::Scan:
            scanScreen.leave();
            break;
        case Screen::Connect:
            connectScreen.leave();
            break;
        case Screen::OssmControl:
            ossmControlScreen.leave();
            break;
        case Screen::OssmPatterns:
            ossmPatternsScreen.leave();
            break;
        case Screen::Menu:
            // Kept open so Back from the diagnostics screen returns to the same row.
            if (next != Screen::Diagnostics) {
                menuScreen.close();
            }
            break;
        case Screen::Diagnostics:
            diagnosticsScreen.leave();
            break;
    }
}

void enterScreen(Screen screen, Screen previous) {
    switch (screen) {
        case Screen::Boot:
            bootScreen.enter();
            break;
        case Screen::Welcome:
            welcomeScreen.enter();
            break;
        case Screen::Scan:
            scanScreen.enter();
            break;
        case Screen::Connect:
            connectScreen.enter();
            break;
        case Screen::OssmControl:
            ossmControlScreen.enter();
            break;
        case Screen::OssmPatterns:
            ossmPatternsScreen.enter();
            break;
        case Screen::Menu: {
            const bool connected = menuConnected();
            ossm::OssmClient::PatternList catalog{};
            if (connected) {
                ossmClient.patternList(catalog);
            }
            ossmSettings.setConnected(connected);
            ossmSettings.setPatternCatalog(catalog);
            menuScreen.setStopAvailable(connected);
            menuScreen.setMotionActive(ossmControl.canStop());
            if (previous == Screen::Diagnostics && menuScreen.isOpen()) {
                menuScreen.resume();
            } else if (connected) {
                menuScreen.open(quickActionsPage, "Close");
            } else {
                menuScreen.open(settingsPage, "Back");
            }
            break;
        }
        case Screen::Diagnostics:
            diagnosticsScreen.setStopAvailable(menuConnected());
            diagnosticsScreen.setMotionActive(ossmControl.canStop());
            diagnosticsScreen.enter();
            break;
    }
}

void navigateTo(Screen screen) {
    if (screen == currentScreen) {
        return;
    }

    const Screen previous = currentScreen;
    leaveScreen(previous, screen);
    currentScreen = screen;
    enterScreen(screen, previous);
    idleTimer.reset(millis());
}

void refreshControlPatternLabel() {
    const int patternId = ossmControl.values().pattern;
    ossmControlScreen.setPatternLabel(patternId, ossmPatternsScreen.patternName(patternId));
}

void startConnectionAttempt() {
    if (currentScreen != Screen::Connect || !hasConnectionTarget) {
        return;
    }

    connectScreen.connectionStarted();
    const NimBLEAddress address(connectionTarget.address, connectionTarget.addressType);
    if (!ossmConnection.connect(address)) {
        connectScreen.connectionFailed();
    }
}

void showConnectionTarget(const SavedOssmConnection& target, ConnectionOrigin origin) {
    connectionTarget = target;
    connectionOrigin = origin;
    hasConnectionTarget = true;

    const NimBLEAddress address(target.address, target.addressType);
    const std::string addressText = address.toString();
    connectScreen.configure(target.name, addressText.c_str());

    ossmPatternsScreen.invalidateCatalog();
    ossmControl.resetDefaults();
    refreshControlPatternLabel();
    navigateTo(Screen::Connect);
    startConnectionAttempt();
}

void handleBootAction(BootScreenAction action) {
    if (action != BootScreenAction::Finished) {
        return;
    }

    if (connectAfterBoot) {
        connectAfterBoot = false;
        showConnectionTarget(startupConnection, ConnectionOrigin::Startup);
    } else {
        navigateTo(Screen::Welcome);
    }
}

// Shows motion the remote changed by itself, e.g. Stop or a ramp step, on the current screen.
void refreshMotion() {
    const bool canStop = ossmControl.canStop();
    if (currentScreen == Screen::OssmControl) {
        ossmControlScreen.refresh();
    } else if (currentScreen == Screen::OssmPatterns) {
        ossmPatternsScreen.refresh();
    } else if (currentScreen == Screen::Menu) {
        menuScreen.setMotionActive(canStop);
        menuScreen.refreshTiles();
    } else if (currentScreen == Screen::Diagnostics) {
        diagnosticsScreen.setMotionActive(canStop);
    }
}

// Back to where the menu was opened from, unless the OSSM went away meanwhile.
void closeMenu() {
    navigateTo(menuConnected() ? Screen::OssmControl : Screen::Welcome);
}

// The same change as picking the other option of the Depth mode setting.
void switchDepthMode() {
    const DepthControlMode mode = settingsStore.depthControlMode() == DepthControlMode::MinMax
                                      ? DepthControlMode::StrokeDepth
                                      : DepthControlMode::MinMax;
    if (!settingsStore.setDepthControlMode(mode)) {
        LOGE(kTag, "Unable to save depth mode");
        return;
    }
    applySettings();
}

// The command of a quick action tile, picked in the menu or held on a button.
void runQuickAction(AppMenuCommand command) {
    switch (command) {
        case AppMenuCommand::SwitchDepthMode:
            // The control screen's range caption shows the new mode.
            switchDepthMode();
            break;
        case AppMenuCommand::Pause:
            ossmControl.togglePause(millis());
            break;
        case AppMenuCommand::PullOut:
            ossmControl.pullOut(millis());
            break;
        case AppMenuCommand::Ramp:
            ossmControl.toggleRamp(
                static_cast<int>(settingsStore.rampIncrease()),
                static_cast<std::uint32_t>(settingsStore.rampDuration()) * 1000,
                millis());
            break;
        case AppMenuCommand::ShowDiagnostics:
        case AppMenuCommand::Disconnect:
            // Rows, not quick actions.
            break;
    }
}

MenuItem*& heldAction(SideButton button) {
    return button == SideButton::Left ? leftHeldAction : rightHeldAction;
}

void showHeldActions() {
    menuScreen.setHeldTiles(leftHeldAction, rightHeldAction);
    ossmControlScreen.setHeldActions(leftHeldAction, rightHeldAction);
}

// Holding a button on the highlighted tile holds its action on that button. Holding the same
// button on it again takes the action off.
void toggleHeldAction(SideButton button) {
    MenuItem* tile = menuScreen.highlightedTile();
    if (tile == nullptr || tile->kind() != MenuItemKind::Action) {
        return;
    }

    MenuItem*& held = heldAction(button);
    MenuItem* next = held == tile ? nullptr : tile;
    if (!settingsStore.setHeldAction(button, next != nullptr ? next->id() : "")) {
        LOGE(kTag, "Unable to save held action");
        return;
    }
    held = next;
    showHeldActions();
}

void runHeldAction(SideButton button) {
    MenuItem* action = heldAction(button);
    if (action == nullptr) {
        return;
    }

    const MenuResult result = action->activate();
    if (result.type == MenuResult::Type::Command) {
        runQuickAction(static_cast<AppMenuCommand>(result.command));
        refreshMotion();
    }
}

void handleMenuCommand(AppMenuCommand command) {
    switch (command) {
        case AppMenuCommand::SwitchDepthMode:
        case AppMenuCommand::Pause:
        case AppMenuCommand::PullOut:
        case AppMenuCommand::Ramp:
            runQuickAction(command);
            closeMenu();
            break;
        case AppMenuCommand::ShowDiagnostics:
            navigateTo(Screen::Diagnostics);
            break;
        case AppMenuCommand::Disconnect:
            // The client stops motion before it drops the link. A requested disconnect raises
            // no readiness-lost event, so reset the control state here.
            ossmConnection.disconnect();
            ossmControl.handleReadinessLost();
            ossmPatternsScreen.invalidateCatalog();
            LOGI(kTag, "Disconnected from OSSM");
            navigateTo(Screen::Welcome);
            break;
    }
}

void handleMenuEvent(const MenuEvent& event) {
    switch (event.type) {
        case MenuEvent::Type::Closed:
            closeMenu();
            break;
        case MenuEvent::Type::Command:
            handleMenuCommand(static_cast<AppMenuCommand>(event.command));
            break;
        case MenuEvent::Type::None:
            break;
    }
}

void handleConnectionEvents(const OssmConnectionEvents& events) {
    if (events.readinessLost) {
        ossmControl.handleReadinessLost();
        ossmPatternsScreen.invalidateCatalog();
        LOGW(kTag, "OSSM readiness lost; returning to welcome");
        navigateTo(Screen::Welcome);
        return;
    }

    if (events.becameConnected && hasConnectionTarget) {
        if (currentScreen == Screen::Connect) {
            connectScreen.connectionEstablished();
        }
    }

    if (events.connectionFailed) {
        ossmControl.handleReadinessLost();
        ossmPatternsScreen.invalidateCatalog();
        if (currentScreen == Screen::Connect && hasConnectionTarget) {
            connectScreen.connectionFailed();
        } else {
            navigateTo(Screen::Welcome);
        }
        return;
    }

    if (events.becameReady) {
        if (hasConnectionTarget && !settingsStore.setSavedOssmConnection(connectionTarget)) {
            LOGE(kTag, "Unable to save OSSM connection");
        }
        diagnosticsScreen.setOssmName(connectionTarget.name);
        hasConnectionTarget = false;
        ossmControl.stop();
        // Resolve by name for this firmware; an unavailable preference remains stored.
        int defaultPatternId = 0;
        ossm::OssmClient::PatternList catalog;
        if (ossmClient.patternList(catalog)) {
            ossmPatternsScreen.setCatalog(catalog);
            for (std::size_t index = 0; index < catalog.count; ++index) {
                if (settingsStore.matchesDefaultPattern(catalog.patterns[index].name)) {
                    defaultPatternId = catalog.patterns[index].id;
                    break;
                }
            }
        } else {
            ossmPatternsScreen.invalidateCatalog();
            LOGW(kTag, "OSSM pattern catalog unavailable after readiness");
        }
        ossmControl.setPattern(defaultPatternId);
        refreshControlPatternLabel();
        navigateTo(Screen::OssmControl);
    }
}

void dispatchInput(const RemoteInputEvents& events, bool connectedScreenWasReady) {
    switch (currentScreen) {
        case Screen::Boot:
            break;
        case Screen::Welcome:
            switch (welcomeScreen.update(events)) {
                case WelcomeScreenAction::Settings:
                    app::showMenu();
                    break;
                case WelcomeScreenAction::Scan:
                    app::showScan();
                    break;
                case WelcomeScreenAction::None:
                    break;
            }
            break;
        case Screen::Scan:
            switch (scanScreen.update(events)) {
                case ScanScreenAction::Cancel:
                    app::cancelScan();
                    break;
                case ScanScreenAction::Connect:
                    app::connectSelectedScanDevice();
                    break;
                case ScanScreenAction::None:
                    break;
            }
            break;
        case Screen::Connect:
            switch (connectScreen.update(events)) {
                case ConnectScreenAction::Cancel:
                    app::cancelConnection();
                    break;
                case ConnectScreenAction::Retry:
                    startConnectionAttempt();
                    break;
                case ConnectScreenAction::None:
                    break;
            }
            break;
        case Screen::OssmControl:
            if (!connectedScreenWasReady || !ossmConnection.isReady()) {
                break;
            }
            switch (ossmControlScreen.update(events)) {
                case OssmControlScreenAction::Menu:
                    app::showMenu();
                    break;
                case OssmControlScreenAction::Patterns:
                    app::showOssmPatterns();
                    break;
                case OssmControlScreenAction::LeftHeld:
                    runHeldAction(SideButton::Left);
                    break;
                case OssmControlScreenAction::RightHeld:
                    runHeldAction(SideButton::Right);
                    break;
                case OssmControlScreenAction::None:
                    break;
            }
            break;
        case Screen::OssmPatterns:
            if (!connectedScreenWasReady || !ossmConnection.isReady()) {
                break;
            }
            switch (ossmPatternsScreen.update(events)) {
                case OssmPatternsScreenAction::Cancel:
                    app::cancelOssmPatterns();
                    break;
                case OssmPatternsScreenAction::Select:
                    app::selectOssmPattern();
                    break;
                case OssmPatternsScreenAction::None:
                    break;
            }
            break;
        case Screen::Menu:
            if (events.leftHold) {
                toggleHeldAction(SideButton::Left);
            }
            if (events.rightHold) {
                toggleHeldAction(SideButton::Right);
            }
            handleMenuEvent(menuScreen.update(events));
            break;
        case Screen::Diagnostics:
            if (diagnosticsScreen.update(events) == DiagnosticsScreenAction::Back) {
                navigateTo(Screen::Menu);
            }
            break;
    }
}

}  // namespace

namespace app {

void begin() {
    m5_platform::begin();
    LOGI(kTag, "%s %s (%s)", APP_DISPLAY_NAME, buildInfo().buildVersion, BUILD_TARGET);
    LOGI(kTag, "Reset reason: %s", system_info::resetReasonName());
    serial_console::begin();
    if (!settingsStore.begin()) {
        LOGE(kTag, "Redux settings initialization failed");
    }
    const std::uint32_t now = millis();
    idleTimer.begin(now);
    backlightController.begin(
        settingsStore.brightnessLevel(),
        static_cast<std::uint32_t>(settingsStore.idleDimTimeout()));
    externalPowerPresent = m5_platform::externalPowerPresent();
    lastExternalPowerPollAtMs = now;
    autoPowerOffController.begin(
        static_cast<std::uint32_t>(settingsStore.idlePowerOffTimeout()), now, externalPowerPresent);
    remoteInput.begin();
    SavedOssmConnection savedConnection;
    const bool hasSavedConnection = settingsStore.savedOssmConnection(savedConnection);
    const bool autoConnectEnabled = settingsStore.autoConnectEnabled();
    lvgl_port::begin();
    applySettings();
    ui_init();

    bootScreen.enter(hasSavedConnection && autoConnectEnabled);
    welcomeScreen.begin();
    scanScreen.begin();
    connectScreen.begin();
    ossmControlScreen.begin();
    ossmPatternsScreen.begin();
    diagnosticsScreen.begin();
    settingsPage.addSource(ossmSettings);
    settingsPage.addSource(remoteSettings);
    quickActionsPage.addSource(ossmQuickActions);
    quickActionsPage.addSource(remoteQuickActions);
    menuScreen.begin();
    leftHeldAction = ossmQuickActions.tile(settingsStore.heldAction(SideButton::Left));
    rightHeldAction = ossmQuickActions.tile(settingsStore.heldAction(SideButton::Right));
    showHeldActions();
    updateBatteryIndicator(true);
    ossmConnection.begin(APP_DISPLAY_NAME);
    if (!ossmDiscovery.begin()) {
        LOGE(kTag, "OSSM discovery initialization failed");
    }
    connectAfterBoot = hasSavedConnection && autoConnectEnabled;
    if (connectAfterBoot) {
        startupConnection = savedConnection;
    }
}

void update() {
    m5_platform::update();
    serial_console::update();
    updateExternalPower();
    updateBatteryIndicator();

    const bool connectedScreenWasReady =
        (currentScreen == Screen::OssmControl || currentScreen == Screen::OssmPatterns) &&
        ossmConnection.isReady();
    remoteInput.update();
    const RemoteInputEvents inputEvents = remoteInput.takeEvents();
    if (currentScreen == Screen::Boot && remoteInput.leftPressed()) {
        remoteInput.suppressLeftPress();
        if (connectAfterBoot) {
            connectAfterBoot = false;
            bootScreen.showAutoConnectSkipped();
        }
    }
    if (hasInputActivity(inputEvents)) {
        idleTimer.reset(millis());
    }

    bool inputConsumed = false;
    if (currentScreen == Screen::Connect && inputEvents.leftClick) {
        cancelConnection();
        inputConsumed = true;
    }

    const Screen screenBeforeConnectionUpdate = currentScreen;
    handleConnectionEvents(ossmConnection.update());
    const bool screenChanged = currentScreen != screenBeforeConnectionUpdate;

    if (inputEvents.mxPress) {
        stopMotion();
    } else if (!screenChanged && !inputConsumed) {
        dispatchInput(inputEvents, connectedScreenWasReady);
    }
    // Pauses, ramps and pull outs move on by themselves.
    if (ossmControl.update(millis())) {
        refreshMotion();
    }

    ui_tick();
    lvgl_port::update();
    if (currentScreen == Screen::Boot) {
        handleBootAction(bootScreen.update());
    }
    const std::uint32_t now = millis();
    if (lvgl_port::takeTouchActivity()) {
        idleTimer.reset(now);
    }
    if (ossmControl.values().speed > 0) {
        idleTimer.reset(now);
    }
    const std::uint32_t idleDurationMs = idleTimer.idleForMs(now);
    backlightController.update(idleDurationMs);
    autoPowerOffController.update(now, idleDurationMs, externalPowerPresent);
}

void stopMotion() {
    ossmControl.stop();
    refreshMotion();
}

void showMenu() {
    if (currentScreen == Screen::Welcome) {
        menuReturnScreen = Screen::Welcome;
    } else if (currentScreen == Screen::OssmControl && ossmConnection.isReady()) {
        menuReturnScreen = Screen::OssmControl;
    } else {
        return;
    }
    navigateTo(Screen::Menu);
}

void activateMenuBack() {
    if (currentScreen == Screen::Menu) {
        menuScreen.requestBack();
    }
}

void activateMenuSelect() {
    if (currentScreen == Screen::Menu) {
        menuScreen.requestSelect();
    }
}

void activateDiagnosticsBack() {
    if (currentScreen == Screen::Diagnostics) {
        diagnosticsScreen.requestBack();
    }
}

void activateDiagnosticsAction() {
    if (currentScreen == Screen::Diagnostics) {
        diagnosticsScreen.requestAction();
    }
}

void showScan() {
    if (currentScreen == Screen::Welcome) {
        navigateTo(Screen::Scan);
    }
}

void cancelScan() {
    if (currentScreen != Screen::Scan) {
        return;
    }

    navigateTo(Screen::Welcome);
}

void connectSelectedScanDevice() {
    if (currentScreen != Screen::Scan) {
        return;
    }

    ossm::DiscoveredOssm selectedDevice;
    if (!scanScreen.selectedDevice(selectedDevice)) {
        return;
    }

    showConnectionTarget(makeSavedConnection(selectedDevice), ConnectionOrigin::Scan);
}

void cancelConnection() {
    if (currentScreen != Screen::Connect) {
        return;
    }

    const ConnectionOrigin origin = connectionOrigin;
    hasConnectionTarget = false;
    ossmConnection.disconnect();
    navigateTo(origin == ConnectionOrigin::Scan ? Screen::Scan : Screen::Welcome);
}

void showOssmPatterns() {
    if (currentScreen == Screen::OssmControl && ossmConnection.isReady()) {
        navigateTo(Screen::OssmPatterns);
    }
}

void cancelOssmPatterns() {
    if (currentScreen == Screen::OssmPatterns && ossmConnection.isReady()) {
        navigateTo(Screen::OssmControl);
    }
}

void selectOssmPattern() {
    if (currentScreen != Screen::OssmPatterns || !ossmConnection.isReady()) {
        return;
    }

    int patternId = 0;
    if (ossmPatternsScreen.selectedPatternId(patternId) && ossmControl.setPattern(patternId)) {
        refreshControlPatternLabel();
        navigateTo(Screen::OssmControl);
    }
}

}  // namespace app

}  // namespace m5_redux
