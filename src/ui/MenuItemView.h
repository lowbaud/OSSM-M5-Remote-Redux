#pragma once

#include <lvgl.h>

#include "menu/MenuItem.h"

namespace m5_redux {

// The widgets drawing one menu item, either as a list row or as an action tile. They are created
// the first time the item is shown and kept.
struct MenuItemView {
    MenuItem* item = nullptr;
    bool tile = false;
    // The list or tile row holding the view.
    lv_obj_t* container = nullptr;
    lv_obj_t* root = nullptr;
    // Tiles only.
    lv_obj_t* icon = nullptr;
    lv_obj_t* name = nullptr;
    // Rows only: the value, or a level bar instead of it.
    lv_obj_t* value = nullptr;
    lv_obj_t* level = nullptr;
};

// The line of up to four tiles above the rows of a list.
lv_obj_t* createMenuTileRow(lv_obj_t* list);

void createMenuRow(MenuItemView& view, lv_obj_t* list);
void createMenuTile(MenuItemView& view, lv_obj_t* tileRow);
void refreshMenuItemView(const MenuItemView& view);
void setMenuItemViewSelected(const MenuItemView& view, bool selected);

}  // namespace m5_redux
