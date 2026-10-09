#pragma once

#include <cstddef>

#include "app/menu/SettingChoiceItem.h"
#include "app/screens/OssmControlScreen.h"
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

class DepthModeItem : public SettingChoiceItem<DepthControlOption, DepthControlMode> {
  public:
    DepthModeItem(SettingsStore& settings, OssmControlScreen& controlScreen);

  protected:
    void applied(DepthControlMode mode) override;

  private:
    OssmControlScreen& controlScreen_;
};

class StrokeDirectionItem : public SettingChoiceItem<StrokeDirectionOption, bool> {
  public:
    StrokeDirectionItem(SettingsStore& settings, OssmControlScreen& controlScreen);

  protected:
    void applied(bool reversed) override;

  private:
    OssmControlScreen& controlScreen_;
};

// Settings for controlling an OSSM. Disconnect and the default pattern only appear while one is
// connected.
class OssmSettingsSource : public MenuSource {
  public:
    OssmSettingsSource(SettingsStore& settings, OssmControlScreen& controlScreen);

    void setConnected(bool connected);
    // An empty catalog hides the default pattern.
    void setPatternCatalog(const ossm::OssmClient::PatternList& catalog);

    void addItems(MenuPage& page) override;

  private:
    MenuCommandItem disconnect_;
    DefaultPatternItem defaultPattern_;
    DepthModeItem depthMode_;
    StrokeDirectionItem strokeDirection_;
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
