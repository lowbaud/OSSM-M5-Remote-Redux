#include "BacklightController.h"

#include <cstddef>

#include "platform/M5Platform.h"

namespace m5_redux {

void BacklightController::begin(BrightnessLevel brightness, std::uint32_t idleDimTimeoutSeconds) {
    brightnessLevel_ = brightness;
    idleDimTimeoutSeconds_ = idleDimTimeoutSeconds;
    dimmed_ = false;
    initialized_ = true;
    applyNormalBrightness();
}

void BacklightController::update(std::uint32_t idleDurationMs) {
    if (!initialized_) {
        return;
    }

    if (idleDimTimeoutSeconds_ == 0) {
        wake();
        return;
    }

    const std::uint32_t timeoutMs = idleDimTimeoutSeconds_ * 1000U;
    if (idleDurationMs < timeoutMs) {
        wake();
    } else if (!dimmed_) {
        m5_platform::setDisplayBrightnessPercent(kDimmedBrightnessPercent);
        dimmed_ = true;
    }
}

void BacklightController::setBrightnessLevel(BrightnessLevel brightness) {
    brightnessLevel_ = brightness;
    if (initialized_ && !dimmed_) {
        applyNormalBrightness();
    }
}

void BacklightController::setIdleDimTimeoutSeconds(std::uint32_t seconds) {
    idleDimTimeoutSeconds_ = seconds;
}

void BacklightController::applyNormalBrightness() {
    const std::size_t index = SettingsStore::brightnessOptionIndex(brightnessLevel_);
    m5_platform::setDisplayBrightnessPercent(SettingsStore::brightnessOption(index).percent);
}

void BacklightController::wake() {
    if (!dimmed_) {
        return;
    }

    dimmed_ = false;
    applyNormalBrightness();
}

}  // namespace m5_redux
