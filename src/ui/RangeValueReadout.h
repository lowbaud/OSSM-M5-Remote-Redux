#pragma once

#include <cstdint>
#include <lvgl.h>

namespace m5_redux {

// Shows transient position labels under the knobs of a range slider while they are adjusted.
class RangeValueReadout {
  public:
    void begin(lv_obj_t* slider);
    void show(bool showStart, bool showEnd);
    void hide();

  private:
    static constexpr std::uint32_t kHoldDurationMs = 800;
    static constexpr std::uint32_t kFadeDurationMs = 250;
    static constexpr std::int32_t kSliderGap = 6;
    static constexpr std::int32_t kLabelGap = 6;
    static constexpr std::int32_t kScreenEdgeGap = 4;

    lv_obj_t* slider_ = nullptr;
    lv_obj_t* startLabel_ = nullptr;
    lv_obj_t* endLabel_ = nullptr;

    lv_obj_t* createLabel();
    std::int32_t knobX(std::int32_t value) const;
    void layout();
    static bool visible(lv_obj_t* label);
    static void reveal(lv_obj_t* label);
};

}  // namespace m5_redux
