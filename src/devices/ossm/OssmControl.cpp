#include "OssmControl.h"

namespace m5_redux {

OssmControl::OssmControl(ossm::OssmControlClient& client) : client_(client) {}

void OssmControl::resetDefaults() {
    values_ = {};
    endAction();
    client_.setMotionRange(values_.depth, values_.stroke);
    client_.setSensation(values_.sensation);
    client_.setPattern(values_.pattern);
    client_.setSpeed(0);
}

// Firmware that can hold a collapsed range allows min and max to meet.
int OssmControl::minimumStroke() const {
    return client_.supportsCollapsedRange() ? 0 : ossm::OssmControlClient::kMinimumOpenStroke;
}

bool OssmControl::apply(const OssmControlAdjustments& adjustments) {
    if (!client_.isReady()) {
        return false;
    }

    bool changed = false;
    switch (action_) {
        case OssmAction::PullingOut:
            // Only Stop interrupts a pull out.
            return false;
        case OssmAction::Paused:
        case OssmAction::Resuming:
        case OssmAction::Ramping:
            // The speed knob takes over: from 0 while paused, from the speed a ramp reached.
            if (adjustments.speed != 0) {
                endAction();
                changed = true;
            }
            break;
        case OssmAction::None:
            break;
    }

    const int strokeFloor = minimumStroke();
    int nextStroke = adjustPercent(values_.stroke, adjustments.stroke);
    int nextDepth = adjustPercent(values_.depth, adjustments.depth);
    if (nextStroke < strokeFloor) {
        nextStroke = strokeFloor;
    }
    if (nextDepth < strokeFloor) {
        nextDepth = strokeFloor;
    }
    if (nextStroke > nextDepth) {
        nextStroke = nextDepth;
    }

    const bool strokeChanged = nextStroke != values_.stroke;
    const bool depthChanged = nextDepth != values_.depth;
    if (strokeChanged) {
        values_.stroke = nextStroke;
        changed = true;
    }

    if (depthChanged) {
        values_.depth = nextDepth;
        changed = true;
    }

    if (strokeChanged && depthChanged) {
        client_.setMotionRange(values_.depth, values_.stroke);
    } else if (strokeChanged) {
        client_.setStroke(values_.stroke);
    } else if (depthChanged) {
        client_.setDepth(values_.depth);
    }

    const int nextSensation = adjustPercent(values_.sensation, adjustments.sensation);
    if (nextSensation != values_.sensation) {
        values_.sensation = nextSensation;
        client_.setSensation(values_.sensation);
        changed = true;
    }

    const int nextSpeed = adjustPercent(values_.speed, adjustments.speed);
    if (nextSpeed != values_.speed) {
        setSpeed(nextSpeed);
        changed = true;
    }

    return changed;
}

bool OssmControl::setPattern(int patternId) {
    // A pull out ignores pattern changes until it is done.
    if (!client_.isReady() || patternId < 0 || action_ == OssmAction::PullingOut) {
        return false;
    }

    if (patternId == values_.pattern) {
        return true;
    }

    values_.pattern = patternId;
    values_.sensation = 50;

    client_.setPattern(values_.pattern);
    client_.setSensation(values_.sensation);
    return true;
}

void OssmControl::stop() {
    endAction();
    values_.speed = 0;
    client_.setSpeed(0);
}

void OssmControl::handleReadinessLost() {
    resetDefaults();
}

const OssmControlValues& OssmControl::values() const {
    return values_;
}

void OssmControl::togglePause(std::uint32_t nowMs) {
    if (!client_.isReady() || action_ == OssmAction::PullingOut) {
        return;
    }

    if (action_ == OssmAction::Paused) {
        startRamp(OssmAction::Resuming, pausedSpeed_, kResumeDurationMs, nowMs);
        return;
    }

    // Pausing again while resuming keeps the speed it was resuming to.
    const int speed = action_ == OssmAction::Resuming ? pausedSpeed_ : values_.speed;
    if (speed == 0) {
        return;
    }
    action_ = OssmAction::Paused;
    pausedSpeed_ = speed;
    setSpeed(0);
}

void OssmControl::toggleRamp(int increase, std::uint32_t durationMs, std::uint32_t nowMs) {
    if (!client_.isReady() || action_ == OssmAction::PullingOut) {
        return;
    }

    if (action_ == OssmAction::Ramping) {
        endAction();
        return;
    }

    // During a pause the speed is 0, so the ramp starts from there and the pause ends.
    const int target = adjustPercent(values_.speed, increase);
    if (target == values_.speed) {
        return;
    }
    startRamp(OssmAction::Ramping, target, durationMs, nowMs);
}

void OssmControl::pullOut(std::uint32_t nowMs) {
    if (!client_.isReady() || action_ == OssmAction::PullingOut) {
        return;
    }

    endAction();
    const int stroke = client_.isOfficialFirmware() ? kOfficialPullOutStroke : minimumStroke();
    values_.stroke = stroke;
    values_.depth = kPullOutStart + stroke;
    client_.setMotionRange(values_.depth, values_.stroke);

    action_ = OssmAction::PullingOut;
    actionStartMs_ = nowMs;
    actionDurationMs_ = kPullOutDurationMs;
    setSpeed(kPullOutSpeed);
}

bool OssmControl::update(std::uint32_t nowMs) {
    if (action_ == OssmAction::None || action_ == OssmAction::Paused || !client_.isReady()) {
        return false;
    }

    const OssmAction previousAction = action_;
    const int previousSpeed = values_.speed;
    const std::uint32_t elapsed = nowMs - actionStartMs_;
    const bool done = elapsed >= actionDurationMs_;
    if (action_ == OssmAction::PullingOut) {
        // The machine had time to get out; it stops there.
        if (done) {
            endAction();
            setSpeed(0);
        }
    } else {
        int speed = rampTo_;
        if (!done) {
            const std::int64_t span = rampTo_ - rampFrom_;
            speed = rampFrom_ + static_cast<int>(span * elapsed / actionDurationMs_);
        }
        if (speed != values_.speed) {
            setSpeed(speed);
        }
        if (done) {
            endAction();
        }
    }
    return values_.speed != previousSpeed || action_ != previousAction;
}

OssmAction OssmControl::action() const {
    return action_;
}

int OssmControl::pausedSpeed() const {
    return pausedSpeed_;
}

// The OSSM can refuse a speed above 0, e.g. when it is not running a pattern; it stops then and
// any running action ends.
void OssmControl::setSpeed(int speed) {
    if (speed == 0) {
        values_.speed = 0;
        client_.setSpeed(0);
        return;
    }
    if (client_.setSpeed(speed)) {
        values_.speed = speed;
        return;
    }
    values_.speed = 0;
    client_.stop();
    endAction();
}

void OssmControl::startRamp(
    OssmAction action, int target, std::uint32_t durationMs, std::uint32_t nowMs) {
    action_ = action;
    // A ramp started from a pause forgets the paused speed; resuming still heads for it.
    if (action != OssmAction::Resuming) {
        pausedSpeed_ = 0;
    }
    rampFrom_ = values_.speed;
    rampTo_ = target;
    actionStartMs_ = nowMs;
    actionDurationMs_ = durationMs;
}

void OssmControl::endAction() {
    action_ = OssmAction::None;
    pausedSpeed_ = 0;
}

int OssmControl::adjustPercent(int value, std::int64_t adjustment) {
    const std::int64_t adjusted = static_cast<std::int64_t>(value) + adjustment;
    if (adjusted < kMinimum) {
        return kMinimum;
    }
    if (adjusted > kMaximum) {
        return kMaximum;
    }
    return static_cast<int>(adjusted);
}

}  // namespace m5_redux
