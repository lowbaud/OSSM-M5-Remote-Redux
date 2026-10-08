#include "MenuItemView.h"

#include "menu/MenuItems.h"
#include "ui/ListRowStyle.h"
#include "ui/ThemeColors.h"

namespace m5_redux {

namespace {

constexpr std::int32_t kGridPad = 6;
constexpr std::int32_t kGridGap = 5;
constexpr std::int32_t kTileHeight = 44;
constexpr std::int32_t kWideTileHeight = 36;
constexpr std::int32_t kTilePadX = 8;
constexpr std::int32_t kTileRadius = 8;
constexpr std::int32_t kSelectedBorderWidth = 2;
constexpr std::int32_t kIconGap = 6;
// Lights the icon and value of a choice that is switched on.
constexpr lv_state_t kToggledOnState = LV_STATE_USER_1;

struct TileStyles {
    lv_style_t grid;
    lv_style_t tile;
    lv_style_t tileSelected;
    lv_style_t regular;
    lv_style_t wide;
    lv_style_t header;
    lv_style_t name;
    lv_style_t tileName;
    lv_style_t icon;
    lv_style_t value;
    lv_style_t toggledOn;
    lv_style_t level;
};

void initTileStyles(TileStyles& styles) {
    lv_style_init(&styles.grid);
    lv_style_set_pad_all(&styles.grid, kGridPad);
    lv_style_set_pad_row(&styles.grid, kGridGap);
    lv_style_set_pad_column(&styles.grid, kGridGap);
    lv_style_set_flex_flow(&styles.grid, LV_FLEX_FLOW_ROW_WRAP);

    lv_style_init(&styles.tile);
    lv_style_set_bg_color(&styles.tile, themeColor(COLOR_ID_SURFACE));
    lv_style_set_bg_opa(&styles.tile, LV_OPA_COVER);
    lv_style_set_border_color(&styles.tile, themeColor(COLOR_ID_SCROLLBAR));
    lv_style_set_border_opa(&styles.tile, LV_OPA_COVER);
    lv_style_set_border_width(&styles.tile, 1);
    lv_style_set_radius(&styles.tile, kTileRadius);
    lv_style_set_pad_left(&styles.tile, kTilePadX);
    lv_style_set_pad_right(&styles.tile, kTilePadX);
    lv_style_set_text_font(&styles.tile, &lv_font_montserrat_14);
    lv_style_set_text_color(&styles.tile, themeColor(COLOR_ID_TEXT_PRIMARY));
    lv_style_set_layout(&styles.tile, LV_LAYOUT_FLEX);

    // A thicker accent border; the padding shrinks with it so the content stays put.
    lv_style_init(&styles.tileSelected);
    lv_style_set_bg_color(&styles.tileSelected, themeColor(COLOR_ID_ROW_SELECTED));
    lv_style_set_border_color(&styles.tileSelected, themeColor(COLOR_ID_ACCENT));
    lv_style_set_border_width(&styles.tileSelected, kSelectedBorderWidth);
    lv_style_set_pad_left(&styles.tileSelected, kTilePadX - (kSelectedBorderWidth - 1));
    lv_style_set_pad_right(&styles.tileSelected, kTilePadX - (kSelectedBorderWidth - 1));

    // Icon and name on the first line, the value below.
    lv_style_init(&styles.regular);
    lv_style_set_height(&styles.regular, kTileHeight);
    lv_style_set_flex_flow(&styles.regular, LV_FLEX_FLOW_COLUMN);
    lv_style_set_flex_main_place(&styles.regular, LV_FLEX_ALIGN_CENTER);
    lv_style_set_pad_row(&styles.regular, 1);

    // Everything on one line.
    lv_style_init(&styles.wide);
    lv_style_set_width(&styles.wide, lv_pct(100));
    lv_style_set_height(&styles.wide, kWideTileHeight);
    lv_style_set_flex_flow(&styles.wide, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_cross_place(&styles.wide, LV_FLEX_ALIGN_CENTER);
    lv_style_set_flex_track_place(&styles.wide, LV_FLEX_ALIGN_CENTER);
    lv_style_set_pad_column(&styles.wide, kIconGap);

    lv_style_init(&styles.header);
    lv_style_set_width(&styles.header, lv_pct(100));
    lv_style_set_height(&styles.header, LV_SIZE_CONTENT);
    lv_style_set_layout(&styles.header, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(&styles.header, LV_FLEX_FLOW_ROW);
    lv_style_set_flex_cross_place(&styles.header, LV_FLEX_ALIGN_CENTER);
    lv_style_set_pad_column(&styles.header, kIconGap);

    lv_style_init(&styles.name);
    lv_style_set_flex_grow(&styles.name, 1);

    // One line high, so long names end in dots instead of wrapping out of the tile.
    lv_style_init(&styles.tileName);
    lv_style_set_height(&styles.tileName, lv_font_get_line_height(&lv_font_montserrat_14));

    lv_style_init(&styles.icon);
    lv_style_set_text_font(&styles.icon, &lv_font_montserrat_16);

    lv_style_init(&styles.value);
    lv_style_set_width(&styles.value, lv_pct(100));
    lv_style_set_height(&styles.value, lv_font_get_line_height(&lv_font_montserrat_12));
    lv_style_set_text_font(&styles.value, &lv_font_montserrat_12);
    lv_style_set_text_color(&styles.value, themeColor(COLOR_ID_TEXT_SECONDARY));

    lv_style_init(&styles.toggledOn);
    lv_style_set_text_color(&styles.toggledOn, themeColor(COLOR_ID_ACCENT));

    lv_style_init(&styles.level);
    lv_style_set_flex_grow(&styles.level, 1);
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

lv_obj_t* createPlainObject(lv_obj_t* parent) {
    lv_obj_t* object = lv_obj_create(parent);
    lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    return object;
}

lv_obj_t* createName(lv_obj_t* parent) {
    lv_obj_t* name = lv_label_create(parent);
    addSharedListRowNameStyles(name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_MODE_DOTS);
    return name;
}

lv_obj_t* createLevel(lv_obj_t* parent) {
    lv_obj_t* level = lv_bar_create(parent);
    lv_obj_remove_style_all(level);
    addSharedListRowLevelStyles(level);
    // Taps on the bar select the row.
    lv_obj_remove_flag(level, LV_OBJ_FLAG_CLICKABLE);
    return level;
}

lv_obj_t* createIcon(lv_obj_t* parent) {
    lv_obj_t* icon = lv_label_create(parent);
    TileStyles& styles = tileStyles();
    lv_obj_add_style(icon, &styles.icon, LV_PART_MAIN);
    lv_obj_add_style(icon, &styles.toggledOn, kToggledOnState);
    return icon;
}

bool showsLevel(const MenuItem& item) {
    return item.kind() == MenuItemKind::Choice &&
           static_cast<const MenuChoiceItem&>(item).showsLevel();
}

void setToggledOn(lv_obj_t* object, bool on) {
    if (object == nullptr) {
        return;
    }
    if (on) {
        lv_obj_add_state(object, kToggledOnState);
    } else {
        lv_obj_remove_state(object, kToggledOnState);
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

void styleMenuTileGrid(lv_obj_t* grid) {
    addSharedListStyles(grid);
    // Added after the list styles, so the grid padding and wrapping take precedence.
    lv_obj_add_style(grid, &tileStyles().grid, LV_PART_MAIN);
}

std::int32_t menuTileWidth(std::int32_t gridWidth) {
    return (gridWidth - 2 * kGridPad - kGridGap) / 2;
}

void createMenuRow(MenuItemView& view, lv_obj_t* list) {
    view.tile = false;
    view.container = list;
    view.root = createPlainObject(list);
    addSharedListRowStyles(view.root);
    view.name = createName(view.root);

    if (showsLevel(*view.item)) {
        view.level = createLevel(view.root);
        return;
    }
    view.value = lv_label_create(view.root);
    addSharedListRowValueStyles(view.value, view.item->kind() == MenuItemKind::Option);
}

void createMenuTile(MenuItemView& view, lv_obj_t* grid, std::int32_t tileWidth) {
    TileStyles& styles = tileStyles();
    view.tile = true;
    view.container = grid;
    view.root = createPlainObject(grid);
    lv_obj_add_style(view.root, &styles.tile, LV_PART_MAIN);
    lv_obj_add_style(view.root, &styles.tileSelected, LV_STATE_CHECKED);

    if (view.item->wideTile()) {
        lv_obj_add_style(view.root, &styles.wide, LV_PART_MAIN);
        view.icon = createIcon(view.root);
        view.name = createName(view.root);
        lv_obj_add_style(view.name, &styles.tileName, LV_PART_MAIN);
        if (showsLevel(*view.item)) {
            view.level = createLevel(view.root);
            lv_obj_add_style(view.level, &styles.level, LV_PART_MAIN);
            return;
        }
        view.value = lv_label_create(view.root);
        addSharedListRowValueStyles(view.value, false);
        return;
    }

    lv_obj_add_style(view.root, &styles.regular, LV_PART_MAIN);
    lv_obj_set_width(view.root, tileWidth);
    lv_obj_t* header = createPlainObject(view.root);
    lv_obj_add_style(header, &styles.header, LV_PART_MAIN);
    // Taps anywhere on the tile select it.
    lv_obj_remove_flag(header, LV_OBJ_FLAG_CLICKABLE);
    view.icon = createIcon(header);
    view.name = createName(header);
    lv_obj_add_style(view.name, &styles.tileName, LV_PART_MAIN);

    view.value = lv_label_create(view.root);
    lv_obj_add_style(view.value, &styles.value, LV_PART_MAIN);
    lv_obj_add_style(view.value, &styles.toggledOn, kToggledOnState);
    lv_label_set_long_mode(view.value, LV_LABEL_LONG_MODE_DOTS);
}

void refreshMenuItemView(const MenuItemView& view) {
    const MenuItem& item = *view.item;
    lv_label_set_text_static(view.name, view.tile ? item.shortName() : item.name());

    if (view.icon != nullptr) {
        lv_label_set_text_static(view.icon, item.icon());
        setHidden(view.icon, item.icon()[0] == '\0');
    }

    const MenuChoiceItem* choice =
        item.kind() == MenuItemKind::Choice ? static_cast<const MenuChoiceItem*>(&item) : nullptr;
    if (view.tile) {
        const bool on = choice != nullptr && choice->toggledOn();
        setToggledOn(view.icon, on);
        setToggledOn(view.value, on);
    }

    if (view.level != nullptr) {
        const std::size_t count = choice->optionCount();
        const std::size_t option = choice->displayedOption();
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

    const char* value = item.showsChevron() ? LV_SYMBOL_RIGHT : item.valueText();
    lv_label_set_text_static(view.value, value);
    // Tiles without a value center their name.
    if (view.tile) {
        setHidden(view.value, value[0] == '\0');
    }
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
