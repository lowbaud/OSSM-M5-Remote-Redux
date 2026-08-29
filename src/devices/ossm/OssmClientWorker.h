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
    static constexpr size_t kStateNotificationCapacity = 512;
    static constexpr TickType_t kModeTimeout = pdMS_TO_TICKS(60000);
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

    // A valid value was accepted by a successful BLE write in this connection; it does not
    // confirm that the machine applied the value.
    struct MotionFieldWriteState {
        int lastWrittenValue = 0;
        bool valid = false;

        bool matchesLastWrite(int value) const {
            return valid && value == lastWrittenValue;
        }

        void recordSuccessfulWrite(int value) {
            lastWrittenValue = value;
            valid = true;
        }
    };

    struct MotionWriteState {
        MotionFieldWriteState speed;
        MotionFieldWriteState stroke;
        MotionFieldWriteState depth;
        MotionFieldWriteState sensation;
        MotionFieldWriteState pattern;
    };

    void reconcile();
    bool reconcileConnection();
    bool reconcileMode();
    void reconcileUrgentStopRequest();
    void reconcileMotion();

    void applyRequestedState(const OssmClient::RequestedState& incoming);
    bool motionReady() const;
    bool hasDirtyMotion() const;
    TickType_t nextWakeAt() const;

    bool connectNow(const NimBLEAddress& address);
    void clearConnectionState();
    void handlePendingDisconnect();

    void setModeState(OssmClient::ModeState state);
    void failMode(ModeFailure failure);
    void resetModeOperation();
    void setMotionReady(bool ready);
    void invalidateSpeed();

    bool drainLatestStateNotification();
    void readInitialState();
    bool loadPatterns();
    void parseStateNotification(const StateNotification& notification);
    MachineStateCategory classifyMachineState(const char* state) const;

    bool writeCommand(const char* command);
    bool writeSetCommand(const char* field, int value);
    bool writePatternCommand(int patternId);

    static const char* modeFailureName(ModeFailure failure);
    static const char* machineStateCategoryName(MachineStateCategory category);
    void recordError(int error);
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

    NimBLEClient* client_ = nullptr;
    uint32_t handledConnectionGeneration_ = 0;
    std::atomic<bool> disconnectPending_{false};
    std::atomic<bool> disconnectExpected_{false};
    std::atomic<int> disconnectReason_{0};

    NimBLERemoteCharacteristic* commandCharacteristic_ = nullptr;
    NimBLERemoteCharacteristic* speedKnobCharacteristic_ = nullptr;
    NimBLERemoteCharacteristic* stateCharacteristic_ = nullptr;
    NimBLERemoteCharacteristic* patternListCharacteristic_ = nullptr;

    bool observedStateValid_ = false;
    MachineStateCategory observedStateCategory_ = MachineStateCategory::NoUsableState;
    ModeOperation modeOperation_{};

    MotionWriteState motionWriteState_{};
    TickType_t nextMotionWriteAt_ = 0;

    bool initialized_ = false;
};

}  // namespace ossm
