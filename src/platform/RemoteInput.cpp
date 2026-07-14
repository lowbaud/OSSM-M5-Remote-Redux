#include "RemoteInput.h"

#include <Arduino.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace m5_redux {

namespace {

struct EncoderPins {
    int clock;
    int data;
};

#ifdef ARDUINO_M5STACK_CORES3
constexpr std::array<EncoderPins, RemoteInputEvents::kEncoderCount> kEncoderPins{{
    {5, 9},
    {18, 17},
    {1, 2},
    {7, 6},
}};
constexpr std::uint8_t kMxButtonPin = 10;
constexpr std::uint8_t kLeftButtonPin = 8;
constexpr std::uint8_t kRightButtonPin = 14;
#else
constexpr std::array<EncoderPins, RemoteInputEvents::kEncoderCount> kEncoderPins{{
    {25, 26},
    {13, 14},
    {33, 32},
    {19, 27},
}};
constexpr std::uint8_t kMxButtonPin = 35;
constexpr std::uint8_t kLeftButtonPin = 36;
constexpr std::uint8_t kRightButtonPin = 34;
#endif

}  // namespace

void RemoteInput::begin() {
    pendingEvents_ = {};
    suppressLeftClick_ = false;
    previousCounts_.fill(0);
    countRemainders_.fill(0);

    for (std::size_t index = 0; index < encoders_.size(); ++index) {
        encoders_[index].attachHalfQuad(kEncoderPins[index].clock, kEncoderPins[index].data);
        encoders_[index].setCount(0);
    }

    // Preserve the active-high, pull-up configuration used by the legacy remote.
    mxButton_.setup(kMxButtonPin, INPUT_PULLUP, false);
    leftButton_.setup(kLeftButtonPin, INPUT_PULLUP, false);
    rightButton_.setup(kRightButtonPin, INPUT_PULLUP, false);
    mxButton_.attachPress(handleMxPress, this);
    leftButton_.attachClick(handleLeftClick, this);
    rightButton_.attachClick(handleRightClick, this);
}

void RemoteInput::update() {
    mxButton_.tick();
    leftButton_.tick();
    rightButton_.tick();

    // A short click is reported after release, so keep suppression until the
    // callback consumes it or the button state machine returns to idle.
    if (suppressLeftClick_ && !leftPressed() && leftButton_.isIdle()) {
        suppressLeftClick_ = false;
    }

    for (std::size_t index = 0; index < encoders_.size(); ++index) {
        const std::int64_t currentCount = encoders_[index].getCount();
        countRemainders_[index] += currentCount - previousCounts_[index];
        previousCounts_[index] = currentCount;

        const std::int64_t detents = countRemainders_[index] / kCountsPerDetent;
        countRemainders_[index] -= detents * kCountsPerDetent;
        pendingEvents_.encoderSteps[index] += static_cast<std::int32_t>(detents);
    }
}

RemoteInputEvents RemoteInput::takeEvents() {
    const RemoteInputEvents events = pendingEvents_;
    pendingEvents_ = {};
    return events;
}

bool RemoteInput::leftPressed() const {
    return digitalRead(kLeftButtonPin) == HIGH;
}

void RemoteInput::suppressLeftClickUntilRelease() {
    suppressLeftClick_ = leftPressed();
}

void RemoteInput::handleMxPress(void* context) {
    static_cast<RemoteInput*>(context)->pendingEvents_.mxPress = true;
}

void RemoteInput::handleLeftClick(void* context) {
    auto* input = static_cast<RemoteInput*>(context);
    if (input->suppressLeftClick_) {
        input->suppressLeftClick_ = false;
        return;
    }
    input->pendingEvents_.leftClick = true;
}

void RemoteInput::handleRightClick(void* context) {
    static_cast<RemoteInput*>(context)->pendingEvents_.rightClick = true;
}

}  // namespace m5_redux
