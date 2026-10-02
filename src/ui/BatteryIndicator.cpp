#include "BatteryIndicator.h"

#include "ui/generated/fonts.h"

namespace m5_redux {

namespace {

// Sized around the 7 px tall digits of the text font, leaving a 3 px margin
// above and below and a 1 px margin left and 2 px right of "100".
constexpr int32_t kBodyWidth = 25;
constexpr int32_t kBodyHeight = 13;
constexpr int32_t kBodyRadius = 3;
constexpr int32_t kNubGap = 1;
constexpr int32_t kNubWidth = 2;
constexpr int32_t kNubHeight = 5;

const lv_font_t* const kTextFont = &ui_font_ami_ega_8x8;

const lv_color_t kEmptyColor = lv_color_hex(0x666666);
const lv_color_t kTextDarkColor = lv_color_hex(0x000000);
const lv_color_t kTextLightColor = lv_color_hex(0xf2f2f2);
const lv_color_t kFillNormalColor = lv_color_hex(0xd9d9d9);
const lv_color_t kFillChargingColor = lv_color_hex(0x2aa248);
const lv_color_t kFillCriticalColor = lv_color_hex(0xff3b30);

lv_color_t fillColorFor(int percent, bool charging) {
    if (charging) {
        return kFillChargingColor;
    }
    if (percent < 15) {
        return kFillCriticalColor;
    }
    return kFillNormalColor;
}

lv_color_t textColorFor(bool charging) {
    return charging ? kTextLightColor : kTextDarkColor;
}

lv_obj_t* createPlainObject(lv_obj_t* parent) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

}  // namespace

void BatteryIndicator::begin(lv_obj_t* anchorLabel) {
    lv_obj_add_flag(anchorLabel, LV_OBJ_FLAG_HIDDEN);

    // Center the battery on the line the anchor label's font would occupy.
    const lv_font_t* anchorFont = lv_obj_get_style_text_font(anchorLabel, LV_PART_MAIN);
    const int32_t x = lv_obj_get_style_x(anchorLabel, LV_PART_MAIN);
    const int32_t y = lv_obj_get_style_y(anchorLabel, LV_PART_MAIN) +
                      (lv_font_get_line_height(anchorFont) - kBodyHeight) / 2;

    container_ = createPlainObject(lv_obj_get_parent(anchorLabel));
    lv_obj_set_pos(container_, x, y);
    lv_obj_set_size(container_, kBodyWidth + kNubGap + kNubWidth, kBodyHeight);
    lv_obj_add_flag(container_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* body = createPlainObject(container_);
    lv_obj_set_size(body, kBodyWidth, kBodyHeight);
    lv_obj_set_style_radius(body, kBodyRadius, LV_PART_MAIN);
    lv_obj_set_style_clip_corner(body, true, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(body, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(body, kEmptyColor, LV_PART_MAIN);

    fill_ = createPlainObject(body);
    lv_obj_set_height(fill_, kBodyHeight);
    lv_obj_set_style_bg_opa(fill_, LV_OPA_COVER, LV_PART_MAIN);

    // Created after the fill so the number draws on top of it.
    label_ = lv_label_create(body);
    lv_obj_set_width(label_, kBodyWidth);
    lv_obj_set_style_text_font(label_, kTextFont, LV_PART_MAIN);
    lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label_, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t* nub = createPlainObject(container_);
    lv_obj_set_pos(nub, kBodyWidth + kNubGap, (kBodyHeight - kNubHeight) / 2);
    lv_obj_set_size(nub, kNubWidth, kNubHeight);
    lv_obj_set_style_radius(nub, 1, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(nub, kEmptyColor, LV_PART_MAIN);
}

void BatteryIndicator::setLevel(int percent, bool charging) {
    if (percent < 0) {
        return;
    }
    if (percent > 100) {
        percent = 100;
    }

    lv_obj_set_width(fill_, (kBodyWidth * percent + 50) / 100);
    lv_obj_set_style_bg_color(fill_, fillColorFor(percent, charging), LV_PART_MAIN);
    lv_obj_set_style_text_color(label_, textColorFor(charging), LV_PART_MAIN);
    lv_label_set_text_fmt(label_, "%d", percent);
    lv_obj_remove_flag(container_, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace m5_redux
