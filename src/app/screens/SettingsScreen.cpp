#include "SettingsScreen.h"

#include <Arduino.h>

#include <algorithm>
#include <cctype>

#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr std::size_t kDefaultPatternSettingIndex = 0;
constexpr char kDefaultPatternSettingName[] = "Default pattern";
constexpr char kAutoPatternOptionName[] = "Auto";
constexpr char kBrightnessSettingName[] = "Brightness";
constexpr char kIdleDimSettingName[] = "Dim";
constexpr char kIdlePowerOffSettingName[] = "Power off";
constexpr char kAutoConnectSettingName[] = "Auto-connect";
constexpr char kDepthControlSettingName[] = "Depth mode";
constexpr char kStrokeDirectionSettingName[] = "Stroke dir.";
constexpr char kTouchscreenSettingName[] = "Touchscreen";
constexpr char kDiagnosticsSettingName[] = "Diagnostics";
constexpr std::size_t kBrightnessSettingIndex = 1;
constexpr std::size_t kIdleDimSettingIndex = 2;
constexpr std::size_t kIdlePowerOffSettingIndex = 3;
constexpr std::size_t kAutoConnectSettingIndex = 4;
constexpr std::size_t kDepthControlSettingIndex = 5;
constexpr std::size_t kStrokeDirectionSettingIndex = 6;
constexpr std::size_t kTouchscreenSettingIndex = 7;
// Opens the diagnostics screen instead of an options panel.
constexpr std::size_t kDiagnosticsSettingIndex = 8;
constexpr std::int32_t kOptionsTitleHeight = 28;
constexpr std::int32_t kOptionsTitlePadX = 12;
// The panel grows with the option count up to this many rows, then scrolls.
constexpr std::size_t kMaxVisibleOptions = 4;
constexpr std::uint32_t kSaveFailureDurationMs = 2000;

std::int64_t clampIndex(std::int64_t index, std::size_t count) {
    if (index < 0) {
        return 0;
    }

    const std::int64_t last = static_cast<std::int64_t>(count - 1);
    return index > last ? last : index;
}

}  // namespace

SettingsScreen::SettingsScreen(SettingsStore& settings) : settings_(settings) {}

void SettingsScreen::begin() {
    stopButtonFeedback_.begin(objects.settings_stop_btn);
    setStopAvailable(false);
    buildSettingRows();
    buildOptionsPanel();
    selectSetting(
        patternCatalog_.count > 0 ? kDefaultPatternSettingIndex : kBrightnessSettingIndex);
    closeOptions();
    refresh();
}

void SettingsScreen::setPatternCatalog(const ossm::OssmClient::PatternList& catalog) {
    patternCatalog_ = catalog;
}

std::size_t SettingsScreen::defaultPatternOptionIndex() const {
    if (settings_.defaultPattern().empty()) {
        return 0;
    }
    for (std::size_t index = 0; index < patternCatalog_.count; ++index) {
        if (settings_.matchesDefaultPattern(patternCatalog_.patterns[index].name)) {
            return index + 1;
        }
    }
    return kNoSelection;
}

void SettingsScreen::enter(bool keepSelection) {
    pendingEvent_ = {};
    closeOptions();
    refresh();
    if (keepSelection) {
        selectSetting(selectedSettingIndex_);
    } else {
        selectSetting(
            patternCatalog_.count > 0 ? kDefaultPatternSettingIndex : kBrightnessSettingIndex);
    }

    if (lv_screen_active() != objects.settings) {
        loadScreen(SCREEN_ID_SETTINGS);
    }
}

void SettingsScreen::leave() {
    pendingEvent_ = {};
    closeOptions();
    stopButtonFeedback_.reset();
}

SettingsScreenEvent SettingsScreen::update(const RemoteInputEvents& events) {
    if (!lv_obj_has_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN) &&
        millis() - saveFailureShownAtMs_ >= kSaveFailureDurationMs) {
        clearSaveFailure();
    }

    if (events.encoderSteps[3] != 0) {
        if (optionsOpen_) {
            const std::int64_t current =
                selectedOptionIndex_ == kNoSelection ? 0 : selectedOptionIndex_;
            const std::int64_t next =
                clampIndex(current + events.encoderSteps[3], currentOptionCount());
            if (static_cast<std::size_t>(next) != selectedOptionIndex_) {
                selectOption(static_cast<std::size_t>(next), true);
            }
        } else {
            const std::int64_t next = clampIndex(
                static_cast<std::int64_t>(selectedSettingIndex_) + events.encoderSteps[3],
                kSettingCount);
            if (static_cast<std::size_t>(next) != selectedSettingIndex_) {
                selectSetting(static_cast<std::size_t>(next));
            }
        }
    }

    if (events.rightClick) {
        requestSelect();
    }
    if (events.leftClick) {
        requestBack();
    }

    const SettingsScreenEvent event = pendingEvent_;
    pendingEvent_ = {};
    return event;
}

