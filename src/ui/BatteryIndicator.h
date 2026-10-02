#pragma once

#include <lvgl.h>

namespace m5_redux {

// The EEZ label passed to begin() only anchors the position and stays hidden.
class BatteryIndicator {
  public:
    void begin(lv_obj_t* anchorLabel);
    void setLevel(int percent, bool charging);

  private:
    lv_obj_t* container_ = nullptr;
    lv_obj_t* fill_ = nullptr;
    lv_obj_t* label_ = nullptr;
};

}  // namespace m5_redux
