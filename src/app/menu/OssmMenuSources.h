#pragma once

#include <array>
#include <cstddef>
#include <string>

#include "app/menu/AppMenuCommands.h"
#include "app/menu/SettingChoiceItem.h"
#include "devices/ossm/OssmClient.h"
#include "devices/ossm/OssmControl.h"
#include "menu/MenuItems.h"
#include "menu/MenuPage.h"
#include "settings/SettingsStore.h"

namespace m5_redux {

// Auto, followed by the patterns of the connected OSSM. The preference is stored by name, so a
// pattern this firmware does not offer stays selected and shows by name.
class DefaultPatternItem : public MenuChoiceItem {
  public:
    explicit DefaultPatternItem(SettingsStore& settings);

    void setCatalog(const ossm::OssmClient::PatternList& catalog);
    bool hasPatterns() const;

    std::size_t optionCount() const override;
    const char* optionName(std::size_t index) const override;
    std::size_t storedOption() const override;
    bool commitOption(std::size_t index) override;
    const char* valueText() const override;

  private:
    SettingsStore& settings_;
    ossm::OssmClient::PatternList catalog_{};
    mutable char storedName_[ossm::OssmClient::kPatternNameCapacity] = {};
};

// Settings for controlling an OSSM. Disconnect and the default pattern only appear while one is
// connected.
class OssmSettingsSource : public MenuSource {
  public:
    OssmSettingsSource(SettingsStore& settings, ApplySettingsFn applySettings);

    void setConnected(bool connected);
    // An empty catalog hides the default pattern.
    void setPatternCatalog(const ossm::OssmClient::PatternList& catalog);

    void addItems(MenuPage& page) override;

  private:
    MenuCommandItem disconnect_;
    DefaultPatternItem defaultPattern_;
    SettingChoiceItem<DepthControlOption, DepthControlMode> depthMode_;
    SettingChoiceItem<StrokeDirectionOption, bool> strokeDirection_;
    bool connected_ = false;
};

// An action tile that pulses while its action runs and then reads as what selecting it again
// does: Pause reads Resume while paused.
class OssmActionTile : public MenuCommandItem {
  public:
    struct Running {
        OssmAction action;
        const char* name;
        const char* icon;
    };

    OssmActionTile(
        const char* id,
        const char* name,
        const char* icon,
        AppMenuCommand command,
        const OssmControl& control,
        const Running& running);

    const char* name() const override;
    const char* icon() const override;
    bool running() const override;

  private:
    const OssmControl& control_;
    Running running_;
};

// Action tiles while an OSSM is under control, and the settings of the Ramp tile.
class OssmQuickActionsSource : public MenuSource {
  public:
    OssmQuickActionsSource(
        SettingsStore& settings, const OssmControl& control, ApplySettingsFn applySettings);

    // The tile with the menu item id, e.g. one stored for a held button, or nullptr.
    MenuItem* tile(const std::string& id);

    void addItems(MenuPage& page) override;

  private:
    OssmActionTile pause_;
    OssmActionTile pullOut_;
    // Switches the depth mode; the control screen shows which one is active.
    MenuCommandItem range_;
    OssmActionTile ramp_;
    SettingChoiceItem<RampIncreaseOption, RampIncrease> rampIncrease_;
    SettingChoiceItem<RampDurationOption, RampDuration> rampDuration_;

    std::array<MenuItem*, MenuPage::kMaxTiles> tiles();
};

}  // namespace m5_redux