void SettingsScreen::refresh() {
    SettingRow& pattern = settingRows_[kDefaultPatternSettingIndex];
    if (patternCatalog_.count > 0) {
        lv_obj_remove_flag(pattern.button, LV_OBJ_FLAG_HIDDEN);
        const std::string& defaultPattern = settings_.defaultPattern();
        lv_label_set_text(
            pattern.valueLabel,
            defaultPattern.empty() ? kAutoPatternOptionName : defaultPattern.c_str());
    } else {
        lv_obj_add_flag(pattern.button, LV_OBJ_FLAG_HIDDEN);
    }

    const std::size_t brightnessIndex =
        SettingsStore::brightnessOptionIndex(settings_.brightnessLevel());
    lv_label_set_text_static(
        settingRows_[kBrightnessSettingIndex].valueLabel,
        SettingsStore::brightnessOption(brightnessIndex).name);

    const std::size_t idleDimIndex = SettingsStore::idleDimOptionIndex(settings_.idleDimTimeout());
    lv_label_set_text_static(
        settingRows_[kIdleDimSettingIndex].valueLabel,
        SettingsStore::idleDimOption(idleDimIndex).name);

    const std::size_t idlePowerOffIndex =
        SettingsStore::idlePowerOffOptionIndex(settings_.idlePowerOffTimeout());
    lv_label_set_text_static(
        settingRows_[kIdlePowerOffSettingIndex].valueLabel,
        SettingsStore::idlePowerOffOption(idlePowerOffIndex).name);

    const std::size_t autoConnectIndex =
        SettingsStore::autoConnectOptionIndex(settings_.autoConnectEnabled());
    lv_label_set_text_static(
        settingRows_[kAutoConnectSettingIndex].valueLabel,
        SettingsStore::autoConnectOption(autoConnectIndex).name);

    const std::size_t touchscreenIndex =
        SettingsStore::touchscreenOptionIndex(settings_.touchscreenEnabled());
    lv_label_set_text_static(
        settingRows_[kTouchscreenSettingIndex].valueLabel,
        SettingsStore::touchscreenOption(touchscreenIndex).name);

    const std::size_t depthControlIndex =
        SettingsStore::depthControlOptionIndex(settings_.depthControlMode());
    lv_label_set_text_static(
        settingRows_[kDepthControlSettingIndex].valueLabel,
        SettingsStore::depthControlOption(depthControlIndex).name);

    const std::size_t strokeDirectionIndex =
        SettingsStore::strokeDirectionOptionIndex(settings_.strokeEncoderReversed());
    lv_label_set_text_static(
        settingRows_[kStrokeDirectionSettingIndex].valueLabel,
        SettingsStore::strokeDirectionOption(strokeDirectionIndex).name);
}

void SettingsScreen::setStopAvailable(bool available) {
    stopAvailable_ = available;
    if (available) {
        lv_obj_remove_flag(objects.settings_stop_btn, LV_OBJ_FLAG_HIDDEN);
    } else {
        stopButtonFeedback_.reset();
        lv_obj_add_flag(objects.settings_stop_btn, LV_OBJ_FLAG_HIDDEN);
    }
}

void SettingsScreen::setMotionActive(bool active) {
    stopButtonFeedback_.setMotionActive(active);
}

void SettingsScreen::requestBack() {
    if (optionsOpen_) {
        closeOptions();
        if (selectedSettingIndex_ == kBrightnessSettingIndex) {
            pendingEvent_.action = SettingsScreenAction::RestoreBrightness;
            pendingEvent_.brightnessLevel = settings_.brightnessLevel();
        }
    } else {
        pendingEvent_.action = SettingsScreenAction::Back;
    }
}

