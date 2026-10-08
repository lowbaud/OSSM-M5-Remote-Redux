#include "MenuPanel.h"

#include <Arduino.h>

#include <algorithm>
#include <cctype>

#include "diagnostics/Log.h"
#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"
#include "ui/generated/actions.h"
#include "ui/generated/screens.h"
#include "ui/generated/styles.h"

namespace m5_redux {

namespace {

constexpr const char* kTag = "menu";
constexpr std::int32_t kPanelWidth = 250;
// Space between the bottom of the panel and the button bar.
constexpr std::int32_t kButtonBarGap = 4;
constexpr std::int32_t kTitleHeight = 28;
constexpr std::int32_t kTitlePadX = 12;
constexpr std::int32_t kPopupWidth = 263;
constexpr std::int32_t kPopupOffsetY = -20;
constexpr std::int32_t kPopupBorderWidth = 1;
// The popup grows with its rows up to this many, then scrolls.
constexpr std::size_t kMaxVisiblePopupRows = 4;
constexpr std::int32_t kSaveFailureWidth = 236;
constexpr lv_opa_t kScrimOpacity = LV_OPA_50;
constexpr std::uint32_t kSlideDurationMs = 180;
constexpr std::uint32_t kSaveFailureDurationMs = 2000;

lv_obj_t* createPlainObject(lv_obj_t* parent) {
    lv_obj_t* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

// Places the button where the screens put theirs, so the bar does not move when the menu opens.
void copyGeometry(lv_obj_t* button, const lv_obj_t* reference) {
    lv_obj_set_pos(
        button,
        lv_obj_get_style_x(reference, LV_PART_MAIN),
        lv_obj_get_style_y(reference, LV_PART_MAIN));
    lv_obj_set_size(
        button,
        lv_obj_get_style_width(reference, LV_PART_MAIN),
        lv_obj_get_style_height(reference, LV_PART_MAIN));
}

lv_obj_t* addButtonLabel(lv_obj_t* button, const char* text) {
    lv_obj_t* label = lv_label_create(button);
    lv_obj_set_align(label, LV_ALIGN_CENTER);
    lv_label_set_text_static(label, text);
    return label;
}

void animate(
    void* target,
    lv_anim_exec_xcb_t exec,
    std::int32_t from,
    std::int32_t to,
    lv_anim_completed_cb_t completed = nullptr,
    void* userData = nullptr) {
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, target);
    lv_anim_set_exec_cb(&animation, exec);
    lv_anim_set_values(&animation, from, to);
    lv_anim_set_duration(&animation, kSlideDurationMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, completed);
    lv_anim_set_user_data(&animation, userData);
    lv_anim_start(&animation);
}

}  // namespace

MenuPanel::MenuPanel(MenuController& controller) : controller_(controller) {}

void MenuPanel::begin() {
    root_ = createPlainObject(lv_layer_top());
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Dims the screen and closes the menu when tapped outside the panel.
    scrim_ = createPlainObject(root_);
    lv_obj_set_size(scrim_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(scrim_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scrim_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_event_cb(scrim_, handleScrimEvent, LV_EVENT_CLICKED, this);

    const std::int32_t buttonBarTop =
        lv_obj_get_style_y(objects.ossm_control_settings_btn, LV_PART_MAIN);
    buildPanel(buttonBarTop - kButtonBarGap);
    buildPopup();
    buildButtonBar();
    buildSaveFailureLabel();
    setStopAvailable(false);
}

void MenuPanel::open(MenuPage& root) {
    controller_.open(root);
    clearTaps();
    hideSaveFailure();
    // Shown before rendering so scrolling to the highlighted row sees the final layout.
    slideIn();
    renderAll();
}

void MenuPanel::close() {
    controller_.close();
    controller_.takeChanges();
    clearTaps();
    hideSaveFailure();
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
    if (visible_) {
        slideOut();
    }
}

void MenuPanel::hide() {
    lv_anim_delete(panel_, setPanelX);
    lv_anim_delete(scrim_, setScrimOpacity);
    clearTaps();
    hideSaveFailure();
    visible_ = false;
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(panel_, -panelWidth_);
    lv_obj_set_style_bg_opa(scrim_, LV_OPA_TRANSP, LV_PART_MAIN);
}

void MenuPanel::show() {
    if (!controller_.isOpen()) {
        return;
    }

    controller_.refresh();
    clearTaps();
    slideIn();
    renderAll();
}

void MenuPanel::refresh() {
    if (!controller_.isOpen()) {
        return;
    }

    controller_.refresh();
    render(controller_.takeChanges());
}

MenuEvent MenuPanel::update(const RemoteInputEvents& events) {
    if (!controller_.isOpen() || !visible_) {
        clearTaps();
        return {};
    }

    if (!lv_obj_has_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN) &&
        millis() - saveFailureShownAtMs_ >= kSaveFailureDurationMs) {
        hideSaveFailure();
    }

    if (closeTapped_) {
        close();
        return {MenuEvent::Type::Closed};
    }

    MenuInput input;
    input.steps = events.menuSteps();
    // Raw knob directions, so the stroke direction setting has no effect in the menu.
    input.knobSteps = {events.encoderSteps[1], events.encoderSteps[2]};
    input.back = events.leftClick || backTapped_;
    input.select = events.rightClick || selectTapped_;
    input.tappedRow = tappedRow_;
    clearTaps();

    const MenuEvent event = controller_.update(input);
    if (event.type == MenuEvent::Type::Closed) {
        close();
        return event;
    }

    render(controller_.takeChanges());
    return event;
}

void MenuPanel::setStopAvailable(bool available) {
    if (available) {
        lv_obj_remove_flag(stopButton_, LV_OBJ_FLAG_HIDDEN);
    } else {
        stopButtonFeedback_.reset();
        lv_obj_add_flag(stopButton_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MenuPanel::setMotionActive(bool active) {
    stopButtonFeedback_.setMotionActive(active);
}

void MenuPanel::buildPanel(std::int32_t height) {
    panelWidth_ = kPanelWidth;
    panel_ = createPlainObject(root_);
    lv_obj_set_pos(panel_, -panelWidth_, 0);
    lv_obj_set_size(panel_, panelWidth_, height);
    lv_obj_set_style_bg_color(panel_, themeColor(COLOR_ID_BACKGROUND), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(panel_, themeColor(COLOR_ID_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_opa(panel_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(panel_, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(
        panel_,
        static_cast<lv_border_side_t>(LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM),
        LV_PART_MAIN);
    // The bottom border takes one pixel from the list.
    buildPageView(panelPage_, panel_, height - kTitleHeight - 1);
}

void MenuPanel::buildPopup() {
    popupScrim_ = createPlainObject(root_);
    lv_obj_set_size(popupScrim_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(popupScrim_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(popupScrim_, kScrimOpacity, LV_PART_MAIN);
    lv_obj_add_event_cb(popupScrim_, handleBackEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);

    popup_ = createPlainObject(popupScrim_);
    lv_obj_set_width(popup_, kPopupWidth);
    lv_obj_align(popup_, LV_ALIGN_CENTER, 0, kPopupOffsetY);
    lv_obj_set_style_bg_color(popup_, themeColor(COLOR_ID_SURFACE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(popup_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(popup_, themeColor(COLOR_ID_BORDER), LV_PART_MAIN);
    lv_obj_set_style_border_opa(popup_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(popup_, kPopupBorderWidth, LV_PART_MAIN);
    lv_obj_set_style_radius(popup_, 8, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(popup_, true, LV_PART_MAIN);
    buildPageView(popupPage_, popup_, 0);
}

void MenuPanel::buildButtonBar() {
    lv_obj_t* backButton = lv_button_create(root_);
    copyGeometry(backButton, objects.ossm_control_settings_btn);
    add_style_button_secondary(backButton);
    lv_obj_add_event_cb(backButton, handleBackEvent, LV_EVENT_CLICKED, this);
    backLabel_ = addButtonLabel(backButton, "Close");

    stopButton_ = lv_button_create(root_);
    copyGeometry(stopButton_, objects.ossm_control_stop_btn);
    add_style_button_danger(stopButton_);
    lv_obj_add_event_cb(stopButton_, action_stop, LV_EVENT_PRESSED, nullptr);
    addButtonLabel(stopButton_, "Stop");
    stopButtonFeedback_.begin(stopButton_);

    selectButton_ = lv_button_create(root_);
    copyGeometry(selectButton_, objects.ossm_control_patterns_btn);
    add_style_button_primary(selectButton_);
    lv_obj_add_event_cb(selectButton_, handleSelectEvent, LV_EVENT_CLICKED, this);
    selectLabel_ = addButtonLabel(selectButton_, "Select");
}

void MenuPanel::buildSaveFailureLabel() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    saveFailureLabel_ = lv_label_create(root_);
    lv_label_set_text_static(saveFailureLabel_, "Couldn't save. Try again.");
    lv_obj_set_width(saveFailureLabel_, kSaveFailureWidth);
    lv_obj_set_style_text_align(saveFailureLabel_, LV_TEXT_ALIGN_CENTER, mainStyle);
    lv_obj_set_style_text_color(saveFailureLabel_, themeColor(COLOR_ID_TEXT_PRIMARY), mainStyle);
    lv_obj_set_style_bg_color(saveFailureLabel_, themeColor(COLOR_ID_SURFACE), mainStyle);
    lv_obj_set_style_bg_opa(saveFailureLabel_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_pad_all(saveFailureLabel_, 8, mainStyle);
    lv_obj_set_style_radius(saveFailureLabel_, 6, mainStyle);
    lv_obj_set_style_border_width(saveFailureLabel_, 1, mainStyle);
    lv_obj_set_style_border_color(saveFailureLabel_, lv_palette_main(LV_PALETTE_RED), mainStyle);
    lv_obj_align(saveFailureLabel_, LV_ALIGN_CENTER, 0, kPopupOffsetY);
    lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuPanel::buildPageView(PageView& view, lv_obj_t* parent, std::int32_t listHeight) {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;
    const lv_font_t* titleFont = &lv_font_montserrat_14;

    // Uppercase label in the style of the value captions on the control screen.
    view.title = lv_label_create(parent);
    lv_obj_set_pos(view.title, 0, 0);
    lv_obj_set_size(view.title, lv_pct(100), kTitleHeight);
    lv_obj_set_style_text_font(view.title, titleFont, mainStyle);
    lv_obj_set_style_text_color(view.title, themeColor(COLOR_ID_TEXT_SECONDARY), mainStyle);
    lv_obj_set_style_pad_left(view.title, kTitlePadX, mainStyle);
    lv_obj_set_style_pad_right(view.title, kTitlePadX, mainStyle);
    lv_obj_set_style_pad_top(
        view.title, (kTitleHeight - 1 - lv_font_get_line_height(titleFont)) / 2, mainStyle);
    lv_obj_set_style_border_color(view.title, themeColor(COLOR_ID_BORDER), mainStyle);
    lv_obj_set_style_border_opa(view.title, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(view.title, 1, mainStyle);
    lv_obj_set_style_border_side(view.title, LV_BORDER_SIDE_BOTTOM, mainStyle);
    lv_label_set_text_static(view.title, view.titleText);

    view.list = createPlainObject(parent);
    lv_obj_add_flag(view.list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(view.list, LV_DIR_VER);
    lv_obj_set_pos(view.list, 0, kTitleHeight);
    lv_obj_set_size(view.list, lv_pct(100), listHeight);
    addSharedListStyles(view.list);
}

void MenuPanel::clearTaps() {
    tappedRow_ = MenuPage::kNoItem;
    backTapped_ = false;
    selectTapped_ = false;
    closeTapped_ = false;
}

void MenuPanel::slideIn() {
    lv_anim_delete(panel_, setPanelX);
    lv_anim_delete(scrim_, setScrimOpacity);
    visible_ = true;
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    animate(panel_, setPanelX, lv_obj_get_style_x(panel_, LV_PART_MAIN), 0);
    animate(scrim_, setScrimOpacity, lv_obj_get_style_bg_opa(scrim_, LV_PART_MAIN), kScrimOpacity);
}

void MenuPanel::slideOut() {
    lv_anim_delete(panel_, setPanelX);
    lv_anim_delete(scrim_, setScrimOpacity);
    visible_ = false;
    animate(
        panel_,
        setPanelX,
        lv_obj_get_style_x(panel_, LV_PART_MAIN),
        -panelWidth_,
        handleSlideOutCompleted,
        this);
    animate(scrim_, setScrimOpacity, lv_obj_get_style_bg_opa(scrim_, LV_PART_MAIN), LV_OPA_TRANSP);
}

void MenuPanel::renderAll() {
    MenuChanges changes = controller_.takeChanges();
    changes.pages = true;
    render(changes);
}

void MenuPanel::render(const MenuChanges& changes) {
    if (!controller_.isOpen()) {
        return;
    }

    const std::size_t top = controller_.depth() - 1;
    const std::size_t base = panelLevel();
    if (changes.pages) {
        showPage(panelPage_, base);
        // A popup opened from a popup replaces it rather than stacking.
        if (top != base) {
            showPage(popupPage_, top);
            resizePopup(controller_.page(top).count());
            lv_obj_remove_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (changes.highlight) {
            updateHighlight(top == base ? panelPage_ : popupPage_, top);
        }
        if (changes.values) {
            updateValues(base);
            if (top != base) {
                updateValues(top);
            }
        }
    }

    updateButtonBar();
    if (changes.saveFailed) {
        showSaveFailure();
    }
}

std::size_t MenuPanel::panelLevel() const {
    std::size_t level = controller_.depth() - 1;
    while (level > 0 && controller_.isPopup(level)) {
        --level;
    }
    return level;
}

void MenuPanel::showPage(PageView& view, std::size_t level) {
    MenuPage& page = controller_.page(level);
    setTitle(view, page.title());

    for (std::size_t index = 0; index < rowCount_; ++index) {
        if (rows_[index].list == view.list) {
            lv_obj_add_flag(rows_[index].row, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (std::size_t index = 0; index < page.count(); ++index) {
        RowView* row = rowFor(page.item(index), view.list);
        if (row == nullptr) {
            break;
        }
        lv_obj_move_to_index(row->row, static_cast<std::int32_t>(index));
        lv_obj_remove_flag(row->row, LV_OBJ_FLAG_HIDDEN);
        refreshRow(*row);
    }

    updateHighlight(view, level);
}

void MenuPanel::updateHighlight(PageView& view, std::size_t level) {
    const MenuPage& page = controller_.page(level);
    const std::size_t highlight = controller_.highlight(level);
    lv_obj_t* highlighted = nullptr;
    for (std::size_t index = 0; index < page.count(); ++index) {
        RowView* row = findRow(page.item(index));
        if (row == nullptr) {
            continue;
        }
        const bool selected = index == highlight;
        setListRowSelected(row->row, selected);
        if (selected) {
            highlighted = row->row;
        }
    }

    if (highlighted != nullptr) {
        lv_obj_scroll_to_view(highlighted, LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(view.list, 0, LV_ANIM_OFF);
    }
}

void MenuPanel::updateValues(std::size_t level) {
    const MenuPage& page = controller_.page(level);
    for (std::size_t index = 0; index < page.count(); ++index) {
        const RowView* row = findRow(page.item(index));
        if (row != nullptr) {
            refreshRow(*row);
        }
    }
}

void MenuPanel::updateButtonBar() {
    lv_label_set_text_static(backLabel_, controller_.depth() > 1 ? "Back" : "Close");

    const MenuItem* item = controller_.highlightedItem();
    lv_label_set_text_static(selectLabel_, item != nullptr ? item->selectLabel() : "Select");
    if (item != nullptr) {
        lv_obj_remove_state(selectButton_, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(selectButton_, LV_STATE_DISABLED);
    }
}

void MenuPanel::resizePopup(std::size_t rowCount) {
    const std::size_t visibleRows = std::min(rowCount, kMaxVisiblePopupRows);
    const std::int32_t listHeight = static_cast<std::int32_t>(visibleRows) * listRowHeight();
    lv_obj_set_height(popupPage_.list, listHeight);
    lv_obj_set_height(popup_, kTitleHeight + listHeight + 2 * kPopupBorderWidth);
}

void MenuPanel::showSaveFailure() {
    saveFailureShownAtMs_ = millis();
    lv_obj_remove_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuPanel::hideSaveFailure() {
    lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuPanel::setTitle(PageView& view, const char* title) {
    std::size_t length = 0;
    for (; title[length] != '\0' && length < sizeof(view.titleText) - 1; ++length) {
        view.titleText[length] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(title[length])));
    }
    view.titleText[length] = '\0';
    lv_label_set_text_static(view.title, view.titleText);
}

MenuPanel::RowView* MenuPanel::rowFor(MenuItem& item, lv_obj_t* list) {
    RowView* existing = findRow(item);
    if (existing != nullptr) {
        if (existing->list != list) {
            lv_obj_set_parent(existing->row, list);
            existing->list = list;
        }
        return existing;
    }

    if (rowCount_ >= rows_.size()) {
        LOGW(kTag, "No room for menu row %s", item.name());
        return nullptr;
    }

    RowView& row = rows_[rowCount_++];
    row.item = &item;
    row.list = list;
    createRow(row);
    return &row;
}

MenuPanel::RowView* MenuPanel::findRow(const MenuItem& item) {
    for (std::size_t index = 0; index < rowCount_; ++index) {
        if (rows_[index].item == &item) {
            return &rows_[index];
        }
    }
    return nullptr;
}

void MenuPanel::createRow(RowView& view) {
    view.row = createPlainObject(view.list);
    addSharedListRowStyles(view.row);
    lv_obj_add_event_cb(view.row, handleRowEvent, LV_EVENT_CLICKED, this);

    view.name = lv_label_create(view.row);
    addSharedListRowNameStyles(view.name);
    lv_label_set_long_mode(view.name, LV_LABEL_LONG_MODE_DOTS);

    const MenuItem& item = *view.item;
    if (item.kind() == MenuItemKind::Choice &&
        static_cast<const MenuChoiceItem&>(item).showsLevel()) {
        view.level = lv_bar_create(view.row);
        lv_obj_remove_style_all(view.level);
        addSharedListRowLevelStyles(view.level);
        // Taps on the bar select the row.
        lv_obj_remove_flag(view.level, LV_OBJ_FLAG_CLICKABLE);
        return;
    }

    view.value = lv_label_create(view.row);
    addSharedListRowValueStyles(view.value, item.kind() == MenuItemKind::Option);
}

void MenuPanel::handleRowClicked(lv_obj_t* row) {
    if (!controller_.isOpen()) {
        return;
    }

    const MenuPage& page = controller_.page(controller_.depth() - 1);
    for (std::size_t index = 0; index < rowCount_; ++index) {
        if (rows_[index].row == row) {
            tappedRow_ = page.indexOf(*rows_[index].item);
            return;
        }
    }
}

void MenuPanel::refreshRow(const RowView& view) {
    const MenuItem& item = *view.item;
    lv_label_set_text_static(view.name, item.name());

    if (view.level != nullptr) {
        const auto& choice = static_cast<const MenuChoiceItem&>(item);
        const std::size_t count = choice.optionCount();
        const std::size_t option = choice.displayedOption();
        // The lowest option still shows one step, like the first bar of a signal meter.
        lv_bar_set_range(view.level, 0, static_cast<std::int32_t>(count));
        lv_bar_set_value(
            view.level, option < count ? static_cast<std::int32_t>(option) + 1 : 0, LV_ANIM_OFF);
        return;
    }

    if (item.kind() == MenuItemKind::Option) {
        // Marks the stored option; the highlight shows where the cursor is.
        lv_label_set_text_static(view.value, LV_SYMBOL_OK);
        if (static_cast<const MenuOptionItem&>(item).stored()) {
            lv_obj_remove_flag(view.value, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(view.value, LV_OBJ_FLAG_HIDDEN);
        }
        return;
    }

    lv_label_set_text_static(view.value, item.showsChevron() ? LV_SYMBOL_RIGHT : item.valueText());
}

void MenuPanel::setPanelX(void* panel, std::int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(panel), x);
}

void MenuPanel::setScrimOpacity(void* scrim, std::int32_t opacity) {
    lv_obj_set_style_bg_opa(
        static_cast<lv_obj_t*>(scrim), static_cast<lv_opa_t>(opacity), LV_PART_MAIN);
}

void MenuPanel::handleSlideOutCompleted(lv_anim_t* animation) {
    auto* panel = static_cast<MenuPanel*>(lv_anim_get_user_data(animation));
    lv_obj_add_flag(panel->root_, LV_OBJ_FLAG_HIDDEN);
}

void MenuPanel::handleRowEvent(lv_event_t* event) {
    auto* panel = static_cast<MenuPanel*>(lv_event_get_user_data(event));
    panel->handleRowClicked(static_cast<lv_obj_t*>(lv_event_get_current_target(event)));
}

void MenuPanel::handleBackEvent(lv_event_t* event) {
    static_cast<MenuPanel*>(lv_event_get_user_data(event))->backTapped_ = true;
}

void MenuPanel::handleSelectEvent(lv_event_t* event) {
    static_cast<MenuPanel*>(lv_event_get_user_data(event))->selectTapped_ = true;
}

void MenuPanel::handleScrimEvent(lv_event_t* event) {
    static_cast<MenuPanel*>(lv_event_get_user_data(event))->closeTapped_ = true;
}

}  // namespace m5_redux
