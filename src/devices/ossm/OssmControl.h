#pragma once

#include <cstdint>

#include "OssmControlClient.h"

namespace m5_redux {

struct OssmControlValues {
    int speed = 0;
    int stroke = 10;
    int depth = 10;
    int sensation = 50;
    int pattern = 0;
};

struct OssmControlAdjustments {
    std::int64_t speed = 0;
    std::int64_t stroke = 0;
    std::int64_t depth = 0;
    std::int64_t sensation = 0;
};

// A quick action that runs for a while and changes how input is handled meanwhile. Only one
// runs at a time; Stop ends any of them.
enum class OssmAction : std::uint8_t {
    None,
    // Stopped, keeping the speed to resume at. The speed knob ends it and starts from 0.
    Paused,
    // Ramping back to the paused speed. The speed knob takes over from the speed reached.
    Resuming,
    // Ramping the speed up. The speed knob takes over from the speed reached.
    Ramping,
    // Moving to the start of the rail at a low speed, then stopping. Ignores all other input.
    PullingOut,
};

class OssmControl {
  public:
    explicit OssmControl(ossm::OssmControlClient& client);

    void resetDefaults();
    int minimumStroke() const;
    bool apply(const OssmControlAdjustments& adjustments);
    bool setPattern(int patternId);
    void stop();
    void handleReadinessLost();
    const OssmControlValues& values() const;

    // Pauses, or resumes when paused. There is nothing to pause while stopped.
    void togglePause(std::uint32_t nowMs);
    // Raises the speed by increase over durationMs, or ends a running ramp at the speed reached.
    void toggleRamp(int increase, std::uint32_t durationMs, std::uint32_t nowMs);
    // Moves both ends of the range to the start of the rail and stops there.
    void pullOut(std::uint32_t nowMs);
    // Advances the running action. Returns true when the values or the action changed.
    bool update(std::uint32_t nowMs);
    OssmAction action() const;
    // The speed a pause resumes at.
    int pausedSpeed() const;
    // Stop has something to stop: the OSSM moves, or a quick action runs, e.g. a pause.
    bool canStop() const;

  private:
    static constexpr int kMinimum = 0;
    static constexpr int kMaximum = 100;
    // How long resuming to full speed takes; slower speeds get the same share of it, so the speed
    // always rises at the same rate.
    static constexpr std::uint32_t kResumeFullSpeedMs = 3000;
    static constexpr int kPullOutSpeed = 15;
    // How long a pull out over the whole rail takes; shorter ones get the same share of it as of
    // the rail they travel.
    static constexpr std::uint32_t kPullOutFullTravelMs = 3000;
    // Where a pull out leaves the range: just before the start of the rail.
    static constexpr int kPullOutStart = 2;
    // The official firmware does not come to a full stop on a collapsed range, so a pull out
    // keeps a short stroke there.
    static constexpr int kOfficialPullOutStroke = 5;

    ossm::OssmControlClient& client_;
    OssmControlValues values_{};
    OssmAction action_ = OssmAction::None;
    int pausedSpeed_ = 0;
    // Speed ramps go from rampFrom_ to rampTo_ over the action's duration.
    int rampFrom_ = 0;
    int rampTo_ = 0;
    std::uint32_t actionStartMs_ = 0;
    std::uint32_t actionDurationMs_ = 0;

    void setSpeed(int speed);
    void startRamp(OssmAction action, int target, std::uint32_t durationMs, std::uint32_t nowMs);
    void endAction();
    static int adjustPercent(int value, std::int64_t adjustment);
};

}  // namespace m5_redux
