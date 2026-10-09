#pragma once

#include <lvgl.h>

#include <cstdint>

namespace m5_redux {

// Shared look for selectable list rows: divider below each row, and a filled background with an
// accent bar on the left while the row has LV_STATE_CHECKED.
void styleListRow(lv_obj_t* row);

// Height of a row styled by styleListRow() holding a single line of text.
std::int32_t listRowHeight();

// Removes row spacing and gives the list the thin scrollbar used by styled rows.
void styleList(lv_obj_t* list);

// Adds the left-aligned label that takes the remaining row width.
lv_obj_t* addListRowName(lv_obj_t* row, const char* text);

// Adds a right-aligned label in the secondary text color.
lv_obj_t* addListRowValue(lv_obj_t* row, const char* text);

// Applies the selected state to the row and its labels, so labels can style their own
// LV_STATE_CHECKED appearance.
void setListRowSelected(lv_obj_t* row, bool selected);

// Shared styles with the look of the functions above, for rows that are created once and kept.
// Each object holds a reference per style instead of its own copy of every property. Remove the
// theme styles first with lv_obj_remove_style_all() on rows and bars. Lists may keep them when
// their own styles clear the theme's background, border and radius, like the EEZ menu lists.
void addSharedListStyles(lv_obj_t* list);
void addSharedListRowStyles(lv_obj_t* row);
void addSharedListRowNameStyles(lv_obj_t* label);
// Accent labels, like the check mark of a stored option, stay in the accent color.
void addSharedListRowValueStyles(lv_obj_t* label, bool accent);
// A small level bar in place of the value label.
void addSharedListRowLevelStyles(lv_obj_t* bar);

}  // namespace m5_redux
