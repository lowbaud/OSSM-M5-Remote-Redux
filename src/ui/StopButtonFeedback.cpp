#include "StopButtonFeedback.h"

namespace m5_redux {

namespace {

const lv_style_prop_t kTransitionProperties[] = {
    LV_STYLE_BG_OPA,
    LV_STYLE_BG_COLOR,
    0,
};

}  // namespace

void StopButtonFeedback::begin(lv_obj_t* button) {
    button_ = button;

    // Make the Stop button's state change visually immediate.
    lv_style_transition_dsc_init(
        &instantTransition_,
        kTransitionProperties,
        lv_anim_path_linear,
        kTransitionDurationMs,
        0,
        nullptr);
    lv_obj_set_style_transition(button_, &instantTransition_, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transition(button_, &instantTransition_, LV_PART_MAIN | LV_STATE_CHECKED);
}

void StopButtonFeedback::setMotionActive(bool active) {
    if (active) {
        lv_obj_add_state(button_, LV_STATE_CHECKED);
    } else {
        lv_obj_remove_state(button_, LV_STATE_CHECKED);
    }
}

void StopButtonFeedback::reset() {
    lv_obj_remove_state(button_, LV_STATE_PRESSED);
}

}  // namespace m5_redux
