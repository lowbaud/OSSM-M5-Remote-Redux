#include "OssmMenuSources.h"

#include <lvgl.h>

#include <cstdio>
#include <string>

#include "app/menu/AppMenuCommands.h"
#include "diagnostics/Log.h"

namespace m5_redux {

namespace {

constexpr char kAutoPatternOptionName[] = "Auto";

const SettingChoiceItem<DepthControlOption, DepthControlMode>::Setting kDepthModeSetting = {
    SettingsStore::kDepthControlOptionCount,
    &SettingsStore::depthControlOption,
    &SettingsStore::depthControlOptionIndex,
    &DepthControlOption::mode,
    &SettingsStore::depthControlMode,
    &SettingsStore::setDepthControlMode,
};

const SettingChoiceItem<StrokeDirectionOption, bool>::Setting kStrokeDirectionSetting = {
    SettingsStore::kStrokeDirectionOptionCount,
    &SettingsStore::strokeDirectionOption,
    &SettingsStore::strokeDirectionOptionIndex,
    &StrokeDirectionOption::reversed,
    &SettingsStore::strokeEncoderReversed,
    &SettingsStore::setStrokeEncoderReversed,
};

}  // namespace

DefaultPatternItem::DefaultPatternItem(SettingsStore& settings)
    : MenuChoiceItem("ossm.defaultpattern", "Default pattern"), settings_(settings) {}

void DefaultPatternItem::setCatalog(const ossm::OssmClient::PatternList& catalog) {
    catalog_ = catalog;
}

bool DefaultPatternItem::hasPatterns() const {
    return catalog_.count > 0;
}

std::size_t DefaultPatternItem::optionCount() const {
    return catalog_.count + 1;
}

const char* DefaultPatternItem::optionName(std::size_t index) const {
    return index == 0 ? kAutoPatternOptionName : catalog_.patterns[index - 1].name;
}

std::size_t DefaultPatternItem::storedOption() const {
    if (settings_.defaultPattern().empty()) {
        return 0;
    }
    for (std::size_t index = 0; index < catalog_.count; ++index) {
        if (settings_.matchesDefaultPattern(catalog_.patterns[index].name)) {
            return index + 1;
        }
    }
    return kNoOption;
}

bool DefaultPatternItem::commitOption(std::size_t index) {
    const std::string name = index == 0 ? std::string() : catalog_.patterns[index - 1].name;
    if (!settings_.setDefaultPattern(name)) {
        LOGE("menu", "Unable to save default pattern setting");
        return false;
    }
    return true;
}

const char* DefaultPatternItem::valueText() const {
    const std::size_t option = displayedOption();
    if (option < optionCount()) {
        return optionName(option);
    }
    std::snprintf(storedName_, sizeof(storedName_), "%s", settings_.defaultPattern().c_str());
    return storedName_;
}

DepthModeItem::DepthModeItem(
    SettingsStore& settings, OssmControlScreen& controlScreen, SelectAction selectAction)
    : SettingChoiceItem("ossm.depthmode", "Depth mode", settings, kDepthModeSetting, selectAction),
      controlScreen_(controlScreen) {
    setShortName("Depth");
    setIcon(LV_SYMBOL_BARS);
}

void DepthModeItem::applied(DepthControlMode mode) {
    controlScreen_.setDepthControlMode(mode);
}

StrokeDirectionItem::StrokeDirectionItem(
    SettingsStore& settings, OssmControlScreen& controlScreen, SelectAction selectAction)
    : SettingChoiceItem(
          "ossm.strokedir", "Stroke dir.", settings, kStrokeDirectionSetting, selectAction),
      controlScreen_(controlScreen) {
    setShortName("Stroke");
    setIcon(LV_SYMBOL_LOOP);
}

void StrokeDirectionItem::applied(bool reversed) {
    controlScreen_.setStrokeEncoderReversed(reversed);
}

OssmSettingsSource::OssmSettingsSource(SettingsStore& settings, OssmControlScreen& controlScreen)
    : disconnect_(
          "ossm.disconnect",
          "Disconnect",
          static_cast<MenuCommand>(AppMenuCommand::Disconnect),
          MenuCommandItem::Style::Destructive),
      defaultPattern_(settings),
      depthMode_(settings, controlScreen, MenuChoiceItem::SelectAction::OpenOptions),
      strokeDirection_(settings, controlScreen, MenuChoiceItem::SelectAction::OpenOptions) {}

void OssmSettingsSource::setConnected(bool connected) {
    connected_ = connected;
}

void OssmSettingsSource::setPatternCatalog(const ossm::OssmClient::PatternList& catalog) {
    defaultPattern_.setCatalog(catalog);
}

void OssmSettingsSource::addItems(MenuPage& page) {
    if (connected_) {
        page.add(disconnect_);
    }
    if (defaultPattern_.hasPatterns()) {
        page.add(defaultPattern_);
    }
    page.add(depthMode_);
    page.add(strokeDirection_);
}

OssmQuickActionsSource::OssmQuickActionsSource(
    SettingsStore& settings, OssmControlScreen& controlScreen)
    : depthMode_(settings, controlScreen, MenuChoiceItem::SelectAction::NextOption),
      strokeDirection_(settings, controlScreen, MenuChoiceItem::SelectAction::NextOption) {}

void OssmQuickActionsSource::addItems(MenuPage& page) {
    page.add(depthMode_);
    page.add(strokeDirection_);
}

}  // namespace m5_redux
