#include "OssmMenuSources.h"

#include <lvgl.h>

#include <cstdio>
#include <string>

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

const SettingChoiceItem<RampIncreaseOption, RampIncrease>::Setting kRampIncreaseSetting = {
    SettingsStore::kRampIncreaseOptionCount,
    &SettingsStore::rampIncreaseOption,
    &SettingsStore::rampIncreaseOptionIndex,
    &RampIncreaseOption::increase,
    &SettingsStore::rampIncrease,
    &SettingsStore::setRampIncrease,
};

const SettingChoiceItem<RampDurationOption, RampDuration>::Setting kRampDurationSetting = {
    SettingsStore::kRampDurationOptionCount,
    &SettingsStore::rampDurationOption,
    &SettingsStore::rampDurationOptionIndex,
    &RampDurationOption::duration,
    &SettingsStore::rampDuration,
    &SettingsStore::setRampDuration,
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

OssmSettingsSource::OssmSettingsSource(SettingsStore& settings, ApplySettingsFn applySettings)
    : disconnect_(
          "ossm.disconnect",
          "Disconnect",
          static_cast<MenuCommand>(AppMenuCommand::Disconnect),
          MenuCommandItem::Style::Destructive),
      defaultPattern_(settings),
      depthMode_("ossm.depthmode", "Depth mode", settings, kDepthModeSetting, applySettings),
      strokeDirection_(
          "ossm.strokedir", "Stroke dir.", settings, kStrokeDirectionSetting, applySettings) {}

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

OssmActionTile::OssmActionTile(
    const char* id,
    const char* name,
    const char* icon,
    AppMenuCommand command,
    const OssmControl& control,
    const Running& running)
    : MenuCommandItem(id, name, static_cast<MenuCommand>(command)), control_(control),
      running_(running) {
    setIcon(icon);
}

const char* OssmActionTile::name() const {
    return running() ? running_.name : MenuCommandItem::name();
}

const char* OssmActionTile::icon() const {
    return running() ? running_.icon : MenuCommandItem::icon();
}

bool OssmActionTile::running() const {
    return control_.action() == running_.action;
}

OssmQuickActionsSource::OssmQuickActionsSource(
    SettingsStore& settings, const OssmControl& control, ApplySettingsFn applySettings)
    : pause_(
          "ossm.pause",
          "Pause",
          LV_SYMBOL_PAUSE,
          AppMenuCommand::Pause,
          control,
          {OssmAction::Paused, "Resume", LV_SYMBOL_PLAY}),
      pullOut_(
          "ossm.pullout",
          "Pull out",
          LV_SYMBOL_PREV,
          AppMenuCommand::PullOut,
          control,
          {OssmAction::PullingOut, "Pull out", LV_SYMBOL_PREV}),
      range_("ossm.range", "Range", static_cast<MenuCommand>(AppMenuCommand::SwitchDepthMode)),
      ramp_(
          "ossm.ramp",
          "Ramp",
          LV_SYMBOL_UP,
          AppMenuCommand::Ramp,
          control,
          {OssmAction::Ramping, "End ramp", LV_SYMBOL_UP}),
      rampIncrease_("ossm.rampby", "Ramp by", settings, kRampIncreaseSetting, applySettings),
      rampDuration_("ossm.ramptime", "Ramp time", settings, kRampDurationSetting, applySettings) {
    range_.setIcon(LV_SYMBOL_SHUFFLE);
}

std::array<MenuItem*, MenuPage::kMaxTiles> OssmQuickActionsSource::tiles() {
    return {&pause_, &pullOut_, &range_, &ramp_};
}

MenuItem* OssmQuickActionsSource::tile(const std::string& id) {
    for (MenuItem* tile : tiles()) {
        if (id == tile->id()) {
            return tile;
        }
    }
    return nullptr;
}

void OssmQuickActionsSource::addItems(MenuPage& page) {
    for (MenuItem* tile : tiles()) {
        page.addTile(*tile);
    }
    page.add(rampIncrease_);
    page.add(rampDuration_);
}

}  // namespace m5_redux
