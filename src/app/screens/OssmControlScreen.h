#pragma once

#include <lvgl.h>

#include <array>
#include <cstdint>

#include "devices/ossm/OssmControl.h"
#include "menu/MenuItem.h"
#include "platform/RemoteInput.h"
#include "settings/SettingsStore.h"
#include "ui/BatteryIndicator.h"
#include "ui/RangeValueReadout.h"
#include "ui/StopButtonFeedback.h"
#include "ui/ValueHighlight.h"

namespace m5_redux {

enum class OssmControlScreenAction : std::uint8_t {
    None,
    Menu,
    Patterns,
    // A side button was held: run the quick action held on it.
    LeftHeld,
    RightHeld,
};

class OssmControlScreen {
  public:
    explicit OssmControlScreen(OssmControl& control);

    void begin();
    void enter();
    void leave();
    OssmControlScreenAction update(const RemoteInputEvents& events);
    void refresh();
    void setBatteryLevel(int percent, bool charging);
    void setPatternLabel(int patternId, const char* patternName);
    void setDepthControlMode(DepthControlMode mode);
    void setStrokeEncoderReversed(bool reversed);
    // Shows the icons of the quick actions held on the left and right button after the labels of
    // their soft buttons; nullptr for none.
    void setHeldActions(const MenuItem* left, const MenuItem* right);

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

    struct BoundaryPushState {
        std::uint32_t lastStepAtMs = 0;
        bool hasPreviousStep = false;
        bool pushing = false;
    };

    // A soft button label followed by the icon of the quick action held on its button.
    struct HeldActionLabel {
        lv_obj_t* label = nullptr;
        // A copy of the EEZ text, e.g. "Menu", to show again without the icon.
        char text[16] = {};
        const MenuItem* action = nullptr;
        char textWithIcon[32] = {};
    };

    static constexpr std::uint32_t kBoundaryPushIntervalMs = 750;
    static constexpr std::uint32_t kAccelerationStepIntervalMs = 200;
    static constexpr std::uint32_t kTopAccelerationStepIntervalMs = 100;
    static constexpr std::uint32_t kUnacceleratedDetents = 5;
    static constexpr std::uint32_t kTopAccelerationDetents = 4;
    static constexpr std::int64_t kContinuousMultiplier = 2;
    static constexpr std::int64_t kTopMultiplier = 3;

    OssmControl& control_;
    std::array<AccelerationState, RemoteInputEvents::kEncoderCount> accelerationStates_{};
    BoundaryPushState boundaryPushState_{};
    BatteryIndicator batteryIndicator_;
    StopButtonFeedback stopButtonFeedback_;
    RangeValueReadout rangeValueReadout_;
    ValueHighlight speedHighlight_;
    ValueHighlight sensationHighlight_;
    HeldActionLabel leftButtonLabel_;
    HeldActionLabel rightButtonLabel_;
    DepthControlMode depthControlMode_ = SettingsStore::kDefaultDepthControlMode;
    bool strokeEncoderReversed_ = false;
    bool holdFeedbackActive_ = false;
    bool speedDimmed_ = false;

    void resetAcceleration();
    void setHoldFeedback(bool active);
    void setSpeedDimmed(bool dimmed);
    static void refreshHeldActionLabel(HeldActionLabel& held);
    bool allowBoundaryPush(
        std::int64_t rawSteps, bool hasRange, bool crossesBoundary, std::uint32_t nowMs);
    static std::int64_t accelerate(
        std::int64_t adjustment,
        AccelerationState& state,
        AccelerationPolicy policy,
        std::uint32_t nowMs);
};

}  // namespace m5_redux
