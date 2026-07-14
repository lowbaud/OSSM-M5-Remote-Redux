#pragma once

#include <cstdint>
#include <lvgl.h>

namespace m5_redux {

class StopButtonFeedback {
  public:
    void begin(lv_obj_t* button);
    void setMotionActive(bool active);
    void reset();

  private:
    static constexpr std::uint32_t kTransitionDurationMs = 1;

    lv_obj_t* button_ = nullptr;
    lv_style_transition_dsc_t instantTransition_{};
};

}  // namespace m5_redux
