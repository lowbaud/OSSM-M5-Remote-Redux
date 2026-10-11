#include "RemoteMenuSources.h"

#include "app/menu/AppMenuCommands.h"

namespace m5_redux {

namespace {

const SettingChoiceItem<BrightnessOption, BrightnessLevel>::Setting kBrightnessSetting = {
    SettingsStore::kBrightnessOptionCount,
    &SettingsStore::brightnessOption,
    &SettingsStore::brightnessOptionIndex,
    &BrightnessOption::level,
    &SettingsStore::brightnessLevel,
    &SettingsStore::setBrightnessLevel,
};

const SettingChoiceItem<IdleDimOption, IdleDimTimeout>::Setting kIdleDimSetting = {
    SettingsStore::kIdleDimOptionCount,
    &SettingsStore::idleDimOption,
    &SettingsStore::idleDimOptionIndex,
    &IdleDimOption::timeout,
    &SettingsStore::idleDimTimeout,
    &SettingsStore::setIdleDimTimeout,
};

const SettingChoiceItem<IdlePowerOffOption, IdlePowerOffTimeout>::Setting kIdlePowerOffSetting = {
    SettingsStore::kIdlePowerOffOptionCount,
    &SettingsStore::idlePowerOffOption,
    &SettingsStore::idlePowerOffOptionIndex,
    &IdlePowerOffOption::timeout,
    &SettingsStore::idlePowerOffTimeout,
    &SettingsStore::setIdlePowerOffTimeout,
};

const SettingChoiceItem<AutoConnectOption, bool>::Setting kAutoConnectSetting = {
    SettingsStore::kAutoConnectOptionCount,
    &SettingsStore::autoConnectOption,
    &SettingsStore::autoConnectOptionIndex,
    &AutoConnectOption::enabled,
    &SettingsStore::autoConnectEnabled,
    &SettingsStore::setAutoConnectEnabled,
};

const SettingChoiceItem<TouchscreenOption, bool>::Setting kTouchscreenSetting = {
    SettingsStore::kTouchscreenOptionCount,
    &SettingsStore::touchscreenOption,
    &SettingsStore::touchscreenOptionIndex,
    &TouchscreenOption::enabled,
    &SettingsStore::touchscreenEnabled,
    &SettingsStore::setTouchscreenEnabled,
};

}  // namespace

RemoteSettingsSource::RemoteSettingsSource(
    SettingsStore& settings, BacklightController& backlight, ApplySettingsFn applySettings)
    : brightness_(settings, backlight, applySettings),
      idleDim_("remote.dim", "Dim", settings, kIdleDimSetting, applySettings),
      idlePowerOff_("remote.poweroff", "Power off", settings, kIdlePowerOffSetting, applySettings),
      autoConnect_(
          "remote.autoconnect", "Auto-connect", settings, kAutoConnectSetting, applySettings),
      touchscreen_(
          "remote.touchscreen", "Touchscreen", settings, kTouchscreenSetting, applySettings),
      diagnostics_(
          "remote.diagnostics",
          "Diagnostics",
          static_cast<MenuCommand>(AppMenuCommand::ShowDiagnostics),
          MenuCommandItem::Style::OpensScreen) {}

void RemoteSettingsSource::addItems(MenuPage& page) {
    page.add(brightness_);
    page.add(idleDim_);
    page.add(idlePowerOff_);
    page.add(autoConnect_);
    page.add(touchscreen_);
    page.add(diagnostics_);
}

RemoteQuickActionsSource::RemoteQuickActionsSource(MenuPage& settingsPage)
    : settingsLink_("remote.settings", "Settings", settingsPage) {}

void RemoteQuickActionsSource::addItems(MenuPage& page) {
    page.add(settingsLink_);
}

BrightnessItem::BrightnessItem(
    SettingsStore& settings, BacklightController& backlight, ApplySettingsFn applySettings)
    : SettingChoiceItem(
          "remote.brightness", "Brightness", settings, kBrightnessSetting, applySettings),
      backlight_(backlight) {}

void BrightnessItem::previewOption(std::size_t index) {
    backlight_.setBrightnessLevel(optionValue(index));
}

void BrightnessItem::cancelPreview() {
    backlight_.setBrightnessLevel(storedValue());
}

bool BrightnessItem::showsLevel() const {
    return true;
}

}  // namespace m5_redux
