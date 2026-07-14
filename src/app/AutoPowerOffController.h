#pragma once

#include <cstdint>

namespace m5_redux {

class AutoPowerOffController {
  public:
    void begin(std::uint32_t timeoutSeconds, std::uint32_t nowMs, bool externalPowerPresent);
    void update(std::uint32_t nowMs, std::uint32_t idleDurationMs, bool externalPowerPresent);
    void setTimeoutSeconds(std::uint32_t seconds);

  private:
    std::uint32_t timeoutSeconds_ = 0;
    std::uint32_t batteryPoweredSinceMs_ = 0;
    bool externalPowerWasPresent_ = false;
    bool shutdownRequested_ = false;
    bool initialized_ = false;
};

}  // namespace m5_redux
