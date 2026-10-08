#include "MenuScreen.h"

#include <cstdint>

#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"
#include "ui/generated/screens.h"

namespace m5_redux {

namespace {

// The list matches the old settings screen.
constexpr std::int32_t kListBarGap = 7;
constexpr std::int32_t kListPadX = 10;
constexpr std::int32_t kListPadTop = 6;
// Matches the screen changes of the EEZ screens.
constexpr std::uint32_t kScreenFadeMs = 200;

}  // namespace

void MenuScreen::begin() {
    const lv_style_selector_t mainStyle = LV_PART_MAIN;

    screen_ = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen_);
    lv_obj_remove_flag(screen_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen_, themeColor(COLOR_ID_BACKGROUND), mainStyle);
    lv_obj_set_style_bg_opa(screen_, LV_OPA_COVER, mainStyle);

    const std::int32_t buttonBarTop =
        lv_obj_get_style_y(objects.ossm_control_settings_btn, LV_PART_MAIN);
    lv_obj_t* list = createMenuScrollArea(screen_);
    lv_obj_set_pos(list, 0, 0);
    lv_obj_set_size(list, lv_pct(100), buttonBarTop - kListBarGap);
    addSharedListStyles(list);
    lv_obj_set_style_pad_left(list, kListPadX, mainStyle);
    lv_obj_set_style_pad_right(list, kListPadX, mainStyle);
    lv_obj_set_style_pad_top(list, kListPadTop, mainStyle);

    PageLayout layout;
    layout.container = list;
    buildChrome(screen_, layout);
}

void MenuScreen::open(MenuPage& root) {
    openPages(root);
    load();
}

void MenuScreen::resume() {
    refreshPages();
    load();
}

void MenuScreen::close() {
    closePages();
}

void MenuScreen::load() {
    if (lv_screen_active() != screen_) {
        lv_screen_load_anim(screen_, LV_SCREEN_LOAD_ANIM_FADE_IN, kScreenFadeMs, 0, false);
    }
}

}  // namespace m5_redux
