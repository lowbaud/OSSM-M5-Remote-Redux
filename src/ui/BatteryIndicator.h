#pragma once

#include <lvgl.h>

namespace m5_redux {

class BatteryIndicator {
  public:
    void begin(lv_obj_t* label);
    void setLevel(int percent);

  private:
    lv_obj_t* label_ = nullptr;
};

}  // namespace m5_redux
