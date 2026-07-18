#pragma once

#include "OssmClient.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <freertos/queue.h>

namespace ossm {

class OssmClientCallbacks : public NimBLEClientCallbacks {
  public:
    explicit OssmClientCallbacks(OssmClientWorker& worker);

    void onDisconnect(NimBLEClient* client, int reason) override;

  private:
    OssmClientWorker& worker_;
};

class OssmClientWorker {
  public:
    OssmClientWorker(
        std::atomic<OssmClient::ConnectionState>& connectionState,
        std::atomic<OssmClient::ModeState>& modeState,
        std::atomic<bool>& ready,
        std::atomic<uint32_t>& speedValidityEpoch,
        std::atomic<int>& lastError);

    bool begin();
    void loop();

    bool publishRequestedState(const OssmClient::RequestedState& requested);
    bool latestObservedState(OssmClient::ObservedState& out) const;
    bool patternList(OssmClient::PatternList& out) const;
    void noteDisconnectedFromCallback(int reason);

  private:
    static constexpr size_t kStateNotificationCapacity = 256;
    static constexpr TickType_t kModeTimeout = pdMS_TO_TICKS(60000);

    // Queue-safe copy of BLE notification bytes; oversized notifications are dropped.
    struct StateNotification {
        size_t length = 0;
        uint8_t data[kStateNotificationCapacity] = {};
    };

    // Internal classification of OSSM-reported state strings.
    enum class MachineStateCategory : uint8_t {
        NoUsableState,
        MenuReady,
        MotionReady,
        Waiting,
        SpeedKnobBlocked,
        UnsupportedBlocked,
    };

    enum class ModeFailure : uint8_t {
        None,
        CommandWriteFailed,
        UnsupportedState,
        TimedOut,
        ReadinessLost,
    };

    struct ModeOperation {
        uint32_t generation = 0;
        TickType_t startedAt = 0;
        bool commandAttempted = false;
        ModeFailure failure = ModeFailure::None;
    };

    void reconcile();
    bool reconcileConnection();
    void applyRequestedState(const OssmClient::RequestedState& incoming);
    bool reconcileMode();
    bool connectNow(const NimBLEAddress& address);
    void clearConnectionState();
    void setModeState(OssmClient::ModeState state);
    static const char* modeFailureName(ModeFailure failure);
    static const char* machineStateCategoryName(MachineStateCategory category);
    void failMode(ModeFailure failure);
    void setMotionReady(bool ready);
    void invalidateSpeed();
    void resetModeOperation();
    void handlePendingDisconnect();
    void reconcileUrgentStopRequest();
    bool drainLatestStateNotification();
    void readInitialState();
    bool loadPatterns();
    void parseStateNotification(const StateNotification& notification);
    MachineStateCategory classifyMachineState(const char* state) const;
    bool motionReady() const;
    void recordError(int error);
    void reconcileMotion();
    bool hasDirtyMotion() const;
    TickType_t nextWakeAt() const;
    bool writeCommand(const char* command);
    bool writeSetCommand(const char* field, int value);
    bool writePatternCommand(int patternId);
    void logRemoteWrite(const char* target, const char* payload, bool success) const;

    QueueHandle_t requestedMailbox_ = nullptr;
    QueueHandle_t stateNotificationMailbox_ = nullptr;
    QueueHandle_t observedStateMailbox_ = nullptr;
    QueueHandle_t patternMailbox_ = nullptr;
    std::atomic<OssmClient::ConnectionState>& connectionState_;
    std::atomic<OssmClient::ModeState>& modeState_;
    std::atomic<bool>& ready_;
    std::atomic<uint32_t>& speedValidityEpoch_;
    std::atomic<int>& lastError_;
    OssmClientCallbacks callbacks_;
    OssmClient::RequestedState requested_{};
    TickType_t nextReconcileAt_ = 0;
    TickType_t nextMotionWriteAt_ = 0;
    NimBLEClient* client_ = nullptr;
    NimBLERemoteCharacteristic* commandCharacteristic_ = nullptr;
    NimBLERemoteCharacteristic* speedKnobCharacteristic_ = nullptr;
    NimBLERemoteCharacteristic* stateCharacteristic_ = nullptr;
    NimBLERemoteCharacteristic* patternListCharacteristic_ = nullptr;
    bool observedStateValid_ = false;
    MachineStateCategory observedStateCategory_ = MachineStateCategory::NoUsableState;
    ModeOperation modeOperation_{};
    OssmClient::RequestedState lastWrittenMotion_{};
    bool motionBaselineWritten_ = false;
    uint32_t handledConnectionGeneration_ = 0;
    std::atomic<bool> disconnectPending_{false};
    std::atomic<bool> disconnectExpected_{false};
    std::atomic<int> disconnectReason_{0};
    bool initialized_ = false;
};

}  // namespace ossm
