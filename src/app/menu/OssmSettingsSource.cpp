#include "OssmSettingsSource.h"

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

OssmSettingsSource::OssmSettingsSource(SettingsStore& settings, OssmControlScreen& controlScreen)
    : disconnect_(
          "ossm.disconnect",
          "Disconnect",
          static_cast<MenuCommand>(AppMenuCommand::Disconnect),
          MenuCommandItem::Style::Destructive),
      defaultPattern_(settings), depthMode_(settings, controlScreen),
      strokeDirection_(settings, controlScreen) {}

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

OssmSettingsSource::DefaultPatternItem::DefaultPatternItem(SettingsStore& settings)
    : MenuChoiceItem("ossm.defaultpattern", "Default pattern"), settings_(settings) {}

void OssmSettingsSource::DefaultPatternItem::setCatalog(
    const ossm::OssmClient::PatternList& catalog) {
    catalog_ = catalog;
}

bool OssmSettingsSource::DefaultPatternItem::hasPatterns() const {
    return catalog_.count > 0;
}

std::size_t OssmSettingsSource::DefaultPatternItem::optionCount() const {
    return catalog_.count + 1;
}

const char* OssmSettingsSource::DefaultPatternItem::optionName(std::size_t index) const {
    return index == 0 ? kAutoPatternOptionName : catalog_.patterns[index - 1].name;
}

std::size_t OssmSettingsSource::DefaultPatternItem::storedOption() const {
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

bool OssmSettingsSource::DefaultPatternItem::commitOption(std::size_t index) {
    const std::string name = index == 0 ? std::string() : catalog_.patterns[index - 1].name;
    if (!settings_.setDefaultPattern(name)) {
        LOGE("menu", "Unable to save default pattern setting");
        return false;
    }
    return true;
}

const char* OssmSettingsSource::DefaultPatternItem::valueText() const {
    const std::size_t option = displayedOption();
    if (option < optionCount()) {
        return optionName(option);
    }
    std::snprintf(storedName_, sizeof(storedName_), "%s", settings_.defaultPattern().c_str());
    return storedName_;
}

OssmSettingsSource::DepthModeItem::DepthModeItem(
    SettingsStore& settings, OssmControlScreen& controlScreen)
    : SettingChoiceItem("ossm.depthmode", "Depth mode", settings, kDepthModeSetting),
      controlScreen_(controlScreen) {}

void OssmSettingsSource::DepthModeItem::applied(DepthControlMode mode) {
    controlScreen_.setDepthControlMode(mode);
}

OssmSettingsSource::StrokeDirectionItem::StrokeDirectionItem(
    SettingsStore& settings, OssmControlScreen& controlScreen)
    : SettingChoiceItem("ossm.strokedir", "Stroke dir.", settings, kStrokeDirectionSetting),
      controlScreen_(controlScreen) {}

void OssmSettingsSource::StrokeDirectionItem::applied(bool reversed) {
    controlScreen_.setStrokeEncoderReversed(reversed);
}

}  // namespace m5_redux
