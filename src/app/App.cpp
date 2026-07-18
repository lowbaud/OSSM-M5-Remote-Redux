#include "App.h"

#include <Arduino.h>

#include <cstdint>
#include <cstdio>
#include <string>

#include "app/AutoPowerOffController.h"
#include "app/BacklightController.h"
#include "app/IdleTimer.h"
#include "app/screens/BootScreen.h"
#include "app/screens/ConnectScreen.h"
#include "app/screens/OssmControlScreen.h"
#include "app/screens/OssmPatternsScreen.h"
#include "app/screens/ScanScreen.h"
#include "app/screens/SettingsScreen.h"
#include "app/screens/WelcomeScreen.h"
#include "devices/ossm/OssmClient.h"
#include "devices/ossm/OssmConnectionController.h"
#include "devices/ossm/OssmControl.h"
#include "devices/ossm/OssmDiscovery.h"
#include "platform/LvglPort.h"
#include "platform/M5Platform.h"
#include "platform/RemoteInput.h"
#include "settings/SettingsStore.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr std::uint32_t kBatteryUpdateIntervalMs = 10000;

enum class Screen : std::uint8_t {
    Boot,
    Welcome,
    Scan,
    Connect,
    OssmControl,
    OssmPatterns,
    Settings,
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
SettingsScreen settingsScreen(settingsStore);

Screen currentScreen = Screen::Boot;
Screen settingsReturnScreen = Screen::Welcome;
ConnectionOrigin connectionOrigin = ConnectionOrigin::Startup;
SavedOssmConnection connectionTarget{};
SavedOssmConnection startupConnection{};
bool hasConnectionTarget = false;
bool connectAfterBoot = false;
std::uint32_t lastBatteryUpdateAtMs = 0;

SavedOssmConnection makeSavedConnection(const ossm::DiscoveredOssm& device) {
    SavedOssmConnection connection;
    connection.address = static_cast<std::uint64_t>(device.address);
    connection.addressType = device.address.getType();
    std::snprintf(connection.name, sizeof(connection.name), "%s", device.name);
    return connection;
}

bool hasInputActivity(const RemoteInputEvents& events) {
    if (events.mxPress || events.leftClick || events.rightClick) {
        return true;
    }

    for (const std::int32_t steps : events.encoderSteps) {
        if (steps != 0) {
            return true;
        }
    }

    return false;
}

void updateBatteryIndicator(bool force = false) {
    const std::uint32_t now = millis();
    if (!force && now - lastBatteryUpdateAtMs < kBatteryUpdateIntervalMs) {
        return;
    }

    lastBatteryUpdateAtMs = now;
    const int batteryLevel = m5_platform::batteryLevelPercent();
    welcomeScreen.setBatteryLevel(batteryLevel);
    ossmControlScreen.setBatteryLevel(batteryLevel);
}

void leaveScreen(Screen screen) {
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
        case Screen::Settings:
            backlightController.setBrightnessLevel(settingsStore.brightnessLevel());
            settingsScreen.leave();
            break;
    }
}

void enterScreen(Screen screen) {
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
        case Screen::Settings:
            settingsScreen.setStopAvailable(
                settingsReturnScreen == Screen::OssmControl && ossmConnection.isReady());
            settingsScreen.setMotionActive(ossmControl.values().speed > 0);
            settingsScreen.enter();
            break;
    }
}

