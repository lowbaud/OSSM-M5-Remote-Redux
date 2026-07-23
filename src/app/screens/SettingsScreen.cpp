#include "SettingsScreen.h"

#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr char kBrightnessSettingName[] = "Display brightness";
constexpr char kIdleDimSettingName[] = "Dim after";
constexpr char kIdlePowerOffSettingName[] = "Power off after";
constexpr char kAutoConnectSettingName[] = "Auto-connect";
constexpr char kStrokeDirectionSettingName[] = "Stroke direction";
constexpr std::size_t kBrightnessSettingIndex = 0;
constexpr std::size_t kIdleDimSettingIndex = 1;
constexpr std::size_t kIdlePowerOffSettingIndex = 2;
constexpr std::size_t kAutoConnectSettingIndex = 3;
constexpr std::size_t kStrokeDirectionSettingIndex = 4;
constexpr std::int32_t kOptionsTitleHeight = 28;
constexpr std::int32_t kOptionsPanelWidth = 263;
constexpr std::int32_t kOptionsPanelHeight = 154;
constexpr std::int32_t kOptionsScrollbarWidth = 5;

void styleSelectableRow(lv_obj_t* row, lv_obj_t* list) {
    const lv_style_selector_t normal = LV_PART_MAIN | LV_STATE_DEFAULT;
    const lv_style_selector_t selected = LV_PART_MAIN | LV_STATE_CHECKED;

    lv_obj_set_style_bg_color(row, lv_obj_get_style_bg_color(list, LV_PART_MAIN), normal);
    lv_obj_set_style_bg_opa(row, lv_obj_get_style_bg_opa(list, LV_PART_MAIN), normal);
    lv_obj_set_style_bg_color(row, lv_theme_get_color_primary(row), selected);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, selected);
    lv_obj_set_style_text_color(row, lv_color_white(), selected);
}

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
    selectSetting(0);
    closeOptions();
    refresh();
}

