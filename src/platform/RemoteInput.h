#pragma once

#include <ESP32Encoder.h>
#include <OneButton.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace m5_redux {

struct RemoteInputEvents {
    static constexpr std::size_t kEncoderCount = 4;

    std::array<std::int32_t, kEncoderCount> encoderSteps{};
    bool mxPress = false;
    bool leftClick = false;
    bool rightClick = false;
};

class RemoteInput {
  public:
    RemoteInput() = default;
    RemoteInput(const RemoteInput&) = delete;
    RemoteInput& operator=(const RemoteInput&) = delete;
    RemoteInput(RemoteInput&&) = delete;
    RemoteInput& operator=(RemoteInput&&) = delete;

    void begin();
    void update();
    RemoteInputEvents takeEvents();
    bool leftPressed() const;
    void suppressLeftClick();

  private:
    static constexpr std::int64_t kCountsPerDetent = 2;

    std::array<ESP32Encoder, RemoteInputEvents::kEncoderCount> encoders_{};
    OneButton mxButton_{};
    OneButton leftButton_{};
    OneButton rightButton_{};
    std::array<std::int64_t, RemoteInputEvents::kEncoderCount> previousCounts_{};
    std::array<std::int64_t, RemoteInputEvents::kEncoderCount> countRemainders_{};
    RemoteInputEvents pendingEvents_{};
    bool suppressLeftClick_ = false;

    static void handleMxPress(void* context);
    static void handleLeftClick(void* context);
    static void handleRightClick(void* context);
};

}  // namespace m5_redux
