#pragma once

#include <array>
#include <cstdint>

#include "devices/ossm/OssmControl.h"
#include "platform/RemoteInput.h"
#include "ui/BatteryIndicator.h"
#include "ui/StopButtonFeedback.h"

namespace m5_redux {

enum class OssmControlScreenAction : std::uint8_t {
    None,
    Settings,
    Patterns,
};

class OssmControlScreen {
  public:
    explicit OssmControlScreen(OssmControl& control);

    void begin();
    void enter();
    void leave();
    OssmControlScreenAction update(const RemoteInputEvents& events);
    void refresh();
    void setBatteryLevel(int percent);
    void setPatternLabel(int patternId, const char* patternName);

  private:
    enum class AccelerationPolicy : std::uint8_t {
        BothDirections,
        DecreaseOnly,
    };

    struct AccelerationState {
        std::uint32_t lastStepAtMs = 0;
        std::uint32_t consecutiveDetents = 0;
        std::uint32_t fastDetents = 0;
        std::int8_t direction = 0;
        bool hasPreviousStep = false;
    };

    static constexpr std::uint32_t kAccelerationStepIntervalMs = 200;
    static constexpr std::uint32_t kTopAccelerationStepIntervalMs = 100;
    static constexpr std::uint32_t kUnacceleratedDetents = 5;
    static constexpr std::uint32_t kTopAccelerationDetents = 4;
    static constexpr std::int64_t kContinuousMultiplier = 2;
    static constexpr std::int64_t kTopMultiplier = 3;

    OssmControl& control_;
    std::array<AccelerationState, RemoteInputEvents::kEncoderCount> accelerationStates_{};
    BatteryIndicator batteryIndicator_;
    StopButtonFeedback stopButtonFeedback_;

    void resetAcceleration();
    static std::int64_t accelerate(
        std::int64_t adjustment,
        AccelerationState& state,
        AccelerationPolicy policy,
        std::uint32_t nowMs);
};

}  // namespace m5_redux
