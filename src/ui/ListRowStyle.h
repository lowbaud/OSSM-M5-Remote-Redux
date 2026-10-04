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

}  // namespace m5_redux
