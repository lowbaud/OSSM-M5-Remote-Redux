#include "QuickMenuPanel.h"

#include "ui/ThemeColors.h"
#include "ui/generated/screens.h"

namespace m5_redux {

namespace {

constexpr std::int32_t kPanelWidth = 250;
// Space between the bottom of the panel and the button bar.
constexpr std::int32_t kButtonBarGap = 4;
// The control screen shows through the panel, darkened.
constexpr lv_opa_t kPanelOpacity = LV_OPA_80;
constexpr std::uint32_t kSlideDurationMs = 180;

void animatePanelX(
    lv_obj_t* panel,
    std::int32_t to,
    lv_anim_exec_xcb_t exec,
    lv_anim_completed_cb_t completed,
    void* userData) {
    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, panel);
    lv_anim_set_exec_cb(&animation, exec);
    lv_anim_set_values(&animation, lv_obj_get_style_x(panel, LV_PART_MAIN), to);
    lv_anim_set_duration(&animation, kSlideDurationMs);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, completed);
    lv_anim_set_user_data(&animation, userData);
    lv_anim_start(&animation);
}

}  // namespace

void QuickMenuPanel::begin() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    root_ = createMenuObject(lv_layer_top());
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);

    // Taps beside the panel close it; the control screen stays as it is.
    lv_obj_t* closeArea = createMenuObject(root_);
    lv_obj_set_size(closeArea, lv_pct(100), lv_pct(100));
    lv_obj_add_event_cb(closeArea, handleCloseEvent, LV_EVENT_CLICKED, this);

    // Full height down to just above the button bar; starts off screen.
    const std::int32_t buttonBarTop =
        lv_obj_get_style_y(objects.ossm_control_settings_btn, LV_PART_MAIN);
    panel_ = createMenuObject(root_);
    lv_obj_set_pos(panel_, -kPanelWidth, 0);
    lv_obj_set_size(panel_, kPanelWidth, buttonBarTop - kButtonBarGap);
    lv_obj_set_style_bg_color(panel_, themeColor(COLOR_ID_BACKGROUND), mainStyle);
    lv_obj_set_style_bg_opa(panel_, kPanelOpacity, mainStyle);
    // Only the edge towards the control screen.
    lv_obj_set_style_border_color(panel_, themeColor(COLOR_ID_BORDER), mainStyle);
    lv_obj_set_style_border_opa(panel_, LV_OPA_COVER, mainStyle);
    lv_obj_set_style_border_width(panel_, 1, mainStyle);
    lv_obj_set_style_border_side(panel_, LV_BORDER_SIDE_RIGHT, mainStyle);

    lv_obj_t* grid = createMenuScrollArea(panel_);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
    styleMenuTileGrid(grid);

    PageLayout layout;
    layout.container = grid;
    layout.tiles = true;
    // The grid fills the panel inside its right border.
    layout.tileWidth = menuTileWidth(kPanelWidth - 1);
    layout.rootBackLabel = "Close";
    buildChrome(root_, layout);
}

void QuickMenuPanel::open(MenuPage& root) {
    // Visible before rendering, so scrolling to the highlighted tile sees the final layout.
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    openPages(root);
    slideIn();
}

void QuickMenuPanel::close(bool animated) {
    closePages();
    if (!shown_) {
        return;
    }
    if (animated) {
        slideOut();
    } else {
        hideNow();
    }
}

void QuickMenuPanel::pagesClosed() {
    if (shown_) {
        slideOut();
    }
}

void QuickMenuPanel::slideIn() {
    lv_anim_delete(panel_, setPanelX);
    shown_ = true;
    animatePanelX(panel_, 0, setPanelX, nullptr, nullptr);
}

void QuickMenuPanel::slideOut() {
    lv_anim_delete(panel_, setPanelX);
    shown_ = false;
    animatePanelX(panel_, -kPanelWidth, setPanelX, handleSlideOutCompleted, this);
}

void QuickMenuPanel::hideNow() {
    lv_anim_delete(panel_, setPanelX);
    shown_ = false;
    lv_obj_set_x(panel_, -kPanelWidth);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

void QuickMenuPanel::setPanelX(void* panel, std::int32_t x) {
    lv_obj_set_x(static_cast<lv_obj_t*>(panel), x);
}

void QuickMenuPanel::handleSlideOutCompleted(lv_anim_t* animation) {
    auto* menu = static_cast<QuickMenuPanel*>(lv_anim_get_user_data(animation));
    lv_obj_add_flag(menu->root_, LV_OBJ_FLAG_HIDDEN);
}

void QuickMenuPanel::handleCloseEvent(lv_event_t* event) {
    static_cast<QuickMenuPanel*>(lv_event_get_user_data(event))->requestClose();
}

}  // namespace m5_redux
