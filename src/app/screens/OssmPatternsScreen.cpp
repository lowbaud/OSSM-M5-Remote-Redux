#include "OssmPatternsScreen.h"

#include <algorithm>
#include <cstring>

#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

void stylePatternRow(lv_obj_t* row, lv_obj_t* list) {
    const lv_style_selector_t normal = LV_PART_MAIN | LV_STATE_DEFAULT;
    const lv_style_selector_t selected = LV_PART_MAIN | LV_STATE_CHECKED;

    lv_obj_set_style_bg_color(row, lv_obj_get_style_bg_color(list, LV_PART_MAIN), normal);
    lv_obj_set_style_bg_opa(row, lv_obj_get_style_bg_opa(list, LV_PART_MAIN), normal);
    lv_obj_set_style_text_font(row, &lv_font_montserrat_16, normal);
    lv_obj_set_style_bg_color(row, lv_theme_get_color_primary(row), selected);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, selected);
    lv_obj_set_style_text_color(row, lv_color_white(), selected);
}

}  // namespace

OssmPatternsScreen::OssmPatternsScreen(OssmControl& control) : control_(control) {}

void OssmPatternsScreen::begin() {
    rows_.reserve(ossm::OssmClient::kMaxPatternCount);
    stopButtonFeedback_.begin(objects.oss_patterns_stop_btn);
    updateSelectButton();
    refresh();
}

void OssmPatternsScreen::setCatalog(const ossm::OssmClient::PatternList& catalog) {
    catalog_ = catalog;
    std::sort(
        catalog_.patterns,
        catalog_.patterns + catalog_.count,
        [](const ossm::OssmClient::PatternInfo& left, const ossm::OssmClient::PatternInfo& right) {
            return std::strcmp(left.name, right.name) < 0;
        });
    selectedIndex_ = kNoSelection;
    pendingAction_ = OssmPatternsScreenAction::None;
    rebuildRows();
}

void OssmPatternsScreen::invalidateCatalog() {
    catalog_ = {};
    selectedIndex_ = kNoSelection;
    pendingAction_ = OssmPatternsScreenAction::None;
    rebuildRows();
}

void OssmPatternsScreen::enter() {
    selectCurrentPattern();
    refresh();

    if (lv_screen_active() != objects.ossm_patterns) {
        loadScreen(SCREEN_ID_OSSM_PATTERNS);
    }
}

void OssmPatternsScreen::leave() {
    pendingAction_ = OssmPatternsScreenAction::None;
    stopButtonFeedback_.reset();
}

OssmPatternsScreenAction OssmPatternsScreen::update(const RemoteInputEvents& events) {
    if (!rows_.empty() && events.encoderSteps[3] != 0) {
        const std::int64_t current = selectedIndex_ == kNoSelection ? 0 : selectedIndex_;
        const std::int64_t last = static_cast<std::int64_t>(rows_.size() - 1);
        std::int64_t next = current + events.encoderSteps[3];
        if (next < 0) {
            next = 0;
        } else if (next > last) {
            next = last;
        }
        selectRow(static_cast<std::size_t>(next));
    }

    if (events.rightClick) {
        requestSelect();
    }
    if (events.leftClick) {
        requestCancel();
    }

    const OssmPatternsScreenAction action = pendingAction_;
    pendingAction_ = OssmPatternsScreenAction::None;
    return action;
}

void OssmPatternsScreen::refresh() {
    stopButtonFeedback_.setMotionActive(control_.values().speed > 0);
}

void OssmPatternsScreen::requestCancel() {
    pendingAction_ = OssmPatternsScreenAction::Cancel;
}

void OssmPatternsScreen::requestSelect() {
    if (selectedIndex_ != kNoSelection) {
        pendingAction_ = OssmPatternsScreenAction::Select;
    }
}

bool OssmPatternsScreen::selectedPatternId(int& patternId) const {
    if (selectedIndex_ == kNoSelection || selectedIndex_ >= catalog_.count) {
        return false;
    }

    patternId = catalog_.patterns[selectedIndex_].id;
    return true;
}

const char* OssmPatternsScreen::patternName(int patternId) const {
    for (std::size_t index = 0; index < catalog_.count; ++index) {
        if (catalog_.patterns[index].id == patternId) {
            return catalog_.patterns[index].name;
        }
    }

    return nullptr;
}

void OssmPatternsScreen::rebuildRows() {
    lv_obj_clean(objects.ossm_patterns_list);
    rows_.clear();
    selectedIndex_ = kNoSelection;

    for (std::size_t index = 0; index < catalog_.count; ++index) {
        lv_obj_t* row =
            lv_list_add_button(objects.ossm_patterns_list, nullptr, catalog_.patterns[index].name);
        stylePatternRow(row, objects.ossm_patterns_list);
        lv_obj_add_event_cb(row, handleRowEvent, LV_EVENT_CLICKED, this);
        rows_.push_back(row);
    }

    updateSelectButton();
}

void OssmPatternsScreen::selectCurrentPattern() {
    if (rows_.empty()) {
        selectedIndex_ = kNoSelection;
        updateSelectButton();
        return;
    }

    const int currentPattern = control_.values().pattern;
    for (std::size_t index = 0; index < catalog_.count; ++index) {
        if (catalog_.patterns[index].id == currentPattern) {
            selectRow(index);
            return;
        }
    }

    selectRow(0);
}

void OssmPatternsScreen::selectRow(std::size_t index) {
    if (index >= rows_.size()) {
        return;
    }

    if (selectedIndex_ != kNoSelection && selectedIndex_ < rows_.size()) {
        lv_obj_remove_state(rows_[selectedIndex_], LV_STATE_CHECKED);
    }

    selectedIndex_ = index;
    lv_obj_add_state(rows_[selectedIndex_], LV_STATE_CHECKED);
    lv_obj_scroll_to_view(rows_[selectedIndex_], LV_ANIM_ON);
    updateSelectButton();
}

void OssmPatternsScreen::updateSelectButton() {
    if (selectedIndex_ == kNoSelection) {
        lv_obj_add_state(objects.ossm_patterns_select_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(objects.ossm_patterns_select_btn, LV_STATE_DISABLED);
    }
}

void OssmPatternsScreen::handleRowClicked(lv_obj_t* row) {
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        if (rows_[index] == row) {
            selectRow(index);
            return;
        }
    }
}

void OssmPatternsScreen::handleRowEvent(lv_event_t* event) {
    auto* screen = static_cast<OssmPatternsScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_target(event));
    screen->handleRowClicked(row);
}

}  // namespace m5_redux
