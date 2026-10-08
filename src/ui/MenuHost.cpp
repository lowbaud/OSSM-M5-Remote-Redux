#include "MenuHost.h"

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

// Setting the same static text again would still redraw the label.
void setStaticText(lv_obj_t* label, const char* text) {
    if (lv_label_get_text(label) != text) {
        lv_label_set_text_static(label, text);
    }
}

}  // namespace

bool MenuHost::isOpen() const {
    return controller_.isOpen();
}

MenuEvent MenuHost::update(const RemoteInputEvents& events) {
    if (!controller_.isOpen()) {
        clearInput();
        return {};
    }

    if (!lv_obj_has_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN) &&
        millis() - saveFailureShownAtMs_ >= kSaveFailureDurationMs) {
        hideSaveFailure();
    }

    if (closeRequested_) {
        closePages();
        pagesClosed();
        return {MenuEvent::Type::Closed};
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
        pagesClosed();
        return event;
    }

    render(controller_.takeChanges());
    return event;
}

void MenuHost::setStopAvailable(bool available) {
    if (available) {
        lv_obj_remove_flag(stopButton_, LV_OBJ_FLAG_HIDDEN);
    } else {
        stopButtonFeedback_.reset();
        lv_obj_add_flag(stopButton_, LV_OBJ_FLAG_HIDDEN);
    }
}

void MenuHost::setMotionActive(bool active) {
    stopButtonFeedback_.setMotionActive(active);
}

void MenuHost::buildChrome(lv_obj_t* parent, const PageLayout& layout) {
    pageView_.container = layout.container;
    pageView_.tiles = layout.tiles;
    tileWidth_ = layout.tileWidth;
    rootBackLabel_ = layout.rootBackLabel;

    buildPopup(parent);
    buildButtonBar(parent);
    buildSaveFailureLabel(parent);
    setStopAvailable(false);
}

void MenuHost::openPages(MenuPage& root) {
    controller_.open(root);
    clearInput();
    hideSaveFailure();
    renderAll();
}

void MenuHost::closePages() {
    controller_.close();
    controller_.takeChanges();
    clearInput();
    hideSaveFailure();
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
}

void MenuHost::refreshPages() {
    if (!controller_.isOpen()) {
        return;
    }

    controller_.refresh();
    clearInput();
    renderAll();
}

void MenuHost::requestClose() {
    closeRequested_ = true;
}

void MenuHost::pagesClosed() {}

void MenuHost::buildPopup(lv_obj_t* parent) {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;
    const lv_font_t* titleFont = &lv_font_montserrat_14;

    popupScrim_ = createMenuObject(parent);
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

void MenuHost::buildButtonBar(lv_obj_t* parent) {
    lv_obj_t* backButton = lv_button_create(parent);
    copyGeometry(backButton, objects.ossm_control_settings_btn);
    add_style_button_secondary(backButton);
    lv_obj_add_event_cb(backButton, handleBackEvent, LV_EVENT_CLICKED, this);
    backLabel_ = addButtonLabel(backButton, rootBackLabel_);

    stopButton_ = lv_button_create(parent);
    copyGeometry(stopButton_, objects.ossm_control_stop_btn);
    add_style_button_danger(stopButton_);
    lv_obj_add_event_cb(stopButton_, action_stop, LV_EVENT_PRESSED, nullptr);
    addButtonLabel(stopButton_, "Stop");
    stopButtonFeedback_.begin(stopButton_);

    selectButton_ = lv_button_create(parent);
    copyGeometry(selectButton_, objects.ossm_control_patterns_btn);
    add_style_button_primary(selectButton_);
    lv_obj_add_event_cb(selectButton_, handleSelectEvent, LV_EVENT_CLICKED, this);
    selectLabel_ = addButtonLabel(selectButton_, "Select");
}

void MenuHost::buildSaveFailureLabel(lv_obj_t* parent) {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    saveFailureLabel_ = lv_label_create(parent);
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

void MenuHost::clearInput() {
    tappedRow_ = MenuPage::kNoItem;
    backTapped_ = false;
    selectTapped_ = false;
    closeRequested_ = false;
}

void MenuHost::renderAll() {
    MenuChanges changes = controller_.takeChanges();
    changes.pages = true;
    render(changes);
}

void MenuHost::render(const MenuChanges& changes) {
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

std::size_t MenuHost::baseLevel() const {
    std::size_t level = controller_.depth() - 1;
    while (level > 0 && controller_.isPopup(level)) {
        --level;
    }
    return level;
}

MenuHost::PageView& MenuHost::viewOf(std::size_t level) {
    return level > baseLevel() ? popupView_ : pageView_;
}

void MenuHost::showPage(PageView& view, std::size_t level) {
    MenuPage& page = controller_.page(level);
    if (view.title != nullptr) {
        setTitle(view, page.title());
    }

    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].container == view.container) {
            lv_obj_add_flag(views_[index].root, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (std::size_t index = 0; index < page.count(); ++index) {
        MenuItemView* itemView = viewFor(page.item(index), view.container, view.tiles);
        if (itemView == nullptr) {
            break;
        }
        lv_obj_move_to_index(itemView->root, static_cast<std::int32_t>(index));
        lv_obj_remove_flag(itemView->root, LV_OBJ_FLAG_HIDDEN);
        refreshMenuItemView(*itemView);
    }

    updateHighlight(level);
}

void MenuHost::updateHighlight(std::size_t level) {
    const PageView& view = viewOf(level);
    const MenuPage& page = controller_.page(level);
    const std::size_t highlight = controller_.highlight(level);
    lv_obj_t* highlighted = nullptr;
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), view.tiles);
        if (itemView == nullptr) {
            continue;
        }
        const bool selected = index == highlight;
        setMenuItemViewSelected(*itemView, selected);
        if (selected) {
            highlighted = itemView->root;
        }
    }

    if (highlighted != nullptr) {
        lv_obj_scroll_to_view(highlighted, LV_ANIM_OFF);
    } else {
        lv_obj_scroll_to_y(view.container, 0, LV_ANIM_OFF);
    }
}

