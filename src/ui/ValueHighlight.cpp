#include "ValueHighlight.h"

#include "ui/ThemeColors.h"

namespace m5_redux {

void ValueHighlight::begin(lv_obj_t* valueLabel, lv_obj_t* captionLabel) {
    valueLabel_ = valueLabel;
    captionLabel_ = captionLabel;
    valueColor_ = lv_obj_get_style_text_color(valueLabel_, LV_PART_MAIN);
    captionColor_ = lv_obj_get_style_text_color(captionLabel_, LV_PART_MAIN);
}

// Lights up immediately and restarts the delayed fade, so continuous turning stays lit.
void ValueHighlight::highlight() {
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

void ValueHighlight::reset() {
    lv_anim_delete(this, applyStrength);
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

}  // namespace m5_redux
