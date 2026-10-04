#include "ListRowStyle.h"

#include <cstdint>

#include "ui/ThemeColors.h"

namespace m5_redux {

namespace {

constexpr std::int32_t kRowPadX = 12;
constexpr std::int32_t kRowPadY = 6;
constexpr std::int32_t kAccentWidth = 4;
constexpr std::int32_t kScrollbarWidth = 3;
constexpr std::int32_t kScrollbarEdgeGap = 2;
const lv_font_t* const kRowFont = &lv_font_montserrat_16;

}  // namespace

void styleListRow(lv_obj_t* row) {
    const lv_style_selector_t normal = 0;
    const lv_style_selector_t selected = LV_STATE_CHECKED;

    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, normal);
    lv_obj_set_style_radius(row, 0, normal);
    lv_obj_set_style_text_font(row, kRowFont, normal);
    lv_obj_set_style_text_color(row, themeColor(COLOR_ID_TEXT_PRIMARY), normal);
    lv_obj_set_style_pad_left(row, kRowPadX, normal);
    lv_obj_set_style_pad_right(row, kRowPadX, normal);
    lv_obj_set_style_pad_top(row, kRowPadY, normal);
    lv_obj_set_style_pad_bottom(row, kRowPadY, normal);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, normal);
    lv_obj_set_style_border_width(row, 1, normal);
    lv_obj_set_style_border_color(row, themeColor(COLOR_ID_DIVIDER), normal);
    lv_obj_set_style_border_opa(row, LV_OPA_COVER, normal);
    lv_obj_set_style_transform_width(row, 0, LV_STATE_PRESSED);

    // Swap the divider for the accent bar and compensate the padding so the content
    // stays put; LVGL counts the border width only on the sides it is drawn.
    lv_obj_set_style_bg_color(row, themeColor(COLOR_ID_ROW_SELECTED), selected);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, selected);
    lv_obj_set_style_text_color(row, lv_color_white(), selected);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_LEFT, selected);
    lv_obj_set_style_border_width(row, kAccentWidth, selected);
    lv_obj_set_style_border_color(row, themeColor(COLOR_ID_ACCENT), selected);
    lv_obj_set_style_pad_left(row, kRowPadX - kAccentWidth, selected);
    lv_obj_set_style_pad_bottom(row, kRowPadY + 1, selected);
}

std::int32_t listRowHeight() {
    // Padding on both sides plus the one-pixel divider.
    return lv_font_get_line_height(kRowFont) + 2 * kRowPadY + 1;
}

void styleList(lv_obj_t* list) {
    lv_obj_set_style_pad_row(list, 0, LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(list, kScrollbarWidth, LV_PART_SCROLLBAR);
    lv_obj_set_style_pad_right(list, kScrollbarEdgeGap, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(list, themeColor(COLOR_ID_SCROLLBAR), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(list, LV_RADIUS_CIRCLE, LV_PART_SCROLLBAR);
}

lv_obj_t* addListRowName(lv_obj_t* row, const char* text) {
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_flex_grow(label, 1);
    return label;
}

lv_obj_t* addListRowValue(lv_obj_t* row, const char* text) {
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, themeColor(COLOR_ID_TEXT_SECONDARY), 0);
    return label;
}

void setListRowSelected(lv_obj_t* row, bool selected) {
    const std::uint32_t childCount = lv_obj_get_child_count(row);
    if (selected) {
        lv_obj_add_state(row, LV_STATE_CHECKED);
        for (std::uint32_t index = 0; index < childCount; ++index) {
            lv_obj_add_state(lv_obj_get_child(row, index), LV_STATE_CHECKED);
        }
    } else {
        lv_obj_remove_state(row, LV_STATE_CHECKED);
        for (std::uint32_t index = 0; index < childCount; ++index) {
            lv_obj_remove_state(lv_obj_get_child(row, index), LV_STATE_CHECKED);
        }
    }
}

}  // namespace m5_redux
