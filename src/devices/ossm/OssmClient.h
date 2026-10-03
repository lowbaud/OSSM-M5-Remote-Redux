#pragma once

#include <NimBLEDevice.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <type_traits>

#include "OssmControlClient.h"

namespace ossm {

class OssmClientWorker;

class OssmClient : public OssmControlClient {
  public:
    enum class ConnectionState : uint32_t {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting,
    };

    enum class ModeState : uint32_t {
        Idle,
        Entering,
        SpeedKnobBlocked,
        Ready,
        Failed,
    };

    // Firmware identified from the Device Information Service.
    enum class FirmwareFamily : uint8_t {
        Unknown,
        Official,
        Lite,
    };

    OssmClient() = default;
    OssmClient(const OssmClient&) = delete;
    OssmClient& operator=(const OssmClient&) = delete;
    OssmClient(OssmClient&&) = delete;
    OssmClient& operator=(OssmClient&&) = delete;

    bool begin();

    bool connect(const NimBLEAddress& address);
    bool disconnect();
    bool enterStrokeEngine();
    ConnectionState connectionState() const;
    ModeState modeState() const;
    bool isReady() const override;
    int lastError() const;
    bool supportsCollapsedRange() const override;
    FirmwareFamily firmwareFamily() const;
    // Signal strength of the current connection in dBm, or 0 when unknown.
    int rssi() const;
    // Lowest amount of unused worker stack seen so far, in bytes.
    uint32_t workerStackMinimumFree() const;

    static constexpr size_t kObservedStateCapacity = 48;
    static constexpr size_t kObservedSessionIdCapacity = 37;
    static constexpr size_t kMaxPatternCount = 32;
    static constexpr size_t kPatternNameCapacity = 32;

    struct ObservedState {
        uint32_t timestamp = 0;
        char state[kObservedStateCapacity] = {};
        float speed = 0;
        float stroke = 0;
        float depth = 0;
        float sensation = 0;
        float buffer = 0;
        int pattern = 0;
        float position = 0;
        char sessionId[kObservedSessionIdCapacity] = {};
    };

    struct PatternInfo {
        int id = 0;
        char name[kPatternNameCapacity] = {};
    };

    struct PatternList {
        size_t count = 0;
        PatternInfo patterns[kMaxPatternCount] = {};
    };

    static_assert(
        std::is_trivially_copyable<ObservedState>::value,
        "ObservedState must be safe to copy through a FreeRTOS queue");
    static_assert(
        std::is_trivially_copyable<PatternList>::value,
        "PatternList must be safe to copy through a FreeRTOS queue");

    bool latestObservedState(ObservedState& out) const;
    bool patternList(PatternList& out) const;

    bool setSpeed(int speed) override;
    void setMotionRange(int depth, int stroke) override;
    void setDepth(int depth) override;
    void setStroke(int stroke) override;
    void setSensation(int sensation) override;
    void setPattern(int patternId) override;

    void stop() override;

  private:
    friend class OssmClientWorker;

    static constexpr uint32_t kWorkerStackSize = 5120;
    static constexpr UBaseType_t kWorkerPriority = 1;
    static constexpr TickType_t kWorkerTickInterval = pdMS_TO_TICKS(100);
    static constexpr TickType_t kMotionWriteInterval = pdMS_TO_TICKS(50);

    enum Error : int {
        kNoError = 0,
        kWorkerStartError = -1,
        kServiceNotFoundError = -2,
        kCommandCharacteristicError = -3,
        kStateCharacteristicError = -4,
        kRequestedMailboxError = -5,
        kWorkerClientCreateError = -6,
        kPatternListCharacteristicError = -7,
    };

    enum class ConnectionAction : uint8_t {
        None,
        Connect,
        Disconnect,
    };

    struct ConnectionRequest {
        uint32_t generation = 0;
        ConnectionAction action = ConnectionAction::None;
        NimBLEAddress address;
    };

    enum class ModeTarget : uint8_t {
        None,
        StrokeEngine,
    };

    struct ModeRequest {
        uint32_t generation = 0;
        ModeTarget target = ModeTarget::None;
    };

    struct RequestedState {
        ConnectionRequest connection;
        ModeRequest mode;
        uint32_t speedValidityEpoch = 0;
        int speed = 0;
        int depth = 0;
        int stroke = 0;
        int sensation = 0;
        int pattern = 0;
    };

    static_assert(
        std::is_trivially_copyable<RequestedState>::value,
        "RequestedState must be safe to copy through a FreeRTOS queue");

    OssmClientWorker* worker_ = nullptr;
    TaskHandle_t workerTask_ = nullptr;
    bool initialized_ = false;
    RequestedState requestedState_{};
    uint32_t nextConnectionGeneration_ = 0;
    uint32_t nextModeGeneration_ = 0;
    std::atomic<ConnectionState> connectionState_{ConnectionState::Disconnected};
    std::atomic<ModeState> modeState_{ModeState::Idle};
    std::atomic<bool> ready_{false};
    std::atomic<uint32_t> speedValidityEpoch_{0};
    std::atomic<int> lastError_{0};
    std::atomic<bool> collapsedRangeSupported_{false};
    std::atomic<FirmwareFamily> firmwareFamily_{FirmwareFamily::Unknown};
    std::atomic<int> rssi_{0};

    static void workerEntry(void* context);
    bool publishRequestedState();
};

}  // namespace ossm
