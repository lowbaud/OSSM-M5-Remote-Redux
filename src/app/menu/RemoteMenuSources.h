#pragma once

#include <cstddef>

#include "app/BacklightController.h"
#include "app/menu/SettingChoiceItem.h"
#include "menu/MenuItems.h"
#include "menu/MenuPage.h"
#include "settings/SettingsStore.h"

namespace m5_redux {

// Previews each option on the backlight while browsing.
class BrightnessItem : public SettingChoiceItem<BrightnessOption, BrightnessLevel> {
  public:
    BrightnessItem(
        SettingsStore& settings, BacklightController& backlight, ApplySettingsFn applySettings);

    void previewOption(std::size_t index) override;
    void cancelPreview() override;
    bool showsLevel() const override;

  private:
    BacklightController& backlight_;
};

// Settings of the remote itself, independent of the connected device.
class RemoteSettingsSource : public MenuSource {
  public:
    RemoteSettingsSource(
        SettingsStore& settings, BacklightController& backlight, ApplySettingsFn applySettings);

    // Also shown in the quick actions.
    MenuItem& brightness();

    void addItems(MenuPage& page) override;

  private:
    BrightnessItem brightness_;
    SettingChoiceItem<IdleDimOption, IdleDimTimeout> idleDim_;
    SettingChoiceItem<IdlePowerOffOption, IdlePowerOffTimeout> idlePowerOff_;
    SettingChoiceItem<AutoConnectOption, bool> autoConnect_;
    SettingChoiceItem<TouchscreenOption, bool> touchscreen_;
    MenuCommandItem diagnostics_;
};

// Quick actions of the remote itself: Brightness from the settings, and the link to the
// settings, which comes last below every other quick action.
class RemoteQuickActionsSource : public MenuSource {
  public:
    RemoteQuickActionsSource(RemoteSettingsSource& settings, MenuPage& settingsPage);

    void addItems(MenuPage& page) override;

  private:
    RemoteSettingsSource& settings_;
    MenuLinkItem settingsLink_;
};

}  // namespace m5_redux
