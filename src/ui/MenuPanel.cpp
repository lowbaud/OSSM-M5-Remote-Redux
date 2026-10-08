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
// Space between the panel and the screen edge, and between the panel and the button bar.
constexpr std::int32_t kPanelMargin = 4;
constexpr std::int32_t kPanelRadius = 10;
// The control screen shows through the panel, darkened.
constexpr lv_opa_t kPanelOpacity = LV_OPA_80;
// The full-screen list matches the old settings screen.
constexpr std::int32_t kScreenListBarGap = 7;
constexpr std::int32_t kScreenListPadX = 10;
constexpr std::int32_t kScreenListPadTop = 6;
constexpr std::int32_t kTitleHeight = 28;
constexpr std::int32_t kTitlePadX = 12;
constexpr std::int32_t kPopupWidth = 263;
constexpr std::int32_t kPopupOffsetY = -20;
constexpr std::int32_t kPopupBorderWidth = 1;
// The popup grows with its rows up to this many, then scrolls.
constexpr std::size_t kMaxVisiblePopupRows = 4;
constexpr lv_opa_t kPopupScrimOpacity = LV_OPA_50;
constexpr std::int32_t kSaveFailureWidth = 236;
constexpr std::uint32_t kSlideDurationMs = 180;
constexpr std::uint32_t kSaveFailureDurationMs = 2000;