void SettingsScreen::requestSelect() {
    clearSaveFailure();
    if (!optionsOpen_) {
        openSelectedSetting();
        return;
    }

    if (selectedOptionIndex_ == kNoSelection) {
        return;
    }

    switch (selectedSettingIndex_) {
        case kDefaultPatternSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitDefaultPattern;
            pendingEvent_.defaultPattern =
                selectedOptionIndex_ == 0 ? ""
                                          : patternCatalog_.patterns[selectedOptionIndex_ - 1].name;
            break;
        case kBrightnessSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitBrightness;
            pendingEvent_.brightnessLevel =
                SettingsStore::brightnessOption(selectedOptionIndex_).level;
            break;
        case kIdleDimSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitIdleDimTimeout;
            pendingEvent_.idleDimTimeout =
                SettingsStore::idleDimOption(selectedOptionIndex_).timeout;
            break;
        case kIdlePowerOffSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitIdlePowerOffTimeout;
            pendingEvent_.idlePowerOffTimeout =
                SettingsStore::idlePowerOffOption(selectedOptionIndex_).timeout;
            break;
        case kAutoConnectSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitAutoConnect;
            pendingEvent_.autoConnectEnabled =
                SettingsStore::autoConnectOption(selectedOptionIndex_).enabled;
            break;
        case kTouchscreenSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitTouchscreen;
            pendingEvent_.touchscreenEnabled =
                SettingsStore::touchscreenOption(selectedOptionIndex_).enabled;
            break;
        case kDepthControlSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitDepthControl;
            pendingEvent_.depthControlMode =
                SettingsStore::depthControlOption(selectedOptionIndex_).mode;
            break;
        case kStrokeDirectionSettingIndex:
            pendingEvent_.action = SettingsScreenAction::CommitStrokeDirection;
            pendingEvent_.strokeEncoderReversed =
                SettingsStore::strokeDirectionOption(selectedOptionIndex_).reversed;
            break;
        default:
            break;
    }
}

void SettingsScreen::commitSucceeded() {
    closeOptions();
    refresh();
}

void SettingsScreen::commitFailed() {
    selectOption(currentStoredOptionIndex(), false);
    if (optionsOpen_) {
        saveFailureShownAtMs_ = millis();
        lv_obj_remove_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
    }
}

void SettingsScreen::buildSettingRows() {
    styleList(objects.settings_list);

    addSettingRow(kDefaultPatternSettingIndex, kDefaultPatternSettingName, "");
    addSettingRow(kBrightnessSettingIndex, kBrightnessSettingName, "");
    addSettingRow(kIdleDimSettingIndex, kIdleDimSettingName, "");
    addSettingRow(kIdlePowerOffSettingIndex, kIdlePowerOffSettingName, "");
    addSettingRow(kAutoConnectSettingIndex, kAutoConnectSettingName, "");
    addSettingRow(kDepthControlSettingIndex, kDepthControlSettingName, "");
    addSettingRow(kStrokeDirectionSettingIndex, kStrokeDirectionSettingName, "");
    addSettingRow(kTouchscreenSettingIndex, kTouchscreenSettingName, "");
    addSettingRow(kDiagnosticsSettingIndex, kDiagnosticsSettingName, LV_SYMBOL_RIGHT);
}

void SettingsScreen::addSettingRow(std::size_t index, const char* name, const char* value) {
    SettingRow& row = settingRows_[index];
    row.button = lv_list_add_button(objects.settings_list, nullptr, nullptr);
    styleListRow(row.button);
    addListRowName(row.button, name);
    row.valueLabel = addListRowValue(row.button, value);
    lv_obj_set_style_text_color(row.valueLabel, themeColor(COLOR_ID_ACCENT), LV_STATE_CHECKED);
    lv_obj_add_event_cb(row.button, handleSettingRowEvent, LV_EVENT_CLICKED, this);
}

