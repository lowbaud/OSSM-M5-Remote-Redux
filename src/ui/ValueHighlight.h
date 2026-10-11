#pragma once

#include <cstdint>
#include <lvgl.h>

namespace m5_redux {

// Calls exec on var with a strength from LV_OPA_TRANSP to LV_OPA_COVER and back, over and over,
// until lv_anim_delete(var, exec). Values and tiles pulse this way while a quick action runs.
void startAccentPulse(void* var, lv_anim_exec_xcb_t exec);

// Tints a value and its caption with the accent color while the value is being adjusted, or
// pulses the caption while something else is in control of the value.
class ValueHighlight {
  public:
    void begin(lv_obj_t* valueLabel, lv_obj_t* captionLabel);
    void highlight();
    // Pulses the caption between the color EEZ assigned and the accent color until turned off.
    // The value stays steady, so it remains easy to read.
    void setPulsing(bool pulsing);
    void reset();

  private:
    static constexpr std::uint32_t kHoldDurationMs = 500;
    static constexpr std::uint32_t kFadeDurationMs = 300;

    lv_obj_t* valueLabel_ = nullptr;
    lv_obj_t* captionLabel_ = nullptr;
    lv_color_t valueColor_{};
    lv_color_t captionColor_{};
    bool pulsing_ = false;

    void setStrength(lv_opa_t strength);
    static void applyStrength(void* highlight, std::int32_t strength);
    static void applyCaptionPulse(void* highlight, std::int32_t strength);
};

}  // namespace m5_redux
