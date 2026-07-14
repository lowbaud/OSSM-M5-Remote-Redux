#include "IdleTimer.h"

namespace m5_redux {

void IdleTimer::begin(std::uint32_t nowMs) {
    lastActivityAtMs_ = nowMs;
    initialized_ = true;
}

void IdleTimer::reset(std::uint32_t nowMs) {
    if (initialized_) {
        lastActivityAtMs_ = nowMs;
    }
}

std::uint32_t IdleTimer::idleForMs(std::uint32_t nowMs) const {
    return initialized_ ? nowMs - lastActivityAtMs_ : 0;
}

}  // namespace m5_redux
