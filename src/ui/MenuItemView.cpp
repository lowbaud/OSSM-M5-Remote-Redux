#include "MenuItemView.h"

#include <cstdint>

#include "menu/MenuItems.h"
#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"
#include "ui/ValueHighlight.h"

namespace m5_redux {

namespace {

constexpr std::int32_t kTileGap = 6;
// Lowers the tiles from the top of the screen; pages without tiles start higher.
constexpr std::int32_t kTileRowGapAbove = 8;
constexpr std::int32_t kTileRowGapBelow = 8;
constexpr std::int32_t kTileHeight = 48;
constexpr std::int32_t kTileIconGap = 4;
constexpr std::int32_t kTilePadX = 4;
constexpr std::int32_t kTileRadius = 8;
constexpr std::int32_t kSelectedBorderWidth = 2;

struct TileStyles {
    lv_style_t row;
    lv_style_t tile;
    lv_style_t tileSelected;
    lv_style_t icon;
    lv_style_t name;
    lv_style_t badge;
};

void initTileStyles(TileStyles& styles) {
    // Equal tiles across the list.
    lv_style_init(&styles.row);
    lv_style_set_width(&styles.row, lv_pct(100));
    lv_style_set_height(&styles.row, LV_SIZE_CONTENT);
    lv_style_set_layout(&styles.row, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&styles.row, LV_FLEX_FLOW_ROW);
    lv_style_set_pad_column(&styles.row, kTileGap);
    lv_style_set_pad_top(&styles.row, kTileRowGapAbove);
    lv_style_set_pad_bottom(&styles.row, kTileRowGapBelow);

    // Icon above the name, both centered.
    lv_style_init(&styles.tile);
    lv_style_set_flex_grow(&styles.tile, 1);
    lv_style_set_height(&styles.tile, kTileHeight);
    lv_style_set_layout(&styles.tile, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&styles.tile, LV_FLEX_FLOW_COLUMN);
    lv_style_set_flex_main_place(&styles.tile, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_cross_place(&styles.tile, LV_FLEX_ALIGN_CENTER);
    lv_style_set_pad_row(&styles.tile, kTileIconGap);
    lv_style_set_pad_left(&styles.tile, kTilePadX);
    lv_style_set_pad_right(&styles.tile, kTilePadX);
    lv_style_set_bg_color(&styles.tile, themeColor(COLOR_ID_SURFACE));
    lv_style_set_bg_opa(&styles.tile, LV_OPA_COVER);
    lv_style_set_border_color(&styles.tile, themeColor(COLOR_ID_SCROLLBAR));
    lv_style_set_border_opa(&styles.tile, LV_OPA_COVER);
    lv_style_set_border_width(&styles.tile, 1);
    lv_style_set_radius(&styles.tile, kTileRadius);
    lv_style_set_text_font(&styles.tile, &lv_font_montserrat_12);
    lv_style_set_text_color(&styles.tile, themeColor(COLOR_ID_TEXT_PRIMARY));

    // A thicker accent border; the padding shrinks with it so the content stays put.
    lv_style_init(&styles.tileSelected);
    lv_style_set_bg_color(&styles.tileSelected, themeColor(COLOR_ID_ROW_SELECTED));
    lv_style_set_border_color(&styles.tileSelected, themeColor(COLOR_ID_ACCENT));
    lv_style_set_border_width(&styles.tileSelected, kSelectedBorderWidth);
    lv_style_set_pad_left(&styles.tileSelected, kTilePadX - (kSelectedBorderWidth - 1));
    lv_style_set_pad_right(&styles.tileSelected, kTilePadX - (kSelectedBorderWidth - 1));

    lv_style_init(&styles.icon);
    lv_style_set_text_font(&styles.icon, &lv_font_montserrat_16);

    // One line high, so long names end in dots instead of wrapping out of the tile.
    lv_style_init(&styles.name);
    lv_style_set_width(&styles.name, lv_pct(100));
    lv_style_set_height(&styles.name, lv_font_get_line_height(&lv_font_montserrat_12));
    lv_style_set_text_align(&styles.name, LV_TEXT_ALIGN_CENTER);

    // Plain accent text in the top right corner of the content.
    lv_style_init(&styles.badge);
    lv_style_set_align(&styles.badge, LV_ALIGN_TOP_RIGHT);
    lv_style_set_text_color(&styles.badge, themeColor(COLOR_ID_ACCENT));
}

TileStyles& tileStyles() {
    static TileStyles styles;
    static bool initialized = false;
    if (!initialized) {
        initTileStyles(styles);
        initialized = true;
    }
    return styles;
}

// A plain object without theme styles or scrolling.
lv_obj_t* createMenuObject(lv_obj_t* parent) {
    lv_obj_t* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

lv_obj_t* createLevel(lv_obj_t* parent) {
    lv_obj_t* level = lv_bar_create(parent);
    lv_obj_remove_style_all(level);
    addSharedListRowLevelStyles(level);
    // Taps on the bar select the row.
    lv_obj_remove_flag(level, LV_OBJ_FLAG_CLICKABLE);
    return level;
}

bool showsLevel(const MenuItem& item) {
    return item.kind() == MenuItemKind::Choice &&
           static_cast<const MenuChoiceItem&>(item).showsLevel();
}

void setHidden(lv_obj_t* object, bool hidden) {
    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

// Setting the same static text again would still redraw the label.
void setStaticText(lv_obj_t* label, const char* text) {
    if (lv_label_get_text(label) != text) {
        lv_label_set_text_static(label, text);
    }
}

// Fills the tile with the accent color at the peak of the pulse, from the background it has
// highlighted or not. A style for a more specific state outranks a local style for the default
// state, so the highlighted background gets a local style of its own; otherwise the highlight
// would hide the pulse.
void setTilePulse(void* object, std::int32_t strength) {
    lv_obj_t* tile = static_cast<lv_obj_t*>(object);
    const lv_color_t accent = themeColor(COLOR_ID_ACCENT);
    const lv_opa_t mix = static_cast<lv_opa_t>(strength);
    lv_obj_set_style_bg_color(
        tile, lv_color_mix(accent, themeColor(COLOR_ID_SURFACE), mix), LV_PART_MAIN);
    lv_obj_set_style_bg_color(
        tile, lv_color_mix(accent, themeColor(COLOR_ID_ROW_SELECTED), mix), LV_STATE_CHECKED);
}

void setTilePulsing(lv_obj_t* tile, bool pulsing) {
    const bool pulsingNow = lv_anim_get(tile, setTilePulse) != nullptr;
    if (pulsing == pulsingNow) {
        return;
    }
    if (pulsing) {
        startAccentPulse(tile, setTilePulse);
        return;
    }
    lv_anim_delete(tile, setTilePulse);
    lv_obj_remove_local_style_prop(tile, LV_STYLE_BG_COLOR, LV_PART_MAIN);
    lv_obj_remove_local_style_prop(tile, LV_STYLE_BG_COLOR, LV_STATE_CHECKED);
}

// Also runs whenever a quick action starts or ends, so unchanged parts are left alone.
void refreshTile(const MenuItemView& view) {
    const MenuItem& item = *view.item;
    setStaticText(view.name, item.name());
    setStaticText(view.icon, item.icon());
    setHidden(view.icon, item.icon()[0] == '\0');
    setTilePulsing(view.root, item.running());
}

}  // namespace

lv_obj_t* createMenuTileRow(lv_obj_t* list) {
    lv_obj_t* row = createMenuObject(list);
    lv_obj_add_style(row, &tileStyles().row, LV_PART_MAIN);
    // Taps between the tiles do nothing.
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    return row;
}

void createMenuRow(MenuItemView& view, lv_obj_t* list) {
    view.tile = false;
    view.container = list;
    view.root = createMenuObject(list);
    addSharedListRowStyles(view.root);
    view.name = lv_label_create(view.root);
    addSharedListRowNameStyles(view.name);
    lv_label_set_long_mode(view.name, LV_LABEL_LONG_MODE_DOTS);

    if (showsLevel(*view.item)) {
        view.level = createLevel(view.root);
        return;
    }
    view.value = lv_label_create(view.root);
    addSharedListRowValueStyles(view.value, view.item->kind() == MenuItemKind::Option);
}

void createMenuTile(MenuItemView& view, lv_obj_t* tileRow) {
    TileStyles& styles = tileStyles();
    view.tile = true;
    view.container = tileRow;
    view.root = createMenuObject(tileRow);
    lv_obj_add_style(view.root, &styles.tile, LV_PART_MAIN);
    lv_obj_add_style(view.root, &styles.tileSelected, LV_STATE_CHECKED);

    view.icon = lv_label_create(view.root);
    lv_obj_add_style(view.icon, &styles.icon, LV_PART_MAIN);
    view.name = lv_label_create(view.root);
    lv_obj_add_style(view.name, &styles.name, LV_PART_MAIN);
    lv_label_set_long_mode(view.name, LV_LABEL_LONG_MODE_DOTS);

    // In the corner, outside the layout that centers the icon and name.
    view.badge = lv_label_create(view.root);
    lv_obj_add_style(view.badge, &styles.badge, LV_PART_MAIN);
    lv_obj_add_flag(view.badge, LV_OBJ_FLAG_IGNORE_LAYOUT);
    setMenuTileBadge(view, "");
}

void refreshMenuItemView(const MenuItemView& view) {
    if (view.tile) {
        refreshTile(view);
        return;
    }

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
        setHidden(view.value, !static_cast<const MenuOptionItem&>(item).stored());
        return;
    }

    lv_label_set_text_static(view.value, item.showsChevron() ? LV_SYMBOL_RIGHT : item.valueText());
}

void setMenuTileBadge(const MenuItemView& view, const char* badge) {
    setStaticText(view.badge, badge);
    setHidden(view.badge, badge[0] == '\0');
}

void setMenuItemViewSelected(const MenuItemView& view, bool selected) {
    if (!view.tile) {
        setListRowSelected(view.root, selected);
        return;
    }
    if (selected) {
        lv_obj_add_state(view.root, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(view.root, LV_STATE_CHECKED);
    }
}

}  // namespace m5_redux
