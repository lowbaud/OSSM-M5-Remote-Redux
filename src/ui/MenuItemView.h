#pragma once

#include <lvgl.h>

#include <cstdint>

#include "menu/MenuItem.h"

namespace m5_redux {

// The widgets drawing one menu item, either as a list row or as a tile. They are created the
// first time the item is shown and kept.
struct MenuItemView {
    MenuItem* item = nullptr;
    bool tile = false;
    // The list or tile grid holding the view.
    lv_obj_t* container = nullptr;
    lv_obj_t* root = nullptr;
    lv_obj_t* icon = nullptr;
    lv_obj_t* name = nullptr;
    lv_obj_t* value = nullptr;
    lv_obj_t* level = nullptr;
};

// A plain object without theme styles or scrolling.
lv_obj_t* createMenuObject(lv_obj_t* parent);
// A plain object that scrolls vertically.
lv_obj_t* createMenuScrollArea(lv_obj_t* parent);

// Lays out tiles in rows of two inside a scrollable container.
void styleMenuTileGrid(lv_obj_t* grid);
// Width of a regular tile in a grid of the given width; wide tiles take the whole line.
std::int32_t menuTileWidth(std::int32_t gridWidth);

void createMenuRow(MenuItemView& view, lv_obj_t* list);
void createMenuTile(MenuItemView& view, lv_obj_t* grid, std::int32_t tileWidth);
void refreshMenuItemView(const MenuItemView& view);
void setMenuItemViewSelected(const MenuItemView& view, bool selected);

}  // namespace m5_redux
