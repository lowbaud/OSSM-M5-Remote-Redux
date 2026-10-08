#include "RemoteSettingsSource.h"

#include <Arduino.h>

#include <cstdint>

#include "app/menu/AppMenuCommands.h"
#include "platform/LvglPort.h"

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
    SettingsStore& settings,
    BacklightController& backlight,
    AutoPowerOffController& autoPowerOff,
    IdleTimer& idleTimer)
    : brightness_(settings, backlight), idleDim_(settings, backlight, idleTimer),
      idlePowerOff_(settings, autoPowerOff, idleTimer),
      autoConnect_("remote.autoconnect", "Auto-connect", settings, kAutoConnectSetting),
      touchscreen_(settings), diagnostics_(
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

RemoteSettingsSource::BrightnessItem::BrightnessItem(
    SettingsStore& settings, BacklightController& backlight)
    : SettingChoiceItem("remote.brightness", "Brightness", settings, kBrightnessSetting),
      backlight_(backlight) {}

void RemoteSettingsSource::BrightnessItem::previewOption(std::size_t index) {
    backlight_.setBrightnessLevel(optionValue(index));
}

void RemoteSettingsSource::BrightnessItem::cancelPreview() {
    backlight_.setBrightnessLevel(storedValue());
}

bool RemoteSettingsSource::BrightnessItem::showsLevel() const {
    return true;
}

void RemoteSettingsSource::BrightnessItem::applied(BrightnessLevel level) {
    backlight_.setBrightnessLevel(level);
}

RemoteSettingsSource::IdleDimItem::IdleDimItem(
    SettingsStore& settings, BacklightController& backlight, IdleTimer& idleTimer)
    : SettingChoiceItem("remote.dim", "Dim", settings, kIdleDimSetting), backlight_(backlight),
      idleTimer_(idleTimer) {}

void RemoteSettingsSource::IdleDimItem::applied(IdleDimTimeout timeout) {
    backlight_.setIdleDimTimeoutSeconds(static_cast<std::uint32_t>(timeout));
    idleTimer_.reset(millis());
}

RemoteSettingsSource::IdlePowerOffItem::IdlePowerOffItem(
    SettingsStore& settings, AutoPowerOffController& autoPowerOff, IdleTimer& idleTimer)
    : SettingChoiceItem("remote.poweroff", "Power off", settings, kIdlePowerOffSetting),
      autoPowerOff_(autoPowerOff), idleTimer_(idleTimer) {}

void RemoteSettingsSource::IdlePowerOffItem::applied(IdlePowerOffTimeout timeout) {
    autoPowerOff_.setTimeoutSeconds(static_cast<std::uint32_t>(timeout));
    idleTimer_.reset(millis());
}

RemoteSettingsSource::TouchscreenItem::TouchscreenItem(SettingsStore& settings)
    : SettingChoiceItem("remote.touchscreen", "Touchscreen", settings, kTouchscreenSetting) {}

void RemoteSettingsSource::TouchscreenItem::applied(bool enabled) {
    lvgl_port::setTouchscreenEnabled(enabled);
}

}  // namespace m5_redux
