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
constexpr std::int32_t kRowColumnGap = 8;
constexpr std::int32_t kLevelWidth = 72;
constexpr std::int32_t kLevelHeight = 8;
const lv_font_t* const kRowFont = &lv_font_montserrat_16;

struct SharedStyles {
    lv_style_t list;
    lv_style_t scrollbar;
    lv_style_t row;
    lv_style_t rowSelected;
    lv_style_t name;
    lv_style_t value;
    lv_style_t valueSelected;
    lv_style_t accentValue;
    lv_style_t level;
    lv_style_t levelIndicator;
};

void initSharedStyles(SharedStyles& styles) {
    lv_style_init(&styles.list);
    lv_style_set_pad_all(&styles.list, 0);
    lv_style_set_pad_row(&styles.list, 0);
    lv_style_set_layout(&styles.list, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&styles.list, LV_FLEX_FLOW_COLUMN);

    lv_style_init(&styles.scrollbar);
    lv_style_set_width(&styles.scrollbar, kScrollbarWidth);
    lv_style_set_pad_right(&styles.scrollbar, kScrollbarEdgeGap);
    lv_style_set_bg_color(&styles.scrollbar, themeColor(COLOR_ID_SCROLLBAR));
    lv_style_set_bg_opa(&styles.scrollbar, LV_OPA_COVER);
    lv_style_set_radius(&styles.scrollbar, LV_RADIUS_CIRCLE);

    lv_style_init(&styles.row);
    lv_style_set_width(&styles.row, lv_pct(100));
    lv_style_set_height(&styles.row, LV_SIZE_CONTENT);
    lv_style_set_layout(&styles.row, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&styles.row, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_cross_place(&styles.row, LV_FLEX_ALIGN_CENTER);
    lv_style_set_pad_column(&styles.row, kRowColumnGap);
    lv_style_set_bg_opa(&styles.row, LV_OPA_TRANSP);
    lv_style_set_text_font(&styles.row, kRowFont);
    lv_style_set_text_color(&styles.row, themeColor(COLOR_ID_TEXT_PRIMARY));
    lv_style_set_pad_left(&styles.row, kRowPadX);
    lv_style_set_pad_right(&styles.row, kRowPadX);
    lv_style_set_pad_top(&styles.row, kRowPadY);
    lv_style_set_pad_bottom(&styles.row, kRowPadY);
    lv_style_set_border_side(&styles.row, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&styles.row, 1);
    lv_style_set_border_color(&styles.row, themeColor(COLOR_ID_DIVIDER));
    lv_style_set_border_opa(&styles.row, LV_OPA_COVER);

    // Same accent bar and padding compensation as styleListRow().
    lv_style_init(&styles.rowSelected);
    lv_style_set_bg_color(&styles.rowSelected, themeColor(COLOR_ID_ROW_SELECTED));
    lv_style_set_bg_opa(&styles.rowSelected, LV_OPA_COVER);
    lv_style_set_text_color(&styles.rowSelected, lv_color_white());
    lv_style_set_border_side(&styles.rowSelected, LV_BORDER_SIDE_LEFT);
    lv_style_set_border_width(&styles.rowSelected, kAccentWidth);
    lv_style_set_border_color(&styles.rowSelected, themeColor(COLOR_ID_ACCENT));
    lv_style_set_pad_left(&styles.rowSelected, kRowPadX - kAccentWidth);
    lv_style_set_pad_bottom(&styles.rowSelected, kRowPadY + 1);

    lv_style_init(&styles.name);
    lv_style_set_flex_grow(&styles.name, 1);

    lv_style_init(&styles.value);
    lv_style_set_text_color(&styles.value, themeColor(COLOR_ID_TEXT_SECONDARY));

    lv_style_init(&styles.valueSelected);
    lv_style_set_text_color(&styles.valueSelected, themeColor(COLOR_ID_ACCENT));

    lv_style_init(&styles.accentValue);
    lv_style_set_text_color(&styles.accentValue, themeColor(COLOR_ID_ACCENT));

    lv_style_init(&styles.level);
    lv_style_set_width(&styles.level, kLevelWidth);
    lv_style_set_height(&styles.level, kLevelHeight);
    lv_style_set_radius(&styles.level, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&styles.level, themeColor(COLOR_ID_DIVIDER));
    lv_style_set_bg_opa(&styles.level, LV_OPA_COVER);

    lv_style_init(&styles.levelIndicator);
    lv_style_set_radius(&styles.levelIndicator, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(&styles.levelIndicator, themeColor(COLOR_ID_ACCENT));
    lv_style_set_bg_opa(&styles.levelIndicator, LV_OPA_COVER);
}

SharedStyles& sharedStyles() {
    static SharedStyles styles;
    static bool initialized = false;
    if (!initialized) {
        initSharedStyles(styles);
        initialized = true;
    }
    return styles;
}

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

void addSharedListStyles(lv_obj_t* list) {
    SharedStyles& styles = sharedStyles();
    lv_obj_add_style(list, &styles.list, LV_PART_MAIN);
    lv_obj_add_style(list, &styles.scrollbar, LV_PART_SCROLLBAR);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
}

void addSharedListRowStyles(lv_obj_t* row) {
    SharedStyles& styles = sharedStyles();
    lv_obj_add_style(row, &styles.row, LV_PART_MAIN);
    lv_obj_add_style(row, &styles.rowSelected, LV_STATE_CHECKED);
}

void addSharedListRowNameStyles(lv_obj_t* label) {
    lv_obj_add_style(label, &sharedStyles().name, LV_PART_MAIN);
}

void addSharedListRowValueStyles(lv_obj_t* label, bool accent) {
    SharedStyles& styles = sharedStyles();
    if (accent) {
        lv_obj_add_style(label, &styles.accentValue, LV_PART_MAIN);
        return;
    }
    lv_obj_add_style(label, &styles.value, LV_PART_MAIN);
    lv_obj_add_style(label, &styles.valueSelected, LV_STATE_CHECKED);
}

void addSharedListRowLevelStyles(lv_obj_t* bar) {
    SharedStyles& styles = sharedStyles();
    lv_obj_add_style(bar, &styles.level, LV_PART_MAIN);
    lv_obj_add_style(bar, &styles.levelIndicator, LV_PART_INDICATOR);
}

}  // namespace m5_redux
