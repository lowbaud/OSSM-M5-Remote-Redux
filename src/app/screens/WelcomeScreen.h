#pragma once

#include <cstdint>

#include "platform/RemoteInput.h"
#include "ui/BatteryIndicator.h"

namespace m5_redux {

enum class WelcomeScreenAction : std::uint8_t {
    None,
    Settings,
    Scan,
};

class WelcomeScreen {
  public:
    void begin();
    void enter();
    void leave();
    WelcomeScreenAction update(const RemoteInputEvents& events);
    void setBatteryLevel(int percent);

  private:
    BatteryIndicator batteryIndicator_;
};

}  // namespace m5_redux