void SettingsScreen::buildOptionsPanel() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    lv_obj_remove_flag(objects.settings_options, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(objects.settings_options, 0, mainStyle);

    optionsTitle_ = lv_label_create(objects.settings_options);
    lv_obj_set_pos(optionsTitle_, 0, 0);
    lv_obj_set_size(optionsTitle_, lv_pct(100), kOptionsTitleHeight);
    // Uppercase label in the same style as the value captions on the control screen.
    lv_obj_set_style_text_font(optionsTitle_, &lv_font_montserrat_14, mainStyle);
    lv_obj_set_style_text_color(optionsTitle_, themeColor(COLOR_ID_TEXT_SECONDARY), mainStyle);
    lv_obj_set_style_pad_left(optionsTitle_, kOptionsTitlePadX, mainStyle);
    lv_obj_set_style_pad_right(optionsTitle_, kOptionsTitlePadX, mainStyle);
    const lv_font_t* titleFont = lv_obj_get_style_text_font(optionsTitle_, LV_PART_MAIN);
    const std::int32_t titleTopPadding =
        (kOptionsTitleHeight - 1 - lv_font_get_line_height(titleFont)) / 2;
    lv_obj_set_style_pad_top(optionsTitle_, titleTopPadding, mainStyle);
    lv_obj_set_style_border_color(optionsTitle_, themeColor(COLOR_ID_BORDER), mainStyle);
    lv_obj_set_style_border_opa(optionsTitle_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(optionsTitle_, 1, mainStyle);
    lv_obj_set_style_border_side(optionsTitle_, LV_BORDER_SIDE_BOTTOM, mainStyle);
    lv_label_set_text_static(optionsTitle_, "");

    optionsList_ = lv_list_create(objects.settings_options);
    lv_obj_set_pos(optionsList_, 0, kOptionsTitleHeight);
    lv_obj_set_width(optionsList_, lv_pct(100));
    lv_obj_set_style_bg_opa(optionsList_, LV_OPA_TRANSP, mainStyle);
    lv_obj_set_style_border_width(optionsList_, 0, mainStyle);
    lv_obj_set_style_outline_width(optionsList_, 0, mainStyle);
    lv_obj_set_style_pad_all(optionsList_, 0, mainStyle);
    // The panel clips its own rounded corners; rounding the list too would cut the rows.
    lv_obj_set_style_radius(optionsList_, 0, mainStyle);
    styleList(optionsList_);

    for (std::size_t index = 0; index < optionRows_.size(); ++index) {
        OptionRow& option = optionRows_[index];
        option.button = lv_list_add_button(optionsList_, nullptr, nullptr);
        styleListRow(option.button);
        option.textLabel = addListRowName(option.button, "");
        // Marks the stored value; the selected row shows where the cursor is.
        option.checkLabel = addListRowValue(option.button, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(option.checkLabel, themeColor(COLOR_ID_ACCENT), 0);
        lv_obj_add_flag(option.checkLabel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(option.button, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(option.button, handleOptionRowEvent, LV_EVENT_CLICKED, this);
    }

    // Overlay the choices without intercepting touch input or changing their layout.
    saveFailureLabel_ = lv_label_create(objects.settings_options);
    lv_label_set_text_static(saveFailureLabel_, "Couldn't save. Try again.");
    lv_obj_set_width(saveFailureLabel_, lv_pct(90));
    lv_obj_set_style_text_align(saveFailureLabel_, LV_TEXT_ALIGN_CENTER, mainStyle);
    lv_obj_set_style_bg_color(
        saveFailureLabel_,
        lv_obj_get_style_bg_color(objects.settings_options, LV_PART_MAIN),
        mainStyle);
    lv_obj_set_style_bg_opa(saveFailureLabel_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_pad_all(saveFailureLabel_, 8, mainStyle);
    lv_obj_set_style_radius(saveFailureLabel_, 6, mainStyle);
    lv_obj_set_style_border_width(saveFailureLabel_, 1, mainStyle);
    lv_obj_set_style_border_color(saveFailureLabel_, lv_palette_main(LV_PALETTE_RED), mainStyle);
    lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_FLOATING);
    lv_obj_remove_flag(saveFailureLabel_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(saveFailureLabel_);
    lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::configureOptions() {
    const char* title = "";
    switch (selectedSettingIndex_) {
        case kDefaultPatternSettingIndex:
            title = kDefaultPatternSettingName;
            break;
        case kBrightnessSettingIndex:
            title = kBrightnessSettingName;
            break;
        case kIdleDimSettingIndex:
            title = kIdleDimSettingName;
            break;
        case kIdlePowerOffSettingIndex:
            title = kIdlePowerOffSettingName;
            break;
        case kAutoConnectSettingIndex:
            title = kAutoConnectSettingName;
            break;
        case kTouchscreenSettingIndex:
            title = kTouchscreenSettingName;
            break;
        case kDepthControlSettingIndex:
            title = kDepthControlSettingName;
            break;
        case kStrokeDirectionSettingIndex:
            title = kStrokeDirectionSettingName;
            break;
        default:
            break;
    }
    setOptionsTitle(title);

    const std::size_t optionCount = currentOptionCount();

    for (std::size_t index = 0; index < optionRows_.size(); ++index) {
        if (index >= optionCount) {
            lv_obj_add_flag(optionRows_[index].button, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const char* name = "";
        switch (selectedSettingIndex_) {
            case kDefaultPatternSettingIndex:
                name =
                    index == 0 ? kAutoPatternOptionName : patternCatalog_.patterns[index - 1].name;
                break;
            case kBrightnessSettingIndex:
                name = SettingsStore::brightnessOption(index).name;
                break;
            case kIdleDimSettingIndex:
                name = SettingsStore::idleDimOption(index).name;
                break;
            case kIdlePowerOffSettingIndex:
                name = SettingsStore::idlePowerOffOption(index).name;
                break;
            case kAutoConnectSettingIndex:
                name = SettingsStore::autoConnectOption(index).name;
                break;
            case kTouchscreenSettingIndex:
                name = SettingsStore::touchscreenOption(index).name;
                break;
            case kDepthControlSettingIndex:
                name = SettingsStore::depthControlOption(index).name;
                break;
            case kStrokeDirectionSettingIndex:
                name = SettingsStore::strokeDirectionOption(index).name;
                break;
            default:
                break;
        }
        lv_label_set_text_static(optionRows_[index].textLabel, name);
        lv_obj_remove_flag(optionRows_[index].button, LV_OBJ_FLAG_HIDDEN);
    }
}

void SettingsScreen::setOptionsTitle(const char* title) {
    char upperTitle[32];
    std::size_t length = 0;
    for (; title[length] != '\0' && length < sizeof(upperTitle) - 1; ++length) {
        upperTitle[length] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(title[length])));
    }
    upperTitle[length] = '\0';
    lv_label_set_text(optionsTitle_, upperTitle);
}

void SettingsScreen::openSelectedSetting() {
    if (selectedSettingIndex_ >= kSettingCount) {
        return;
    }

    if (selectedSettingIndex_ == kDiagnosticsSettingIndex) {
        pendingEvent_.action = SettingsScreenAction::ShowDiagnostics;
        return;
    }

    optionsOpen_ = true;
    configureOptions();
    resizeOptionsPanel();
    lv_obj_remove_flag(objects.settings_options_scrim, LV_OBJ_FLAG_HIDDEN);
    selectOption(currentStoredOptionIndex(), false);
}

void SettingsScreen::resizeOptionsPanel() {
    const std::size_t visibleRows = std::min(currentOptionCount(), kMaxVisibleOptions);
    const std::int32_t listHeight = static_cast<std::int32_t>(visibleRows) * listRowHeight();
    const std::int32_t borderWidth =
        lv_obj_get_style_border_width(objects.settings_options, LV_PART_MAIN);
    lv_obj_set_height(optionsList_, listHeight);
    lv_obj_set_height(objects.settings_options, kOptionsTitleHeight + listHeight + 2 * borderWidth);
}

void SettingsScreen::closeOptions() {
    clearSaveFailure();
    optionsOpen_ = false;
    selectedOptionIndex_ = kNoSelection;
    lv_obj_add_flag(objects.settings_options_scrim, LV_OBJ_FLAG_HIDDEN);
}

void SettingsScreen::clearSaveFailure() {
    if (saveFailureLabel_) {
        lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
    }
}

std::size_t SettingsScreen::currentOptionCount() const {
    switch (selectedSettingIndex_) {
        case kDefaultPatternSettingIndex:
            return patternCatalog_.count + 1;
        case kBrightnessSettingIndex:
            return SettingsStore::kBrightnessOptionCount;
        case kIdleDimSettingIndex:
            return SettingsStore::kIdleDimOptionCount;
        case kIdlePowerOffSettingIndex:
            return SettingsStore::kIdlePowerOffOptionCount;
        case kAutoConnectSettingIndex:
            return SettingsStore::kAutoConnectOptionCount;
        case kTouchscreenSettingIndex:
            return SettingsStore::kTouchscreenOptionCount;
        case kDepthControlSettingIndex:
            return SettingsStore::kDepthControlOptionCount;
        case kStrokeDirectionSettingIndex:
            return SettingsStore::kStrokeDirectionOptionCount;
        default:
            return 0;
    }
}

std::size_t SettingsScreen::currentStoredOptionIndex() const {
    switch (selectedSettingIndex_) {
        case kDefaultPatternSettingIndex:
            return defaultPatternOptionIndex();
        case kBrightnessSettingIndex:
            return SettingsStore::brightnessOptionIndex(settings_.brightnessLevel());
        case kIdleDimSettingIndex:
            return SettingsStore::idleDimOptionIndex(settings_.idleDimTimeout());
        case kIdlePowerOffSettingIndex:
            return SettingsStore::idlePowerOffOptionIndex(settings_.idlePowerOffTimeout());
        case kAutoConnectSettingIndex:
            return SettingsStore::autoConnectOptionIndex(settings_.autoConnectEnabled());
        case kTouchscreenSettingIndex:
            return SettingsStore::touchscreenOptionIndex(settings_.touchscreenEnabled());
        case kDepthControlSettingIndex:
            return SettingsStore::depthControlOptionIndex(settings_.depthControlMode());
        case kStrokeDirectionSettingIndex:
            return SettingsStore::strokeDirectionOptionIndex(settings_.strokeEncoderReversed());
        default:
            return kNoSelection;
    }
}

void SettingsScreen::selectOption(std::size_t index, bool preview) {
    if (index != selectedOptionIndex_) {
        clearSaveFailure();
    }
    const std::size_t storedIndex = currentStoredOptionIndex();
    for (std::size_t rowIndex = 0; rowIndex < optionRows_.size(); ++rowIndex) {
        setListRowSelected(optionRows_[rowIndex].button, false);
        if (rowIndex == storedIndex) {
            lv_obj_remove_flag(optionRows_[rowIndex].checkLabel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(optionRows_[rowIndex].checkLabel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    selectedOptionIndex_ = kNoSelection;
    if (index >= currentOptionCount()) {
        return;
    }
    selectedOptionIndex_ = index;
    setListRowSelected(optionRows_[index].button, true);
    lv_obj_scroll_to_view(optionRows_[index].button, LV_ANIM_OFF);

    if (preview && selectedSettingIndex_ == kBrightnessSettingIndex) {
        pendingEvent_.action = SettingsScreenAction::PreviewBrightness;
        pendingEvent_.brightnessLevel = SettingsStore::brightnessOption(index).level;
    }
}

void SettingsScreen::selectSetting(std::size_t index) {
    if (index == kDefaultPatternSettingIndex && patternCatalog_.count == 0) {
        index = kBrightnessSettingIndex;
    }
    if (index >= settingRows_.size()) {
        return;
    }

    if (selectedSettingIndex_ < settingRows_.size()) {
        setListRowSelected(settingRows_[selectedSettingIndex_].button, false);
    }

    selectedSettingIndex_ = index;
    setListRowSelected(settingRows_[selectedSettingIndex_].button, true);
    lv_obj_scroll_to_view(settingRows_[selectedSettingIndex_].button, LV_ANIM_OFF);
    lv_obj_remove_state(objects.settings_select_btn, LV_STATE_DISABLED);
}

void SettingsScreen::handleSettingClicked(lv_obj_t* row) {
    if (optionsOpen_) {
        return;
    }

    for (std::size_t index = 0; index < settingRows_.size(); ++index) {
        if (settingRows_[index].button == row) {
            selectSetting(index);
            openSelectedSetting();
            return;
        }
    }
}

void SettingsScreen::handleOptionClicked(lv_obj_t* row) {
    for (std::size_t index = 0; index < currentOptionCount(); ++index) {
        if (optionRows_[index].button == row) {
            selectOption(index, false);
            requestSelect();
            return;
        }
    }
}

void SettingsScreen::handleSettingRowEvent(lv_event_t* event) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_target(event));
    screen->handleSettingClicked(row);
}

void SettingsScreen::handleOptionRowEvent(lv_event_t* event) {
    auto* screen = static_cast<SettingsScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_target(event));
    screen->handleOptionClicked(row);
}

}  // namespace m5_redux
