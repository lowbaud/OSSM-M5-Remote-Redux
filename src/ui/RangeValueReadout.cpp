#include "RangeValueReadout.h"

#include <initializer_list>

#include "ui/ThemeColors.h"

namespace m5_redux {

namespace {

void setLabelOpacity(void* object, std::int32_t opacity) {
    lv_obj_set_style_opa(
        static_cast<lv_obj_t*>(object), static_cast<lv_opa_t>(opacity), LV_PART_MAIN);
}

}  // namespace

void RangeValueReadout::begin(lv_obj_t* slider) {
    slider_ = slider;
    startLabel_ = createLabel();
    endLabel_ = createLabel();
}

lv_obj_t* RangeValueReadout::createLabel() {
    lv_obj_t* label = lv_label_create(lv_obj_get_parent(slider_));
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, themeColor(COLOR_ID_ACCENT), LV_PART_MAIN);
    lv_obj_set_style_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_label_set_text_static(label, "");
    return label;
}

// Labels that are still visible keep following their knob; the others stay hidden.
void RangeValueReadout::show(bool showStart, bool showEnd) {
    if (!showStart && !showEnd) {
        return;
    }
    lv_label_set_text_fmt(startLabel_, "%d", static_cast<int>(lv_slider_get_left_value(slider_)));
    lv_label_set_text_fmt(endLabel_, "%d", static_cast<int>(lv_slider_get_value(slider_)));
    if (showStart) {
        reveal(startLabel_);
    }
    if (showEnd) {
        reveal(endLabel_);
    }
    layout();
}

void RangeValueReadout::hide() {
    for (lv_obj_t* label : {startLabel_, endLabel_}) {
        lv_anim_delete(label, setLabelOpacity);
        lv_obj_set_style_opa(label, LV_OPA_TRANSP, LV_PART_MAIN);
    }
}

std::int32_t RangeValueReadout::knobX(std::int32_t value) const {
    const std::int32_t minimum = lv_slider_get_min_value(slider_);
    const std::int32_t span = lv_slider_get_max_value(slider_) - minimum;
    const std::int32_t offset = span > 0 ? (value - minimum) * lv_obj_get_width(slider_) / span : 0;
    return lv_obj_get_x(slider_) + offset;
}

void RangeValueReadout::layout() {
    // Measure the labels with their new text before positioning them.
    lv_obj_update_layout(startLabel_);

    const std::int32_t startKnob = knobX(lv_slider_get_left_value(slider_));
    const std::int32_t endKnob = knobX(lv_slider_get_value(slider_));
    const std::int32_t startWidth = lv_obj_get_width(startLabel_);
    const std::int32_t endWidth = lv_obj_get_width(endLabel_);
    const std::int32_t minimumX = kScreenEdgeGap;
    const std::int32_t maximumX = lv_obj_get_width(lv_obj_get_parent(slider_)) - kScreenEdgeGap;

    std::int32_t startX = startKnob - startWidth / 2;
    std::int32_t endX = endKnob - endWidth / 2;

    if (visible(startLabel_) && visible(endLabel_)) {
        // Close knobs push the labels apart around their midpoint, then the pair shifts
        // as one so clamping at the screen edge cannot make them overlap again.
        if (startX + startWidth + kLabelGap > endX) {
            const std::int32_t middle = (startKnob + endKnob) / 2;
            startX = middle - kLabelGap / 2 - startWidth;
            endX = startX + startWidth + kLabelGap;
        }
        if (endX + endWidth > maximumX) {
            const std::int32_t shift = endX + endWidth - maximumX;
            startX -= shift;
            endX -= shift;
        }
        if (startX < minimumX) {
            const std::int32_t shift = minimumX - startX;
            startX += shift;
            endX += shift;
        }
    } else {
        startX = LV_CLAMP(minimumX, startX, maximumX - startWidth);
        endX = LV_CLAMP(minimumX, endX, maximumX - endWidth);
    }

    const std::int32_t y = lv_obj_get_y(slider_) + lv_obj_get_height(slider_) + kSliderGap;
    lv_obj_set_pos(startLabel_, startX, y);
    lv_obj_set_pos(endLabel_, endX, y);
}

bool RangeValueReadout::visible(lv_obj_t* label) {
    return lv_obj_get_style_opa(label, LV_PART_MAIN) > LV_OPA_TRANSP;
}

void RangeValueReadout::reveal(lv_obj_t* label) {
    lv_anim_delete(label, setLabelOpacity);
    lv_obj_set_style_opa(label, LV_OPA_COVER, LV_PART_MAIN);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, label);
    lv_anim_set_exec_cb(&fade, setLabelOpacity);
    lv_anim_set_values(&fade, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_delay(&fade, kHoldDurationMs);
    lv_anim_set_duration(&fade, kFadeDurationMs);
    lv_anim_set_path_cb(&fade, lv_anim_path_ease_in);
    lv_anim_start(&fade);
}

}  // namespace m5_redux
