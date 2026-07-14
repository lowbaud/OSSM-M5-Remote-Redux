#pragma once

#include <cstdint>

namespace m5_redux {

class IdleTimer {
  public:
    void begin(std::uint32_t nowMs);
    void reset(std::uint32_t nowMs);
    std::uint32_t idleForMs(std::uint32_t nowMs) const;

  private:
    std::uint32_t lastActivityAtMs_ = 0;
    bool initialized_ = false;
};

}  // namespace m5_redux
