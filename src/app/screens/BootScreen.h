#pragma once

#include <cstdint>

namespace m5_redux {

enum class BootScreenAction : std::uint8_t {
    None,
    Finished,
};

class BootScreen {
  public:
    void enter();
    void leave();
    BootScreenAction update();

  private:
    bool timerStarted_ = false;
    std::uint32_t shownAtMs_ = 0;

    void startLogoPulse();
    void stopLogoPulse();
};

}  // namespace m5_redux
