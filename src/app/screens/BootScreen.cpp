#include "BootScreen.h"

#include <Arduino.h>

#include "app/BuildInfo.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr std::uint32_t kMinimumDurationMs = 3000;
constexpr std::uint32_t kPulseDurationMs = 90;
constexpr std::uint32_t kPulseReturnDurationMs = 130;
constexpr std::uint32_t kBetweenPulsesMs = 40;
constexpr std::uint32_t kHeartbeatPauseMs = 600;
constexpr std::uint32_t kHeartbeatDurationMs =
    (kPulseDurationMs + kPulseReturnDurationMs) * 2 + kBetweenPulsesMs;
constexpr std::int32_t kFirstPulseScale = 269;
constexpr std::int32_t kSecondPulseScale = 265;

void setVersionText() {
    char text[kBuildVersionTextSize];
    formatBuildVersion(text, sizeof(text));
    lv_label_set_text(objects.boot_version_lbl, text);
}

std::int32_t
interpolateScale(std::int32_t from, std::int32_t to, std::int32_t elapsed, std::int32_t duration) {
    return from + ((to - from) * elapsed) / duration;
}

void setLogoHeartbeatFrame(void* object, std::int32_t elapsedMs) {
    std::int32_t scale = LV_SCALE_NONE;

    if (elapsedMs < static_cast<std::int32_t>(kPulseDurationMs)) {
        scale = interpolateScale(LV_SCALE_NONE, kFirstPulseScale, elapsedMs, kPulseDurationMs);
    } else if (
        (elapsedMs -= kPulseDurationMs) < static_cast<std::int32_t>(kPulseReturnDurationMs)) {
        scale =
            interpolateScale(kFirstPulseScale, LV_SCALE_NONE, elapsedMs, kPulseReturnDurationMs);
    } else if (
        (elapsedMs -= kPulseReturnDurationMs) < static_cast<std::int32_t>(kBetweenPulsesMs)) {
        scale = LV_SCALE_NONE;
    } else if ((elapsedMs -= kBetweenPulsesMs) < static_cast<std::int32_t>(kPulseDurationMs)) {
        scale = interpolateScale(LV_SCALE_NONE, kSecondPulseScale, elapsedMs, kPulseDurationMs);
    } else {
        elapsedMs -= kPulseDurationMs;
        scale =
            interpolateScale(kSecondPulseScale, LV_SCALE_NONE, elapsedMs, kPulseReturnDurationMs);
    }

    lv_image_set_scale(static_cast<lv_obj_t*>(object), static_cast<std::uint32_t>(scale));
}

}  // namespace

void BootScreen::enter() {
    timerStarted_ = false;
    stopLogoPulse();
    setVersionText();
    if (lv_screen_active() != objects.boot) {
        loadScreen(SCREEN_ID_BOOT);
    }
    startLogoPulse();
}

void BootScreen::leave() {
    timerStarted_ = false;
    stopLogoPulse();
}

BootScreenAction BootScreen::update() {
    if (!timerStarted_) {
        shownAtMs_ = millis();
        timerStarted_ = true;
        return BootScreenAction::None;
    }

    if (millis() - shownAtMs_ < kMinimumDurationMs) {
        return BootScreenAction::None;
    }

    return BootScreenAction::Finished;
}

void BootScreen::startLogoPulse() {
    lv_anim_t heartbeat;
    lv_anim_init(&heartbeat);
    lv_anim_set_var(&heartbeat, objects.redux_logo_img);
    lv_anim_set_exec_cb(&heartbeat, setLogoHeartbeatFrame);
    lv_anim_set_values(&heartbeat, 0, kHeartbeatDurationMs);
    lv_anim_set_duration(&heartbeat, kHeartbeatDurationMs);
    lv_anim_set_path_cb(&heartbeat, lv_anim_path_linear);
    lv_anim_set_repeat_count(&heartbeat, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_repeat_delay(&heartbeat, kHeartbeatPauseMs);
    lv_anim_start(&heartbeat);
}

void BootScreen::stopLogoPulse() {
    lv_anim_delete(objects.redux_logo_img, setLogoHeartbeatFrame);

    if (objects.redux_logo_img) {
        lv_image_set_scale(objects.redux_logo_img, LV_SCALE_NONE);
    }
}

}  // namespace m5_redux
