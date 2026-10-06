#include "OssmPatternsScreen.h"

#include <algorithm>
#include <cstring>

#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

OssmPatternsScreen::OssmPatternsScreen(OssmControl& control) : control_(control) {}

void OssmPatternsScreen::begin() {
    rows_.reserve(ossm::OssmClient::kMaxPatternCount);
    styleList(objects.ossm_patterns_list);
    stopButtonFeedback_.begin(objects.ossm_patterns_stop_btn);
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
    markCurrentPattern();
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
    const std::int32_t steps = events.menuSteps();
    if (!rows_.empty() && steps != 0) {
        const std::int64_t current = selectedIndex_ == kNoSelection ? 0 : selectedIndex_;
        const std::int64_t last = static_cast<std::int64_t>(rows_.size() - 1);
        std::int64_t next = current + steps;
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
        Row row;
        row.button = lv_list_add_button(objects.ossm_patterns_list, nullptr, nullptr);
        styleListRow(row.button);
        addListRowName(row.button, catalog_.patterns[index].name);
        row.currentMark = addListRowValue(row.button, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(row.currentMark, themeColor(COLOR_ID_ACCENT), 0);
        lv_obj_add_flag(row.currentMark, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(row.button, handleRowEvent, LV_EVENT_CLICKED, this);
        rows_.push_back(row);
    }

    markCurrentPattern();

    updateSelectButton();
}

void OssmPatternsScreen::markCurrentPattern() {
    const int currentPattern = control_.values().pattern;
    for (std::size_t index = 0; index < rows_.size(); ++index) {
        if (catalog_.patterns[index].id == currentPattern) {
            lv_obj_remove_flag(rows_[index].currentMark, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(rows_[index].currentMark, LV_OBJ_FLAG_HIDDEN);
        }
    }
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
        setListRowSelected(rows_[selectedIndex_].button, false);
    }

    selectedIndex_ = index;
    setListRowSelected(rows_[selectedIndex_].button, true);
    lv_obj_scroll_to_view(rows_[selectedIndex_].button, LV_ANIM_ON);
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
        if (rows_[index].button == row) {
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
