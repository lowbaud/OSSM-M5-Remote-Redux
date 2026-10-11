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

// How long the left or right button is held down before it counts as a hold, not a click.
constexpr unsigned int kHoldMs = 500;

}  // namespace

void RemoteInput::begin() {
    pendingEvents_ = {};
    suppressLeftPress_ = false;
    previousCounts_.fill(0);
    countRemainders_.fill(0);

    // The expansion board provides 10 kOhm pull-ups for the encoder signals.
    ESP32Encoder::useInternalWeakPullResistors = puType::none;
    for (std::size_t index = 0; index < encoders_.size(); ++index) {
        encoders_[index].attachHalfQuad(kEncoderPins[index].clock, kEncoderPins[index].data);
        encoders_[index].setCount(0);
    }

    // The expansion board provides 10 kOhm pull-downs; pressing a button drives it high.
    mxButton_.setup(kMxButtonPin, INPUT, false);
    leftButton_.setup(kLeftButtonPin, INPUT, false);
    rightButton_.setup(kRightButtonPin, INPUT, false);
    mxButton_.attachPress(handleMxPress, this);
    leftButton_.attachClick(handleLeftClick, this);
    rightButton_.attachClick(handleRightClick, this);
    leftButton_.setPressMs(kHoldMs);
    rightButton_.setPressMs(kHoldMs);
    leftButton_.attachLongPressStart(handleLeftHold, this);
    rightButton_.attachLongPressStart(handleRightHold, this);
}

void RemoteInput::update() {
    mxButton_.tick();
    leftButton_.tick();
    rightButton_.tick();

    // A short click is reported after release, so keep suppression until the
    // callback consumes it or the button state machine returns to idle.
    if (suppressLeftPress_ && !leftPressed() && leftButton_.isIdle()) {
        suppressLeftPress_ = false;
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

void RemoteInput::suppressLeftPress() {
    suppressLeftPress_ = leftPressed();
}

void RemoteInput::handleMxPress(void* context) {
    static_cast<RemoteInput*>(context)->pendingEvents_.mxPress = true;
}

void RemoteInput::handleLeftClick(void* context) {
    auto* input = static_cast<RemoteInput*>(context);
    if (input->suppressLeftPress_) {
        input->suppressLeftPress_ = false;
        return;
    }
    input->pendingEvents_.leftClick = true;
}

void RemoteInput::handleRightClick(void* context) {
    static_cast<RemoteInput*>(context)->pendingEvents_.rightClick = true;
}

void RemoteInput::handleLeftHold(void* context) {
    auto* input = static_cast<RemoteInput*>(context);
    // Still the suppressed press; update() lifts the suppression once it is released.
    if (!input->suppressLeftPress_) {
        input->pendingEvents_.leftHold = true;
    }
}

void RemoteInput::handleRightHold(void* context) {
    static_cast<RemoteInput*>(context)->pendingEvents_.rightHold = true;
}

}  // namespace m5_redux
