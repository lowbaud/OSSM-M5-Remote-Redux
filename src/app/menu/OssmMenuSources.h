#pragma once

#include <cstddef>

#include "app/menu/SettingChoiceItem.h"
#include "devices/ossm/OssmClient.h"
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

// Action tiles while an OSSM is under control.
class OssmQuickActionsSource : public MenuSource {
  public:
    OssmQuickActionsSource();

    void addItems(MenuPage& page) override;

  private:
    MenuCommandItem pause_;
    MenuCommandItem pullOut_;
    // Switches the depth mode; the control screen shows which one is active.
    MenuCommandItem range_;
};

}  // namespace m5_redux
