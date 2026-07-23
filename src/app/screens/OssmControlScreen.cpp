#include "OssmControlScreen.h"

#include <Arduino.h>

#include <cstdint>

#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

OssmControlScreen::OssmControlScreen(OssmControl& control) : control_(control) {}

void OssmControlScreen::begin() {
    batteryIndicator_.begin(objects.ossm_control_battery_lbl);
    stopButtonFeedback_.begin(objects.oss_control_stop_btn);

    lv_slider_set_range(objects.ossm_control_motion_range_slider, 0, 100);
    lv_obj_remove_flag(objects.ossm_control_motion_range_slider, LV_OBJ_FLAG_CLICKABLE);
    setPatternLabel(control_.values().pattern, nullptr);
    refresh();
}

void OssmControlScreen::enter() {
    resetAcceleration();
    refresh();
    if (lv_screen_active() != objects.ossm_control) {
        loadScreen(SCREEN_ID_OSSM_CONTROL);
    }
}

void OssmControlScreen::leave() {
    resetAcceleration();
    stopButtonFeedback_.reset();
}

OssmControlScreenAction OssmControlScreen::update(const RemoteInputEvents& events) {
    if (events.leftClick) {
        return OssmControlScreenAction::Settings;
    }
    if (events.rightClick) {
        return OssmControlScreenAction::Patterns;
    }

    const std::uint32_t now = millis();
    const std::int64_t strokeAdjustment =
        static_cast<std::int64_t>(events.encoderSteps[1]) * (strokeEncoderReversed_ ? 1 : -1);

    OssmControlAdjustments adjustments;
    adjustments.speed = accelerate(
        events.encoderSteps[0], accelerationStates_[0], AccelerationPolicy::BothDirections, now);
    adjustments.stroke = strokeAdjustment;
    adjustments.depth = accelerate(
        events.encoderSteps[2], accelerationStates_[2], AccelerationPolicy::DecreaseOnly, now);
    adjustments.sensation = accelerate(
        events.encoderSteps[3], accelerationStates_[3], AccelerationPolicy::BothDirections, now);

    if (control_.apply(adjustments)) {
        refresh();
    }

    return OssmControlScreenAction::None;
}

void OssmControlScreen::resetAcceleration() {
    accelerationStates_.fill(AccelerationState{});
}

std::int64_t OssmControlScreen::accelerate(
    std::int64_t adjustment,
    AccelerationState& state,
    AccelerationPolicy policy,
    std::uint32_t nowMs) {
    if (adjustment == 0) {
        return 0;
    }

    const std::int8_t direction = adjustment > 0 ? 1 : -1;
    const bool sameDirection = state.hasPreviousStep && state.direction == direction;
    const std::uint32_t stepInterval = nowMs - state.lastStepAtMs;
    const bool continuesSequence = sameDirection && stepInterval <= kAccelerationStepIntervalMs;
    const bool continuesFastSequence =
        sameDirection && stepInterval <= kTopAccelerationStepIntervalMs;
    const bool accelerationAllowed = policy == AccelerationPolicy::BothDirections || direction < 0;

    const std::uint32_t previousDetents = continuesSequence ? state.consecutiveDetents : 0;
    const std::int64_t magnitude = adjustment > 0 ? adjustment : -adjustment;
    const std::int64_t sequenceEnd = previousDetents + magnitude;
    const std::uint64_t fastSequenceDetents =
        (continuesFastSequence ? state.fastDetents : 0) + magnitude;

    std::int64_t continuousDetents = 0;
    if (accelerationAllowed) {
        if (sequenceEnd > kUnacceleratedDetents) {
            const std::int64_t continuousStart =
                previousDetents > kUnacceleratedDetents ? previousDetents : kUnacceleratedDetents;
            continuousDetents = sequenceEnd - continuousStart;
        }
    }

    const bool topAccelerationActive =
        accelerationAllowed && fastSequenceDetents >= kTopAccelerationDetents;
    const std::int64_t topDetents = topAccelerationActive ? continuousDetents : 0;
    const std::int64_t acceleratedMagnitude = magnitude +
                                              continuousDetents * (kContinuousMultiplier - 1) +
                                              topDetents * (kTopMultiplier - kContinuousMultiplier);

    const std::uint64_t sequenceDetents = static_cast<std::uint64_t>(previousDetents) + magnitude;
    const std::uint32_t sequenceCap = kUnacceleratedDetents + 1;
    if (sequenceDetents > sequenceCap) {
        state.consecutiveDetents = sequenceCap;
    } else {
        state.consecutiveDetents = static_cast<std::uint32_t>(sequenceDetents);
    }

    if (fastSequenceDetents > kTopAccelerationDetents) {
        state.fastDetents = kTopAccelerationDetents;
    } else {
        state.fastDetents = static_cast<std::uint32_t>(fastSequenceDetents);
    }

    state.lastStepAtMs = nowMs;
    state.direction = direction;
    state.hasPreviousStep = true;
    return direction * acceleratedMagnitude;
}

void OssmControlScreen::refresh() {
    const OssmControlValues& values = control_.values();
    lv_label_set_text_fmt(objects.ossm_control_speed_value_lbl, "%02d", values.speed);
    lv_label_set_text_fmt(objects.ossm_control_sensation_value_lbl, "%02d", values.sensation);

    stopButtonFeedback_.setMotionActive(values.speed > 0);

    const int strokeStart = values.depth - values.stroke;
    lv_slider_set_value(objects.ossm_control_motion_range_slider, values.depth, LV_ANIM_OFF);
    lv_slider_set_start_value(objects.ossm_control_motion_range_slider, strokeStart, LV_ANIM_OFF);
}

void OssmControlScreen::setBatteryLevel(int percent) {
    batteryIndicator_.setLevel(percent);
}

void OssmControlScreen::setPatternLabel(int patternId, const char* patternName) {
    if (patternName && patternName[0] != '\0') {
        lv_label_set_text(objects.ossm_control_pattern_lbl, patternName);
    } else {
        lv_label_set_text_fmt(objects.ossm_control_pattern_lbl, "Pattern %d", patternId);
    }
}

void OssmControlScreen::setStrokeEncoderReversed(bool reversed) {
    strokeEncoderReversed_ = reversed;
}

}  // namespace m5_redux