void MenuHost::updateValues(std::size_t level) {
    const PageView& view = viewOf(level);
    const MenuPage& page = controller_.page(level);
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), view.tiles);
        if (itemView != nullptr) {
            refreshMenuItemView(*itemView);
        }
    }
}

void MenuHost::updateButtonBar() {
    setStaticText(backLabel_, controller_.depth() > 1 ? "Back" : rootBackLabel_);

    const MenuItem* item = controller_.highlightedItem();
    setStaticText(selectLabel_, item != nullptr ? item->selectLabel() : "Select");
    if (item != nullptr) {
        lv_obj_remove_state(selectButton_, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(selectButton_, LV_STATE_DISABLED);
    }
}

void MenuHost::resizePopup(std::size_t rowCount) {
    const std::size_t visibleRows = std::min(rowCount, kMaxVisiblePopupRows);
    const std::int32_t listHeight = static_cast<std::int32_t>(visibleRows) * listRowHeight();
    lv_obj_set_height(popupView_.container, listHeight);
    lv_obj_set_height(popup_, kTitleHeight + listHeight + 2 * kPopupBorderWidth);
}

void MenuHost::showSaveFailure() {
    saveFailureShownAtMs_ = millis();
    lv_obj_remove_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuHost::hideSaveFailure() {
    lv_obj_add_flag(saveFailureLabel_, LV_OBJ_FLAG_HIDDEN);
}

void MenuHost::setTitle(PageView& view, const char* title) {
    std::size_t length = 0;
    for (; title[length] != '\0' && length < sizeof(view.titleText) - 1; ++length) {
        view.titleText[length] =
            static_cast<char>(std::toupper(static_cast<unsigned char>(title[length])));
    }
    view.titleText[length] = '\0';
    lv_label_set_text_static(view.title, view.titleText);
}

MenuItemView* MenuHost::viewFor(MenuItem& item, lv_obj_t* container, bool tile) {
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
        createMenuTile(view, container, tileWidth_);
    } else {
        createMenuRow(view, container);
    }
    lv_obj_add_event_cb(view.root, handleViewEvent, LV_EVENT_CLICKED, this);
    return &view;
}

MenuItemView* MenuHost::findView(const MenuItem& item, bool tile) {
    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].item == &item && views_[index].tile == tile) {
            return &views_[index];
        }
    }
    return nullptr;
}

void MenuHost::handleViewClicked(lv_obj_t* root) {
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

void MenuHost::handleViewEvent(lv_event_t* event) {
    auto* host = static_cast<MenuHost*>(lv_event_get_user_data(event));
    host->handleViewClicked(static_cast<lv_obj_t*>(lv_event_get_current_target(event)));
}

void MenuHost::handleBackEvent(lv_event_t* event) {
    static_cast<MenuHost*>(lv_event_get_user_data(event))->backTapped_ = true;
}

void MenuHost::handleSelectEvent(lv_event_t* event) {
    static_cast<MenuHost*>(lv_event_get_user_data(event))->selectTapped_ = true;
}

}  // namespace m5_redux
