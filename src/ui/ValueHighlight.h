#pragma once

#include <cstdint>
#include <lvgl.h>

namespace m5_redux {

// Tints a value and its caption with the accent color while the value is being adjusted.
class ValueHighlight {
  public:
    void begin(lv_obj_t* valueLabel, lv_obj_t* captionLabel);
    void highlight();
    void reset();

  private:
    static constexpr std::uint32_t kHoldDurationMs = 500;
    static constexpr std::uint32_t kFadeDurationMs = 300;

    lv_obj_t* valueLabel_ = nullptr;
    lv_obj_t* captionLabel_ = nullptr;
    lv_color_t valueColor_{};
    lv_color_t captionColor_{};

    void setStrength(lv_opa_t strength);
    static void applyStrength(void* highlight, std::int32_t strength);
};

}  // namespace m5_redux
