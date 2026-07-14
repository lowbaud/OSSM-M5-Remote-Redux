#include "BatteryIndicator.h"

namespace m5_redux {

void BatteryIndicator::begin(lv_obj_t* label) {
    label_ = label;
    lv_obj_add_flag(label_, LV_OBJ_FLAG_HIDDEN);
}

void BatteryIndicator::setLevel(int percent) {
    if (percent < 0) {
        return;
    }

    const char* symbol = LV_SYMBOL_BATTERY_EMPTY;
    if (percent >= 90) {
        symbol = LV_SYMBOL_BATTERY_FULL;
    } else if (percent >= 65) {
        symbol = LV_SYMBOL_BATTERY_3;
    } else if (percent >= 40) {
        symbol = LV_SYMBOL_BATTERY_2;
    } else if (percent >= 15) {
        symbol = LV_SYMBOL_BATTERY_1;
    }

    lv_label_set_text(label_, symbol);
    lv_obj_remove_flag(label_, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace m5_redux