void SettingsScreen::enter() {
    pendingEvent_ = {};
    selectSetting(0);
    closeOptions();
    refresh();

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
    if (!optionsOpen_) {
        openSelectedSetting();
        return;
    }

    if (selectedOptionIndex_ == kNoSelection) {
        return;
    }

    switch (selectedSettingIndex_) {
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
}

void SettingsScreen::buildSettingRows() {
    SettingRow& brightnessSetting = settingRows_[kBrightnessSettingIndex];
    brightnessSetting.button = lv_list_add_button(objects.settings_list, nullptr, nullptr);
    styleSelectableRow(brightnessSetting.button, objects.settings_list);

    lv_obj_t* brightnessNameLabel = lv_label_create(brightnessSetting.button);
    lv_label_set_text_static(brightnessNameLabel, kBrightnessSettingName);
    lv_obj_set_flex_grow(brightnessNameLabel, 1);

    brightnessSetting.valueLabel = lv_label_create(brightnessSetting.button);
    lv_label_set_text_static(brightnessSetting.valueLabel, "");

    lv_obj_add_event_cb(brightnessSetting.button, handleSettingRowEvent, LV_EVENT_CLICKED, this);

    SettingRow& idleDimSetting = settingRows_[kIdleDimSettingIndex];
    idleDimSetting.button = lv_list_add_button(objects.settings_list, nullptr, nullptr);
    styleSelectableRow(idleDimSetting.button, objects.settings_list);

    lv_obj_t* idleDimNameLabel = lv_label_create(idleDimSetting.button);
    lv_label_set_text_static(idleDimNameLabel, kIdleDimSettingName);
    lv_obj_set_flex_grow(idleDimNameLabel, 1);

    idleDimSetting.valueLabel = lv_label_create(idleDimSetting.button);
    lv_label_set_text_static(idleDimSetting.valueLabel, "");

    lv_obj_add_event_cb(idleDimSetting.button, handleSettingRowEvent, LV_EVENT_CLICKED, this);

    SettingRow& idlePowerOffSetting = settingRows_[kIdlePowerOffSettingIndex];
    idlePowerOffSetting.button = lv_list_add_button(objects.settings_list, nullptr, nullptr);
    styleSelectableRow(idlePowerOffSetting.button, objects.settings_list);

    lv_obj_t* idlePowerOffNameLabel = lv_label_create(idlePowerOffSetting.button);
    lv_label_set_text_static(idlePowerOffNameLabel, kIdlePowerOffSettingName);
    lv_obj_set_flex_grow(idlePowerOffNameLabel, 1);

    idlePowerOffSetting.valueLabel = lv_label_create(idlePowerOffSetting.button);
    lv_label_set_text_static(idlePowerOffSetting.valueLabel, "");

    lv_obj_add_event_cb(idlePowerOffSetting.button, handleSettingRowEvent, LV_EVENT_CLICKED, this);

    SettingRow& autoConnectSetting = settingRows_[kAutoConnectSettingIndex];
    autoConnectSetting.button = lv_list_add_button(objects.settings_list, nullptr, nullptr);
    styleSelectableRow(autoConnectSetting.button, objects.settings_list);

    lv_obj_t* autoConnectNameLabel = lv_label_create(autoConnectSetting.button);
    lv_label_set_text_static(autoConnectNameLabel, kAutoConnectSettingName);
    lv_obj_set_flex_grow(autoConnectNameLabel, 1);

    autoConnectSetting.valueLabel = lv_label_create(autoConnectSetting.button);
    lv_label_set_text_static(autoConnectSetting.valueLabel, "");

    lv_obj_add_event_cb(autoConnectSetting.button, handleSettingRowEvent, LV_EVENT_CLICKED, this);

    SettingRow& strokeDirectionSetting = settingRows_[kStrokeDirectionSettingIndex];
    strokeDirectionSetting.button = lv_list_add_button(objects.settings_list, nullptr, nullptr);
    styleSelectableRow(strokeDirectionSetting.button, objects.settings_list);

    lv_obj_t* strokeDirectionNameLabel = lv_label_create(strokeDirectionSetting.button);
    lv_label_set_text_static(strokeDirectionNameLabel, kStrokeDirectionSettingName);
    lv_obj_set_flex_grow(strokeDirectionNameLabel, 1);

    strokeDirectionSetting.valueLabel = lv_label_create(strokeDirectionSetting.button);
    lv_label_set_text_static(strokeDirectionSetting.valueLabel, "");

    lv_obj_add_event_cb(
        strokeDirectionSetting.button, handleSettingRowEvent, LV_EVENT_CLICKED, this);
}

void SettingsScreen::buildOptionsPanel() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN | LV_STATE_DEFAULT;

    lv_obj_remove_flag(objects.settings_options, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(objects.settings_options, 0, mainStyle);

    optionsTitle_ = lv_label_create(objects.settings_options);
    lv_obj_set_pos(optionsTitle_, 0, 0);
    lv_obj_set_size(optionsTitle_, kOptionsPanelWidth, kOptionsTitleHeight);
    lv_obj_set_style_text_align(optionsTitle_, LV_TEXT_ALIGN_CENTER, mainStyle);
    const lv_font_t* titleFont = lv_obj_get_style_text_font(optionsTitle_, LV_PART_MAIN);
    const std::int32_t titleTopPadding =
        (kOptionsTitleHeight - 2 - lv_font_get_line_height(titleFont)) / 2;
    lv_obj_set_style_pad_top(optionsTitle_, titleTopPadding, mainStyle);
    lv_obj_set_style_border_color(
        optionsTitle_,
        lv_obj_get_style_border_color(objects.settings_options, LV_PART_MAIN),
        mainStyle);
    lv_obj_set_style_border_opa(optionsTitle_, LV_OPA_50, mainStyle);
    lv_obj_set_style_border_width(optionsTitle_, 2, mainStyle);
    lv_obj_set_style_border_side(optionsTitle_, LV_BORDER_SIDE_BOTTOM, mainStyle);
    lv_label_set_text_static(optionsTitle_, "");

    optionsList_ = lv_list_create(objects.settings_options);
    lv_obj_set_pos(optionsList_, 0, kOptionsTitleHeight);
    lv_obj_set_size(optionsList_, kOptionsPanelWidth, kOptionsPanelHeight - kOptionsTitleHeight);
    lv_obj_set_style_bg_opa(optionsList_, LV_OPA_TRANSP, mainStyle);
    lv_obj_set_style_border_width(optionsList_, 0, mainStyle);
    lv_obj_set_style_outline_width(optionsList_, 0, mainStyle);
    lv_obj_set_style_pad_all(optionsList_, 0, mainStyle);
    lv_obj_set_style_pad_right(optionsList_, kOptionsScrollbarWidth + 1, mainStyle);
    lv_obj_set_style_pad_row(optionsList_, 0, mainStyle);
    lv_obj_set_scrollbar_mode(optionsList_, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(optionsList_, kOptionsScrollbarWidth, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(
        optionsList_, lv_theme_get_color_primary(optionsList_), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(optionsList_, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(optionsList_, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(optionsList_, 0, LV_PART_SCROLLBAR);

    for (std::size_t index = 0; index < optionRows_.size(); ++index) {
        OptionRow& option = optionRows_[index];
        option.button = lv_list_add_button(optionsList_, nullptr, nullptr);
        styleSelectableRow(option.button, optionsList_);
        lv_obj_set_style_pad_left(option.button, 10, mainStyle);
        lv_obj_set_style_pad_right(option.button, 10, mainStyle);
        lv_obj_set_style_pad_top(option.button, 4, mainStyle);
        lv_obj_set_style_pad_bottom(option.button, 4, mainStyle);
        lv_obj_set_style_pad_column(option.button, 4, mainStyle);

        option.checkLabel = lv_label_create(option.button);
        lv_obj_set_width(option.checkLabel, 18);
        lv_label_set_text_static(option.checkLabel, LV_SYMBOL_OK);
        lv_obj_set_style_text_opa(
            option.checkLabel, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);

        option.textLabel = lv_label_create(option.button);
        lv_label_set_text_static(option.textLabel, "");
        lv_obj_set_flex_grow(option.textLabel, 1);
        lv_obj_add_flag(option.button, LV_OBJ_FLAG_HIDDEN);

        lv_obj_add_event_cb(option.button, handleOptionRowEvent, LV_EVENT_CLICKED, this);
    }
}

void SettingsScreen::configureOptions() {
    switch (selectedSettingIndex_) {
        case kBrightnessSettingIndex:
            lv_label_set_text_static(optionsTitle_, kBrightnessSettingName);
            break;
        case kIdleDimSettingIndex:
            lv_label_set_text_static(optionsTitle_, kIdleDimSettingName);
            break;
        case kIdlePowerOffSettingIndex:
            lv_label_set_text_static(optionsTitle_, kIdlePowerOffSettingName);
            break;
        case kAutoConnectSettingIndex:
            lv_label_set_text_static(optionsTitle_, kAutoConnectSettingName);
            break;
        case kStrokeDirectionSettingIndex:
            lv_label_set_text_static(optionsTitle_, kStrokeDirectionSettingName);
            break;
        default:
            lv_label_set_text_static(optionsTitle_, "");
            break;
    }

    const std::size_t optionCount = currentOptionCount();

    for (std::size_t index = 0; index < optionRows_.size(); ++index) {
        if (index >= optionCount) {
            lv_obj_add_flag(optionRows_[index].button, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const char* name = "";
        switch (selectedSettingIndex_) {
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

void SettingsScreen::openSelectedSetting() {
    if (selectedSettingIndex_ >= kSettingCount) {
        return;
    }

    optionsOpen_ = true;
    configureOptions();
    lv_obj_remove_flag(objects.settings_options, LV_OBJ_FLAG_HIDDEN);
    selectOption(currentStoredOptionIndex(), false);
}

void SettingsScreen::closeOptions() {
    optionsOpen_ = false;
    selectedOptionIndex_ = kNoSelection;
    lv_obj_add_flag(objects.settings_options, LV_OBJ_FLAG_HIDDEN);
}

std::size_t SettingsScreen::currentOptionCount() const {
    switch (selectedSettingIndex_) {
        case kBrightnessSettingIndex:
            return SettingsStore::kBrightnessOptionCount;
        case kIdleDimSettingIndex:
            return SettingsStore::kIdleDimOptionCount;
        case kIdlePowerOffSettingIndex:
            return SettingsStore::kIdlePowerOffOptionCount;
        case kAutoConnectSettingIndex:
            return SettingsStore::kAutoConnectOptionCount;
        case kStrokeDirectionSettingIndex:
            return SettingsStore::kStrokeDirectionOptionCount;
        default:
            return 0;
    }
}

std::size_t SettingsScreen::currentStoredOptionIndex() const {
    switch (selectedSettingIndex_) {
        case kBrightnessSettingIndex:
            return SettingsStore::brightnessOptionIndex(settings_.brightnessLevel());
        case kIdleDimSettingIndex:
            return SettingsStore::idleDimOptionIndex(settings_.idleDimTimeout());
        case kIdlePowerOffSettingIndex:
            return SettingsStore::idlePowerOffOptionIndex(settings_.idlePowerOffTimeout());
        case kAutoConnectSettingIndex:
            return SettingsStore::autoConnectOptionIndex(settings_.autoConnectEnabled());
        case kStrokeDirectionSettingIndex:
            return SettingsStore::strokeDirectionOptionIndex(settings_.strokeEncoderReversed());
        default:
            return kNoSelection;
    }
}

void SettingsScreen::selectOption(std::size_t index, bool preview) {
    if (index >= currentOptionCount()) {
        return;
    }

    for (std::size_t rowIndex = 0; rowIndex < optionRows_.size(); ++rowIndex) {
        lv_obj_remove_state(optionRows_[rowIndex].button, LV_STATE_CHECKED);
        lv_obj_set_style_text_opa(
            optionRows_[rowIndex].checkLabel, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    selectedOptionIndex_ = index;
    lv_obj_add_state(optionRows_[index].button, LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(
        optionRows_[index].checkLabel, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_scroll_to_view(optionRows_[index].button, LV_ANIM_OFF);

    if (preview && selectedSettingIndex_ == kBrightnessSettingIndex) {
        pendingEvent_.action = SettingsScreenAction::PreviewBrightness;
        pendingEvent_.brightnessLevel = SettingsStore::brightnessOption(index).level;
    }
}

void SettingsScreen::selectSetting(std::size_t index) {
    if (index >= settingRows_.size()) {
        return;
    }

    if (selectedSettingIndex_ < settingRows_.size()) {
        lv_obj_remove_state(settingRows_[selectedSettingIndex_].button, LV_STATE_CHECKED);
    }

    selectedSettingIndex_ = index;
    lv_obj_add_state(settingRows_[selectedSettingIndex_].button, LV_STATE_CHECKED);
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
