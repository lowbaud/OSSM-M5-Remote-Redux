#include "MenuScreen.h"

#include <Arduino.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "diagnostics/Log.h"
#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"
#include "ui/generated/actions.h"
#include "ui/generated/screens.h"
#include "ui/generated/styles.h"

namespace m5_redux {

namespace {

constexpr const char* kTag = "menu";
// The list matches the old settings screen.
constexpr std::int32_t kListBarGap = 7;
constexpr std::int32_t kListPadX = 10;
constexpr std::int32_t kListPadTop = 6;
constexpr std::int32_t kStatusHeight = 30;
constexpr std::int32_t kStatusPadX = 12;
constexpr std::int32_t kStatusGap = 6;
constexpr std::int32_t kStatusDotSize = 8;
constexpr std::int32_t kTitleHeight = 28;
constexpr std::int32_t kTitlePadX = 12;
constexpr std::int32_t kPopupWidth = 263;
constexpr std::int32_t kPopupOffsetY = -20;
constexpr std::int32_t kPopupBorderWidth = 1;
// The popup grows with its rows up to this many, then scrolls.
constexpr std::size_t kMaxVisiblePopupRows = 4;
constexpr lv_opa_t kPopupScrimOpacity = LV_OPA_50;
constexpr std::int32_t kSaveFailureWidth = 236;
constexpr std::uint32_t kSaveFailureDurationMs = 2000;
// Matches the screen changes of the EEZ screens.
constexpr std::uint32_t kScreenFadeMs = 200;

// Places the button where the screens put theirs, so the bar does not move when a menu shows.
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

lv_obj_t* addStatusLabel(lv_obj_t* bar, const lv_font_t* font, Colors color) {
    lv_obj_t* label = lv_label_create(bar);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, themeColor(color), LV_PART_MAIN);
    return label;
}

// Setting the same static text again would still redraw the label.
void setStaticText(lv_obj_t* label, const char* text) {
    if (lv_label_get_text(label) != text) {
        lv_label_set_text_static(label, text);
    }
}

void setHidden(lv_obj_t* object, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

}  // namespace

void MenuScreen::begin() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    screen_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen_);
    lv_obj_remove_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, themeColor(COLOR_ID_BACKGROUND), mainStyle);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, mainStyle);

    buildStatusBar();

    listBottom_ = lv_obj_get_style_y(objects.ossm_control_settings_btn, LV_PART_MAIN) - kListBarGap;
    list_ = createMenuScrollArea(screen_);
    lv_obj_set_width(list_, lv_pct(100));
    addSharedListStyles(list_);
    lv_obj_set_style_pad_left(list_, kListPadX, mainStyle);
    lv_obj_set_style_pad_right(list_, kListPadX, mainStyle);
    lv_obj_set_style_pad_top(list_, kListPadTop, mainStyle);
    pageView_.container = list_;
    pageView_.tileRow = createMenuTileRow(list_);
    hideStatus();

    buildPopup();
    buildButtonBar();
    buildSaveFailureLabel();
    setStopAvailable(false);
}

void MenuScreen::open(MenuPage& root, const char* rootBackLabel) {
    rootBackLabel_ = rootBackLabel;
    controller_.open(root);
    clearInput();
    hideSaveFailure();
    renderAll();
    load();
}

void MenuScreen::resume() {
    if (controller_.isOpen()) {
        controller_.refresh();
        clearInput();
        renderAll();
    }
    load();
}

void MenuScreen::close() {
    closePages();
}

bool MenuScreen::isOpen() const {
    return controller_.isOpen();
}

MenuEvent MenuScreen::update(const RemoteInputEvents& events) {
    if (!controller_.isOpen()) {
        clearInput();
        return {};
    }

    if (!lv_obj_has_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN) &&
        millis() - saveFailureShownAtMs_ >= kSaveFailureDurationMs) {
        hideSaveFailure();
    }

    MenuInput input;
    input.steps = events.menuSteps();
    // Raw knob directions, so the stroke direction setting has no effect in the menu.
    input.knobSteps = {events.encoderSteps[1], events.encoderSteps[2]};
    input.back = events.leftClick || backTapped_;
    input.select = events.rightClick || selectTapped_;
    input.tappedRow = tappedRow_;
    clearInput();

    const MenuEvent event = controller_.update(input);
    if (event.type == MenuEvent::Type::Closed) {
        closePages();
        return event;
    }

    render(controller_.takeChanges());
    return event;
}

