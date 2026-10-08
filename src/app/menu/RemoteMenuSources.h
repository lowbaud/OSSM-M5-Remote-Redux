#pragma once

#include <cstddef>

#include "app/AutoPowerOffController.h"
#include "app/BacklightController.h"
#include "app/IdleTimer.h"
#include "app/menu/SettingChoiceItem.h"
#include "menu/MenuItems.h"
#include "menu/MenuPage.h"
#include "settings/SettingsStore.h"

namespace m5_redux {

// Previews each option on the backlight while browsing.
class BrightnessItem : public SettingChoiceItem<BrightnessOption, BrightnessLevel> {
  public:
    BrightnessItem(SettingsStore& settings, BacklightController& backlight);

    void previewOption(std::size_t index) override;
    void cancelPreview() override;
    bool showsLevel() const override;

  protected:
    void applied(BrightnessLevel level) override;

  private:
    BacklightController& backlight_;
};

class IdleDimItem : public SettingChoiceItem<IdleDimOption, IdleDimTimeout> {
  public:
    IdleDimItem(SettingsStore& settings, BacklightController& backlight, IdleTimer& idleTimer);

  protected:
    void applied(IdleDimTimeout timeout) override;

  private:
    BacklightController& backlight_;
    IdleTimer& idleTimer_;
};

class IdlePowerOffItem : public SettingChoiceItem<IdlePowerOffOption, IdlePowerOffTimeout> {
  public:
    IdlePowerOffItem(
        SettingsStore& settings, AutoPowerOffController& autoPowerOff, IdleTimer& idleTimer);

  protected:
    void applied(IdlePowerOffTimeout timeout) override;

  private:
    AutoPowerOffController& autoPowerOff_;
    IdleTimer& idleTimer_;
};

class TouchscreenItem : public SettingChoiceItem<TouchscreenOption, bool> {
  public:
    explicit TouchscreenItem(SettingsStore& settings);

  protected:
    void applied(bool enabled) override;
};

// Settings of the remote itself, independent of the connected device.
class RemoteSettingsSource : public MenuSource {
  public:
    RemoteSettingsSource(
        SettingsStore& settings,
        BacklightController& backlight,
        AutoPowerOffController& autoPowerOff,
        IdleTimer& idleTimer);

    void addItems(MenuPage& page) override;

  private:
    BrightnessItem brightness_;
    IdleDimItem idleDim_;
    IdlePowerOffItem idlePowerOff_;
    SettingChoiceItem<AutoConnectOption, bool> autoConnect_;
    TouchscreenItem touchscreen_;
    MenuCommandItem diagnostics_;
};

// Quick actions of the remote itself. Settings comes last, below every other quick action.
class RemoteQuickActionsSource : public MenuSource {
  public:
    RemoteQuickActionsSource(
        SettingsStore& settings, BacklightController& backlight, MenuPage& settingsPage);

    void addItems(MenuPage& page) override;

  private:
    BrightnessItem brightness_;
    MenuLinkItem settings_;
};

}  // namespace m5_redux