void navigateTo(Screen screen) {
    if (screen == currentScreen) {
        return;
    }

    leaveScreen(currentScreen);
    currentScreen = screen;
    enterScreen(currentScreen);
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

void handleSettingsEvent(const SettingsScreenEvent& event) {
    switch (event.action) {
        case SettingsScreenAction::Back:
            if (settingsReturnScreen == Screen::OssmControl && ossmConnection.isReady()) {
                navigateTo(Screen::OssmControl);
            } else {
                navigateTo(Screen::Welcome);
            }
            break;
        case SettingsScreenAction::PreviewBrightness:
            backlightController.setBrightnessLevel(event.brightnessLevel);
            break;
        case SettingsScreenAction::RestoreBrightness:
            backlightController.setBrightnessLevel(settingsStore.brightnessLevel());
            break;
        case SettingsScreenAction::CommitBrightness:
            if (settingsStore.setBrightnessLevel(event.brightnessLevel)) {
                backlightController.setBrightnessLevel(event.brightnessLevel);
                settingsScreen.commitSucceeded();
            } else {
                backlightController.setBrightnessLevel(settingsStore.brightnessLevel());
                settingsScreen.commitFailed();
                Serial.println("Unable to save display brightness setting");
            }
            break;
        case SettingsScreenAction::CommitIdleDimTimeout:
            if (settingsStore.setIdleDimTimeout(event.idleDimTimeout)) {
                backlightController.setIdleDimTimeoutSeconds(
                    static_cast<std::uint32_t>(event.idleDimTimeout));
                idleTimer.reset(millis());
                settingsScreen.commitSucceeded();
            } else {
                settingsScreen.commitFailed();
                Serial.println("Unable to save idle dim setting");
            }
            break;
        case SettingsScreenAction::CommitIdlePowerOffTimeout:
            if (settingsStore.setIdlePowerOffTimeout(event.idlePowerOffTimeout)) {
                const std::uint32_t now = millis();
                autoPowerOffController.setTimeoutSeconds(
                    static_cast<std::uint32_t>(event.idlePowerOffTimeout));
                idleTimer.reset(now);
                settingsScreen.commitSucceeded();
            } else {
                settingsScreen.commitFailed();
                Serial.println("Unable to save idle power-off setting");
            }
            break;
        case SettingsScreenAction::CommitAutoConnect:
            if (settingsStore.setAutoConnectEnabled(event.autoConnectEnabled)) {
                settingsScreen.commitSucceeded();
            } else {
                settingsScreen.commitFailed();
                Serial.println("Unable to save auto-connect setting");
            }
            break;
        case SettingsScreenAction::None:
            break;
    }
}

void handleConnectionEvents(const OssmConnectionEvents& events) {
    if (events.readinessLost) {
        ossmControl.handleReadinessLost();
        ossmPatternsScreen.invalidateCatalog();
        Serial.println("OSSM readiness lost; returning to welcome");
        navigateTo(Screen::Welcome);
        return;
    }

    if (events.becameConnected && hasConnectionTarget) {
        if (!settingsStore.setSavedOssmConnection(connectionTarget)) {
            Serial.println("Unable to save OSSM connection");
        }
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
        hasConnectionTarget = false;
        ossmControl.stop();
        ossm::OssmClient::PatternList catalog;
        if (ossmClient.patternList(catalog)) {
            ossmPatternsScreen.setCatalog(catalog);
        } else {
            ossmPatternsScreen.invalidateCatalog();
            Serial.println("OSSM pattern catalog unavailable after readiness");
        }
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
                    app::showSettings();
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
                case OssmControlScreenAction::Settings:
                    app::showSettings();
                    break;
                case OssmControlScreenAction::Patterns:
                    app::showOssmPatterns();
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
        case Screen::Settings:
            handleSettingsEvent(settingsScreen.update(events));
            break;
    }
}

}  // namespace

namespace app {

void begin() {
    m5_platform::begin();
    if (!settingsStore.begin()) {
        Serial.println("Redux settings initialization failed");
    }
    const std::uint32_t now = millis();
    idleTimer.begin(now);
    backlightController.begin(
        settingsStore.brightnessLevel(),
        static_cast<std::uint32_t>(settingsStore.idleDimTimeout()));
    autoPowerOffController.begin(
        static_cast<std::uint32_t>(settingsStore.idlePowerOffTimeout()),
        now,
        m5_platform::externalPowerPresent());
    remoteInput.begin();
    SavedOssmConnection savedConnection;
    const bool hasSavedConnection = settingsStore.savedOssmConnection(savedConnection);
    const bool autoConnectEnabled = settingsStore.autoConnectEnabled();
    lvgl_port::begin();
    ui_init();

    bootScreen.enter(hasSavedConnection && autoConnectEnabled);
    welcomeScreen.begin();
    scanScreen.begin();
    connectScreen.begin();
    ossmControlScreen.begin();
    ossmPatternsScreen.begin();
    settingsScreen.begin();
    updateBatteryIndicator(true);
    ossmConnection.begin(APP_DISPLAY_NAME);
    if (!ossmDiscovery.begin()) {
        Serial.println("OSSM discovery initialization failed");
    }

    connectAfterBoot = hasSavedConnection && autoConnectEnabled;
    if (connectAfterBoot) {
        startupConnection = savedConnection;
    }
}

void update() {
    m5_platform::update();
    updateBatteryIndicator();

    const bool connectedScreenWasReady =
        (currentScreen == Screen::OssmControl || currentScreen == Screen::OssmPatterns) &&
        ossmConnection.isReady();
    remoteInput.update();
    const RemoteInputEvents inputEvents = remoteInput.takeEvents();
    if (currentScreen == Screen::Boot && remoteInput.leftPressed()) {
        remoteInput.suppressLeftClick();
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
    autoPowerOffController.update(now, idleDurationMs, m5_platform::externalPowerPresent());
}

void stopMotion() {
    ossmControl.stop();
    if (currentScreen == Screen::OssmControl) {
        ossmControlScreen.refresh();
    } else if (currentScreen == Screen::OssmPatterns) {
        ossmPatternsScreen.refresh();
    } else if (currentScreen == Screen::Settings) {
        settingsScreen.setMotionActive(false);
    }
}

void showSettings() {
    if (currentScreen != Screen::Welcome &&
        (currentScreen != Screen::OssmControl || !ossmConnection.isReady())) {
        return;
    }

    settingsReturnScreen = currentScreen;
    navigateTo(Screen::Settings);
}

void activateSettingsBack() {
    if (currentScreen == Screen::Settings) {
        settingsScreen.requestBack();
    }
}

void activateSettingsSelect() {
    if (currentScreen == Screen::Settings) {
        settingsScreen.requestSelect();
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