void MenuScreen::setStopAvailable(bool available) {
    if (available) {
        lv_obj_remove_flag(stopButton_, LV_OBJ_FLAG_HIDDEN);
    } else {
        stopButtonFeedback_.reset();
        lv_obj_add_flag(stopButton_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MenuScreen::setMotionActive(bool active) {
    stopButtonFeedback_.setMotionActive(active);
}

void MenuScreen::showStatus(const char* pattern, int speed, int sensation) {
    std::snprintf(patternText_, sizeof(patternText_), "%s", pattern);
    std::snprintf(speedText_, sizeof(speedText_), "%d", speed);
    std::snprintf(sensationText_, sizeof(sensationText_), "%d", sensation);
    // The same buffers again, so the labels have to be told to read them.
    lv_label_set_text_static(statusPattern_, patternText_);
    lv_label_set_text_static(statusSpeed_, speedText_);
    lv_label_set_text_static(statusSensation_, sensationText_);
    if (speed > 0) {
        lv_obj_add_state(statusDot_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(statusDot_, LV_STATE_CHECKED);
    }

    lv_obj_remove_flag(statusBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(list_, kStatusHeight);
    lv_obj_set_height(list_, listBottom_ - kStatusHeight);
}

void MenuScreen::hideStatus() {
    lv_obj_add_flag(statusBar_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(list_, 0);
    lv_obj_set_height(list_, listBottom_);
}

void MenuScreen::buildStatusBar() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    statusBar_ = createMenuObject(screen_);
    lv_obj_set_pos(statusBar_, 0, 0);
    lv_obj_set_size(statusBar_, lv_pct(100), kStatusHeight);
    lv_obj_set_flex_flow(statusBar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        statusBar_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_left(statusBar_, kStatusPadX, mainStyle);
    lv_obj_set_style_pad_right(statusBar_, kStatusPadX, mainStyle);
    lv_obj_set_style_pad_column(statusBar_, kStatusGap, mainStyle);
    lv_obj_set_style_border_color(statusBar_, themeColor(COLOR_ID_DIVIDER), mainStyle);
    lv_obj_set_style_border_opa(statusBar_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(statusBar_, 1, mainStyle);
    lv_obj_set_style_border_side(statusBar_, LV_BORDER_SIDE_BOTTOM, mainStyle);

    // Lit while the device moves.
    statusDot_ = createMenuObject(statusBar_);
    lv_obj_set_size(statusDot_, kStatusDotSize, kStatusDotSize);
    lv_obj_set_style_radius(statusDot_, LV_RADIUS_CIRCLE, mainStyle);
    lv_obj_set_style_bg_opa(statusDot_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_bg_color(statusDot_, themeColor(COLOR_ID_DIVIDER), mainStyle);
    lv_obj_set_style_bg_color(statusDot_, themeColor(COLOR_ID_ACCENT), LV_STATE_CHECKED);

    statusPattern_ = addStatusLabel(statusBar_, &lv_font_montserrat_14, COLOR_ID_TEXT_PRIMARY);
    lv_obj_set_flex_grow(statusPattern_, 1);
    lv_label_set_long_mode(statusPattern_, LV_LABEL_LONG_MODE_DOTS);

    // Captions in the style of the control screen's.
    lv_obj_t* speedCaption =
        addStatusLabel(statusBar_, &lv_font_montserrat_12, COLOR_ID_TEXT_SECONDARY);
    lv_label_set_text_static(speedCaption, "SPEED");
    statusSpeed_ = addStatusLabel(statusBar_, &lv_font_montserrat_16, COLOR_ID_TEXT_PRIMARY);
    lv_obj_t* sensationCaption =
        addStatusLabel(statusBar_, &lv_font_montserrat_12, COLOR_ID_TEXT_SECONDARY);
    lv_label_set_text_static(sensationCaption, "SENS");
    // Separates the two pairs.
    lv_obj_set_style_pad_left(sensationCaption, kStatusGap, mainStyle);
    statusSensation_ = addStatusLabel(statusBar_, &lv_font_montserrat_16, COLOR_ID_TEXT_PRIMARY);
}

void MenuScreen::buildPopup() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;
    const lv_font_t* titleFont = &lv_font_montserrat_14;

    popupScrim_ = createMenuObject(screen_);
    lv_obj_set_size(popupScrim_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(popupScrim_, lv_color_black(), mainStyle);
    lv_obj_set_style_bg_opa(popupScrim_, kPopupScrimOpacity, mainStyle);
    lv_obj_add_event_cb(popupScrim_, handleBackEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);

    popup_ = createMenuObject(popupScrim_);
    lv_obj_set_width(popup_, kPopupWidth);
    lv_obj_align(popup_, LV_ALIGN_CENTER, 0, kPopupOffsetY);
    lv_obj_set_style_bg_color(popup_, themeColor(COLOR_ID_SURFACE), mainStyle);
    lv_obj_set_style_bg_opa(popup_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_color(popup_, themeColor(COLOR_ID_BORDER), mainStyle);
    lv_obj_set_style_border_opa(popup_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(popup_, kPopupBorderWidth, mainStyle);
    lv_obj_set_style_radius(popup_, 8, mainStyle);
    lv_obj_set_style_clip_corner(popup_, true, mainStyle);

    // Uppercase label in the style of the value captions on the control screen.
    lv_obj_t* title = lv_label_create(popup_);
    popupView_.title = title;
    lv_obj_set_pos(title, 0, 0);
    lv_obj_set_size(title, lv_pct(100), kTitleHeight);
    lv_obj_set_style_text_font(title, titleFont, mainStyle);
    lv_obj_set_style_text_color(title, themeColor(COLOR_ID_TEXT_SECONDARY), mainStyle);
    lv_obj_set_style_pad_left(title, kTitlePadX, mainStyle);
    lv_obj_set_style_pad_right(title, kTitlePadX, mainStyle);
    lv_obj_set_style_pad_top(
        title, (kTitleHeight - 1 - lv_font_get_line_height(titleFont)) / 2, mainStyle);
    lv_obj_set_style_border_color(title, themeColor(COLOR_ID_BORDER), mainStyle);
    lv_obj_set_style_border_opa(title, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(title, 1, mainStyle);
    lv_obj_set_style_border_side(title, LV_BORDER_SIDE_BOTTOM, mainStyle);
    lv_label_set_text_static(title, popupView_.titleText);

    popupView_.container = createMenuScrollArea(popup_);
    lv_obj_set_pos(popupView_.container, 0, kTitleHeight);
    lv_obj_set_width(popupView_.container, lv_pct(100));
    addSharedListStyles(popupView_.container);
}

void MenuScreen::buildButtonBar() {
    lv_obj_t* backButton = lv_button_create(screen_);
    copyGeometry(backButton, objects.ossm_control_settings_btn);
    add_style_button_secondary(backButton);
    lv_obj_add_event_cb(backButton, handleBackEvent, LV_EVENT_CLICKED, this);
    backLabel_ = addButtonLabel(backButton, rootBackLabel_);

    stopButton_ = lv_button_create(screen_);
    copyGeometry(stopButton_, objects.ossm_control_stop_btn);
    add_style_button_danger(stopButton_);
    lv_obj_add_event_cb(stopButton_, action_stop, LV_EVENT_PRESSED, nullptr);
    addButtonLabel(stopButton_, "Stop");
    stopButtonFeedback_.begin(stopButton_);

    selectButton_ = lv_button_create(screen_);
    copyGeometry(selectButton_, objects.ossm_control_patterns_btn);
    add_style_button_primary(selectButton_);
    lv_obj_add_event_cb(selectButton_, handleSelectEvent, LV_EVENT_CLICKED, this);
    selectLabel_ = addButtonLabel(selectButton_, "Select");
}

void MenuScreen::buildSaveFailureLabel() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    saveFailureLabel_ = lv_label_create(screen_);
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

void MenuScreen::load() {
    if (lv_screen_active() != screen_) {
        lv_screen_load_anim(screen_, LV_SCREEN_LOAD_ANIM_FADE_IN, kScreenFadeMs, 0, false);
    }
}

void MenuScreen::closePages() {
    controller_.close();
    controller_.takeChanges();
    clearInput();
    hideSaveFailure();
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
}

void MenuScreen::clearInput() {
    tappedRow_ = MenuPage::kNoItem;
    backTapped_ = false;
    selectTapped_ = false;
}

void MenuScreen::renderAll() {
    MenuChanges changes = controller_.takeChanges();
    changes.pages = true;
    render(changes);
}

void MenuScreen::render(const MenuChanges& changes) {
    if (!controller_.isOpen()) {
        return;
    }
    // Nothing to redraw; touching the widgets anyway would still make LVGL redraw them.
    if (!changes.pages && !changes.highlight && !changes.values && !changes.saveFailed) {
        return;
    }

    const std::size_t top = controller_.depth() - 1;
    const std::size_t base = baseLevel();
    if (changes.pages) {
        showPage(pageView_, base);
        // A popup opened from a popup replaces it rather than stacking.
        if (top != base) {
            showPage(popupView_, top);
            resizePopup(controller_.page(top).count());
            lv_obj_remove_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (changes.highlight) {
            updateHighlight(top);
        }
        if (changes.values) {
            updateValues(top);
            if (top != base) {
                updateValues(base);
            }
        }
    }

    updateButtonBar();
    if (changes.saveFailed) {
        showSaveFailure();
    }
}

std::size_t MenuScreen::baseLevel() const {
    std::size_t level = controller_.depth() - 1;
    while (level > 0 && controller_.isPopup(level)) {
        --level;
    }
    return level;
}

MenuScreen::PageView& MenuScreen::viewOf(std::size_t level) {
    return level > baseLevel() ? popupView_ : pageView_;
}

void MenuScreen::showPage(PageView& view, std::size_t level) {
    MenuPage& page = controller_.page(level);
    if (view.title != nullptr) {
        setTitle(view, page.title());
    }

    for (std::size_t index = 0; index < viewCount_; ++index) {
        const lv_obj_t* container = views_[index].container;
        if (container == view.container || (view.tileRow != nullptr && container == view.tileRow)) {
            lv_obj_add_flag(views_[index].root, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const std::size_t tiles = view.tileRow != nullptr ? page.tileCount() : 0;
    if (view.tileRow != nullptr) {
        setHidden(view.tileRow, tiles == 0);
    }
    // Rows follow the tile row, which stays the first child of the list.
    const std::size_t firstRow = view.tileRow != nullptr ? 1 : 0;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const bool tile = index < tiles;
        MenuItemView* itemView =
            viewFor(page.item(index), tile ? view.tileRow : view.container, tile);
        if (itemView == nullptr) {
            break;
        }
        const std::size_t position = tile ? index : index - tiles + firstRow;
        lv_obj_move_to_index(itemView->root, static_cast<std::int32_t>(position));
        lv_obj_remove_flag(itemView->root, LV_OBJ_FLAG_HIDDEN);
        refreshMenuItemView(*itemView);
    }

    updateHighlight(level);
}

void MenuScreen::updateHighlight(std::size_t level) {
    const PageView& view = viewOf(level);
    const MenuPage& page = controller_.page(level);
    const std::size_t tiles = view.tileRow != nullptr ? page.tileCount() : 0;
    const std::size_t highlight = controller_.highlight(level);
    lv_obj_t* highlighted = nullptr;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), index < tiles);
        if (itemView == nullptr) {
            continue;
        }
        const bool selected = index == highlight;
        setMenuItemViewSelected(*itemView, selected);
        if (selected) {
            // Tiles scroll with their row, which the list holds.
            highlighted = itemView->tile ? view.tileRow : itemView->root;
        }
    }

    if (highlighted != nullptr) {
        lv_obj_scroll_to_view(highlighted, LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(view.container, 0, LV_ANIM_OFF);
    }
}

void MenuScreen::updateValues(std::size_t level) {
    const PageView& view = viewOf(level);
    const MenuPage& page = controller_.page(level);
    const std::size_t tiles = view.tileRow != nullptr ? page.tileCount() : 0;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), index < tiles);
        if (itemView != nullptr) {
            refreshMenuItemView(*itemView);
        }
    }
}

void MenuScreen::updateButtonBar() {
    setStaticText(backLabel_, controller_.depth() > 1 ? "Back" : rootBackLabel_);

    const MenuItem* item = controller_.highlightedItem();
    setStaticText(selectLabel_, item != nullptr ? item->selectLabel() : "Select");
    if (item != nullptr) {
        lv_obj_remove_state(selectButton_, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(selectButton_, LV_STATE_DISABLED);
    }
}

void MenuScreen::resizePopup(std::size_t rowCount) {
    const std::size_t visibleRows = std::min(rowCount, kMaxVisiblePopupRows);
    const std::int32_t listHeight = static_cast<std::int32_t>(visibleRows) * listRowHeight();
    lv_obj_set_height(popupView_.container, listHeight);
    lv_obj_set_height(popup_, kTitleHeight + listHeight + 2 * kPopupBorderWidth);
}

void MenuScreen::showSaveFailure() {
    saveFailureShownAtMs_ = millis();
    lv_obj_remove_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuScreen::hideSaveFailure() {
    lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuScreen::setTitle(PageView& view, const char* title) {
    std::size_t length = 0;
    for (; title[length] != '\0' && length < sizeof(view.titleText) - 1; ++length) {
        view.titleText[length] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(title[length])));
    }
    view.titleText[length] = '\0';
    lv_label_set_text_static(view.title, view.titleText);
}

MenuItemView* MenuScreen::viewFor(MenuItem& item, lv_obj_t* container, bool tile) {
    MenuItemView* existing = findView(item, tile);
    if (existing != nullptr) {
        // List rows move between the page and the popup.
        if (existing->container != container) {
            lv_obj_set_parent(existing->root, container);
            existing->container = container;
        }
        return existing;
    }

    if (viewCount_ >= views_.size()) {
        LOGW(kTag, "No room for menu item %s", item.name());
        return nullptr;
    }

    MenuItemView& view = views_[viewCount_++];
    view.item = &item;
    if (tile) {
        createMenuTile(view, container);
    } else {
        createMenuRow(view, container);
    }
    lv_obj_add_event_cb(view.root, handleViewEvent, LV_EVENT_CLICKED, this);
    return &view;
}

MenuItemView* MenuScreen::findView(const MenuItem& item, bool tile) {
    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].item == &item && views_[index].tile == tile) {
            return &views_[index];
        }
    }
    return nullptr;
}

void MenuScreen::handleViewClicked(lv_obj_t* root) {
    if (!controller_.isOpen()) {
        return;
    }

    const MenuPage& page = controller_.page(controller_.depth() - 1);
    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].root == root) {
            tappedRow_ = page.indexOf(*views_[index].item);
            return;
        }
    }
}

void MenuScreen::handleViewEvent(lv_event_t* event) {
    auto* menu = static_cast<MenuScreen*>(lv_event_get_user_data(event));
    menu->handleViewClicked(static_cast<lv_obj_t*>(lv_event_get_current_target(event)));
}

void MenuScreen::handleBackEvent(lv_event_t* event) {
    static_cast<MenuScreen*>(lv_event_get_user_data(event))->backTapped_ = true;
}

void MenuScreen::handleSelectEvent(lv_event_t* event) {
    static_cast<MenuScreen*>(lv_event_get_user_data(event))->selectTapped_ = true;
}

}  // namespace m5_redux