lv_obj_t* createPlainObject(lv_obj_t* parent) {
    lv_obj_t* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

lv_obj_t* createScrollArea(lv_obj_t* parent) {
    lv_obj_t* area = createPlainObject(parent);
    lv_obj_add_flag(area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(area, LV_DIR_VER);
    return area;
}

void setHidden(lv_obj_t* object, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
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

    // Taps beside the panel close the menu; the screen behind stays as it is.
    lv_obj_t* closeArea = createPlainObject(root_);
    lv_obj_set_size(closeArea, lv_pct(100), lv_pct(100));
    lv_obj_add_event_cb(closeArea, handleCloseEvent, LV_EVENT_CLICKED, this);

    const std::int32_t buttonBarTop =
        lv_obj_get_style_y(objects.ossm_control_settings_btn, LV_PART_MAIN);
    buildPanel(buttonBarTop);
    buildScreen(buttonBarTop);
    buildPopup();
    buildButtonBar();
    buildSaveFailureLabel();
    setStopAvailable(false);
}

void MenuPanel::open(MenuPage& root) {
    controller_.open(root);
    clearTaps();
    hideSaveFailure();
    visible_ = true;
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    renderAll();
}

void MenuPanel::close() {
    controller_.close();
    controller_.takeChanges();
    clearTaps();
    hideSaveFailure();
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
    if (!visible_) {
        return;
    }

    visible_ = false;
    // Only the panel slides; the full-screen page goes at once, like a screen change.
    if (panelIn_ && lv_obj_has_flag(screen_, LV_OBJ_FLAG_HIDDEN)) {
        slideOut();
    } else {
        hideNow();
    }
}

void MenuPanel::hide() {
    visible_ = false;
    clearTaps();
    hideSaveFailure();
    hideNow();
}

void MenuPanel::show() {
    if (!controller_.isOpen()) {
        return;
    }

    controller_.refresh();
    clearTaps();
    visible_ = true;
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
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

void MenuPanel::buildPanel(std::int32_t buttonBarTop) {
    const std::int32_t screenHeight = lv_display_get_vertical_resolution(nullptr);
    const std::int32_t maxHeight = buttonBarTop - 2 * kPanelMargin;
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    // Fits its tiles and grows upwards from just above the button bar; starts off screen.
    panel_ = createPlainObject(root_);
    lv_obj_set_size(panel_, kPanelWidth, LV_SIZE_CONTENT);
    lv_obj_set_style_max_height(panel_, maxHeight, mainStyle);
    lv_obj_align(
        panel_, LV_ALIGN_BOTTOM_LEFT, -kPanelWidth, -(screenHeight - buttonBarTop + kPanelMargin));
    lv_obj_set_style_bg_color(panel_, themeColor(COLOR_ID_BACKGROUND), mainStyle);
    lv_obj_set_style_bg_opa(panel_, kPanelOpacity, mainStyle);
    lv_obj_set_style_border_color(panel_, themeColor(COLOR_ID_BORDER), mainStyle);
    lv_obj_set_style_border_opa(panel_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(panel_, 1, mainStyle);
    lv_obj_set_style_radius(panel_, kPanelRadius, mainStyle);

    panelPage_.tiles = true;
    panelPage_.container = createScrollArea(panel_);
    lv_obj_set_size(panelPage_.container, lv_pct(100), LV_SIZE_CONTENT);
    // Inside the panel border.
    lv_obj_set_style_max_height(panelPage_.container, maxHeight - 2, mainStyle);
    styleMenuTileGrid(panelPage_.container);
}

void MenuPanel::buildScreen(std::int32_t buttonBarTop) {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    screen_ = createPlainObject(root_);
    lv_obj_set_size(screen_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(screen_, themeColor(COLOR_ID_BACKGROUND), mainStyle);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, mainStyle);
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);

    screenPage_.container = createScrollArea(screen_);
    lv_obj_set_pos(screenPage_.container, 0, 0);
    lv_obj_set_size(screenPage_.container, lv_pct(100), buttonBarTop - kScreenListBarGap);
    addSharedListStyles(screenPage_.container);
    lv_obj_set_style_pad_left(screenPage_.container, kScreenListPadX, mainStyle);
    lv_obj_set_style_pad_right(screenPage_.container, kScreenListPadX, mainStyle);
    lv_obj_set_style_pad_top(screenPage_.container, kScreenListPadTop, mainStyle);
}

void MenuPanel::buildPopup() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;
    const lv_font_t* titleFont = &lv_font_montserrat_14;

    popupScrim_ = createPlainObject(root_);
    lv_obj_set_size(popupScrim_, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(popupScrim_, lv_color_black(), mainStyle);
    lv_obj_set_style_bg_opa(popupScrim_, kPopupScrimOpacity, mainStyle);
    lv_obj_add_event_cb(popupScrim_, handleBackEvent, LV_EVENT_CLICKED, this);
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);

    popup_ = createPlainObject(popupScrim_);
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
    popupPage_.title = title;
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
    lv_label_set_text_static(title, popupPage_.titleText);

    popupPage_.container = createScrollArea(popup_);
    lv_obj_set_pos(popupPage_.container, 0, kTitleHeight);
    lv_obj_set_width(popupPage_.container, lv_pct(100));
    addSharedListStyles(popupPage_.container);
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

void MenuPanel::clearTaps() {
    tappedRow_ = MenuPage::kNoItem;
    backTapped_ = false;
    selectTapped_ = false;
    closeTapped_ = false;
}

void MenuPanel::slideIn() {
    lv_anim_delete(panel_, setPanelX);
    panelIn_ = true;
    animate(panel_, setPanelX, lv_obj_get_style_x(panel_, LV_PART_MAIN), kPanelMargin);
}

void MenuPanel::slideOut() {
    lv_anim_delete(panel_, setPanelX);
    panelIn_ = false;
    animate(
        panel_,
        setPanelX,
        lv_obj_get_style_x(panel_, LV_PART_MAIN),
        -kPanelWidth,
        handleSlideOutCompleted,
        this);
}

void MenuPanel::movePanelIn() {
    lv_anim_delete(panel_, setPanelX);
    panelIn_ = true;
    lv_obj_set_x(panel_, kPanelMargin);
}

void MenuPanel::hideNow() {
    lv_anim_delete(panel_, setPanelX);
    panelIn_ = false;
    lv_obj_set_x(panel_, -kPanelWidth);
    lv_obj_add_flag(screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(popupScrim_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
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
    const std::size_t base = baseLevel();
    if (changes.pages) {
        // The quick actions stay in the panel while a full-screen page covers them.
        const bool screenShown = !drawsTiles(base);
        const std::size_t panelLevel = screenShown ? 0 : base;
        const bool panelShown = drawsTiles(panelLevel);
        if (panelShown) {
            showPage(panelPage_, panelLevel);
        }
        if (screenShown) {
            showPage(screenPage_, base);
        }
        setHidden(screen_, !screenShown);

        if (panelShown && !panelIn_) {
            if (screenShown) {
                movePanelIn();
            } else {
                slideIn();
            }
        } else if (!panelShown && panelIn_) {
            lv_anim_delete(panel_, setPanelX);
            panelIn_ = false;
            lv_obj_set_x(panel_, -kPanelWidth);
        }

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

std::size_t MenuPanel::baseLevel() const {
    std::size_t level = controller_.depth() - 1;
    while (level > 0 && controller_.isPopup(level)) {
        --level;
    }
    return level;
}

bool MenuPanel::drawsTiles(std::size_t level) const {
    return !controller_.isPopup(level) &&
           controller_.page(level).layout() == MenuPage::Layout::Tiles;
}

MenuPanel::PageView& MenuPanel::pageView(std::size_t level) {
    if (level > baseLevel()) {
        return popupPage_;
    }
    return drawsTiles(level) ? panelPage_ : screenPage_;
}

void MenuPanel::showPage(PageView& view, std::size_t level) {
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

void MenuPanel::updateHighlight(std::size_t level) {
    const PageView& view = pageView(level);
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

void MenuPanel::updateValues(std::size_t level) {
    const PageView& view = pageView(level);
    const MenuPage& page = controller_.page(level);
    for (std::size_t index = 0; index < page.count(); ++index) {
        const MenuItemView* itemView = findView(page.item(index), view.tiles);
        if (itemView != nullptr) {
            refreshMenuItemView(*itemView);
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
    lv_obj_set_height(popupPage_.container, listHeight);
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

MenuItemView* MenuPanel::viewFor(MenuItem& item, lv_obj_t* container, bool tile) {
    MenuItemView* existing = findView(item, tile);
    if (existing != nullptr) {
        // List rows move between the full-screen page and the popup.
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
        // The grid fills the panel inside its border.
        createMenuTile(view, container, menuTileWidth(kPanelWidth - 2));
    } else {
        createMenuRow(view, container);
    }
    lv_obj_add_event_cb(view.root, handleViewEvent, LV_EVENT_CLICKED, this);
    return &view;
}

MenuItemView* MenuPanel::findView(const MenuItem& item, bool tile) {
    for (std::size_t index = 0; index < viewCount_; ++index) {
        if (views_[index].item == &item && views_[index].tile == tile) {
            return &views_[index];
        }
    }
    return nullptr;
}

void MenuPanel::handleViewClicked(lv_obj_t* root) {
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

void MenuPanel::setPanelX(void* panel, std::int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(panel), x);
}

void MenuPanel::handleSlideOutCompleted(lv_anim_t* animation) {
    auto* panel = static_cast<MenuPanel*>(lv_anim_get_user_data(animation));
    lv_obj_add_flag(panel->root_, LV_OBJ_FLAG_HIDDEN);
}

void MenuPanel::handleViewEvent(lv_event_t* event) {
    auto* panel = static_cast<MenuPanel*>(lv_event_get_user_data(event));
    panel->handleViewClicked(static_cast<lv_obj_t*>(lv_event_get_current_target(event)));
}

void MenuPanel::handleBackEvent(lv_event_t* event) {
    static_cast<MenuPanel*>(lv_event_get_user_data(event))->backTapped_ = true;
}

void MenuPanel::handleSelectEvent(lv_event_t* event) {
    static_cast<MenuPanel*>(lv_event_get_user_data(event))->selectTapped_ = true;
}

void MenuPanel::handleCloseEvent(lv_event_t* event) {
    static_cast<MenuPanel*>(lv_event_get_user_data(event))->closeTapped_ = true;
}

}  // namespace m5_redux
