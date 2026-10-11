#include "ValueHighlight.h"

#include "ui/ThemeColors.h"

namespace m5_redux {

namespace {

// Each way of a pulse; a whole pulse takes twice as long.
constexpr std::uint32_t kPulseDurationMs = 600;

}  // namespace

void startAccentPulse(void* var, lv_anim_exec_xcb_t exec) {
    lv_anim_t pulse;
    lv_anim_init(&pulse);
    lv_anim_set_var(&pulse, var);
    lv_anim_set_exec_cb(&pulse, exec);
    lv_anim_set_values(&pulse, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&pulse, kPulseDurationMs);
    lv_anim_set_reverse_duration(&pulse, kPulseDurationMs);
    lv_anim_set_repeat_count(&pulse, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&pulse, lv_anim_path_ease_in_out);
    lv_anim_start(&pulse);
}

void ValueHighlight::begin(lv_obj_t* valueLabel, lv_obj_t* captionLabel) {
    valueLabel_ = valueLabel;
    captionLabel_ = captionLabel;
    valueColor_ = lv_obj_get_style_text_color(valueLabel_, LV_PART_MAIN);
    captionColor_ = lv_obj_get_style_text_color(captionLabel_, LV_PART_MAIN);
}

// Lights up immediately and restarts the delayed fade, so continuous turning stays lit.
void ValueHighlight::highlight() {
    // A pulse already shows the value in the accent color.
    if (pulsing_) {
        return;
    }
    lv_anim_delete(this, applyStrength);
    setStrength(LV_OPA_COVER);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, this);
    lv_anim_set_exec_cb(&fade, applyStrength);
    lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_delay(&fade, kHoldDurationMs);
    lv_anim_set_duration(&fade, kFadeDurationMs);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_in_out);
    lv_anim_start(&fade);
}

void ValueHighlight::setPulsing(bool pulsing) {
    if (pulsing == pulsing_) {
        return;
    }
    pulsing_ = pulsing;
    lv_anim_delete(this, applyStrength);
    lv_anim_delete(this, applyCaptionPulse);
    setStrength(LV_OPA_TRANSP);
    if (pulsing) {
        startAccentPulse(this, applyCaptionPulse);
    }
}

void ValueHighlight::reset() {
    pulsing_ = false;
    lv_anim_delete(this, applyStrength);
    lv_anim_delete(this, applyCaptionPulse);
    setStrength(LV_OPA_TRANSP);
}

// Blends from the colors EEZ assigned toward the accent color.
void ValueHighlight::setStrength(lv_opa_t strength) {
    const lv_color_t accent = themeColor(COLOR_ID_ACCENT);
    lv_obj_set_style_text_color(
        valueLabel_, lv_color_mix(accent, valueColor_, strength), LV_PART_MAIN);
    lv_obj_set_style_text_color(
        captionLabel_, lv_color_mix(accent, captionColor_, strength), LV_PART_MAIN);
}

void ValueHighlight::applyStrength(void* highlight, std::int32_t strength) {
    static_cast<ValueHighlight*>(highlight)->setStrength(static_cast<lv_opa_t>(strength));
}

void ValueHighlight::applyCaptionPulse(void* highlight, std::int32_t strength) {
    auto* self = static_cast<ValueHighlight*>(highlight);
    lv_obj_set_style_text_color(
        self->captionLabel_,
        lv_color_mix(
            themeColor(COLOR_ID_ACCENT), self->captionColor_, static_cast<lv_opa_t>(strength)),
        LV_PART_MAIN);
}

}  // namespace m5_redux
