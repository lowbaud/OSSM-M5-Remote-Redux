#pragma once

#include <lvgl.h>

#include "ui/generated/screens.h"

namespace m5_redux {

// Reads a named color from the active EEZ color theme. Colors applied from handwritten code
// are not refreshed by change_color_theme().
inline lv_color_t themeColor(Colors color) {
    return lv_color_hex(theme_colors[active_theme_index][color]);
}

}  // namespace m5_redux
