#include "OssmControl.h"

namespace m5_redux {

OssmControl::OssmControl(ossm::OssmClient& client) : client_(client) {}

void OssmControl::resetDefaults() {
    values_ = {};
    client_.setDepth(values_.depth);
    client_.setStroke(values_.stroke);
    client_.setSensation(values_.sensation);
    client_.setPattern(values_.pattern);
    client_.setSpeed(0);
}

bool OssmControl::apply(const OssmControlAdjustments& adjustments) {
    if (!client_.isReady()) {
        return false;
    }

    bool changed = false;

    int nextStroke = adjustPercent(values_.stroke, adjustments.stroke);
    const int nextDepth = adjustPercent(values_.depth, adjustments.depth);
    if (nextStroke > nextDepth) {
        nextStroke = nextDepth;
    }

    if (nextStroke != values_.stroke) {
        values_.stroke = nextStroke;
        client_.setStroke(values_.stroke);
        changed = true;
    }

    if (nextDepth != values_.depth) {
        values_.depth = nextDepth;
        client_.setDepth(values_.depth);
        changed = true;
    }

    const int nextSensation = adjustPercent(values_.sensation, adjustments.sensation);
    if (nextSensation != values_.sensation) {
        values_.sensation = nextSensation;
        client_.setSensation(values_.sensation);
        changed = true;
    }

    const int nextSpeed = adjustPercent(values_.speed, adjustments.speed);
    if (nextSpeed != values_.speed) {
        if (nextSpeed == 0) {
            values_.speed = 0;
            client_.setSpeed(0);
        } else if (client_.setSpeed(nextSpeed)) {
            values_.speed = nextSpeed;
        } else {
            values_.speed = 0;
            client_.stop();
        }
        changed = true;
    }

    return changed;
}

bool OssmControl::setPattern(int patternId) {
    if (!client_.isReady() || patternId < 0) {
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
    values_.speed = 0;
    client_.setSpeed(0);
}

void OssmControl::handleReadinessLost() {
    stop();
}

const OssmControlValues& OssmControl::values() const {
    return values_;
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
