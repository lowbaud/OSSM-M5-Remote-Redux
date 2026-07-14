#include "AutoPowerOffController.h"

#include "platform/M5Platform.h"

namespace m5_redux {

void AutoPowerOffController::begin(
    std::uint32_t timeoutSeconds, std::uint32_t nowMs, bool externalPowerPresent) {
    timeoutSeconds_ = timeoutSeconds;
    batteryPoweredSinceMs_ = nowMs;
    externalPowerWasPresent_ = externalPowerPresent;
    shutdownRequested_ = false;
    initialized_ = true;
}

void AutoPowerOffController::update(
    std::uint32_t nowMs, std::uint32_t idleDurationMs, bool externalPowerPresent) {
    if (!initialized_ || shutdownRequested_) {
        return;
    }

    if (externalPowerPresent) {
        externalPowerWasPresent_ = true;
        return;
    }

    if (externalPowerWasPresent_) {
        externalPowerWasPresent_ = false;
        batteryPoweredSinceMs_ = nowMs;
    }

    if (timeoutSeconds_ == 0) {
        return;
    }

    const std::uint32_t timeoutMs = timeoutSeconds_ * 1000U;
    const std::uint32_t batteryPoweredDurationMs = nowMs - batteryPoweredSinceMs_;
    if (idleDurationMs >= timeoutMs && batteryPoweredDurationMs >= timeoutMs) {
        shutdownRequested_ = true;
        m5_platform::powerOff();
    }
}

void AutoPowerOffController::setTimeoutSeconds(std::uint32_t seconds) {
    timeoutSeconds_ = seconds;
    shutdownRequested_ = false;
}

}  // namespace m5_redux
