#include "OssmClientWorker.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "diagnostics/Log.h"

namespace ossm {

namespace {
constexpr const char* kTag = "ossm";
const NimBLEUUID kOssmServiceUuid("522B443A-4F53-534D-0001-420BADBABE69");
const NimBLEUUID kCommandCharacteristicUuid("522B443A-4F53-534D-1000-420BADBABE69");
const NimBLEUUID kSpeedKnobCharacteristicUuid("522B443A-4F53-534D-1010-420BADBABE69");
const NimBLEUUID kStateCharacteristicUuid("522B443A-4F53-534D-2000-420BADBABE69");
const NimBLEUUID kPatternListCharacteristicUuid("522B443A-4F53-534D-3000-420BADBABE69");
const NimBLEUUID kDeviceInformationServiceUuid(static_cast<uint16_t>(0x180A));
const NimBLEUUID kModelNumberCharacteristicUuid(static_cast<uint16_t>(0x2A24));
const NimBLEUUID kManufacturerNameCharacteristicUuid(static_cast<uint16_t>(0x2A29));
constexpr const char* kSpeedKnobDisabled = "false";
constexpr const char* kLiteModelNumber = "OSSM Lite";
constexpr const char* kOfficialManufacturerName = "Research And Desire";
constexpr const char* kGoToPointPatternName = "Go to Point";

bool startsWith(const char* value, const char* prefix) {
    return value && prefix && std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

// Compares a readable text characteristic, ignoring trailing NUL padding.
bool characteristicTextEquals(
    NimBLERemoteService& service, const NimBLEUUID& uuid, const char* expected) {
    NimBLERemoteCharacteristic* characteristic = service.getCharacteristic(uuid);
    if (!characteristic || !characteristic->canRead())
        return false;

    const NimBLEAttValue value = characteristic->readValue();
    size_t length = value.length();
    while (length > 0 && value.data()[length - 1] == '\0') {
        --length;
    }

    const size_t expectedLength = std::strlen(expected);
    return length == expectedLength && std::memcmp(value.data(), expected, length) == 0;
}

bool startsWith(const uint8_t* data, size_t length, const char* prefix) {
    if (!data || !prefix)
        return false;

    const size_t prefixLength = std::strlen(prefix);
    return length >= prefixLength && std::memcmp(data, prefix, prefixLength) == 0;
}

bool writeTextValue(
    const NimBLERemoteCharacteristic& characteristic, const char* payload, bool response) {
    return payload && characteristic.writeValue(payload, std::strlen(payload), response);
}

// Dumps a payload at debug level as escaped text and hex, split into line-sized chunks.
void logPayload(const char* label, const uint8_t* data, size_t length) {
    if (REDUX_LOG_LEVEL < REDUX_LOG_LEVEL_DEBUG)
        return;

    constexpr size_t kBytesPerLine = 32;
    for (size_t offset = 0; offset < length; offset += kBytesPerLine) {
        const size_t count = std::min(kBytesPerLine, length - offset);
        // Worst case is four characters per byte for text and three for hex.
        char text[kBytesPerLine * 4 + 1];
        char hex[kBytesPerLine * 3 + 1];
        size_t textLength = 0;
        size_t hexLength = 0;

        for (size_t index = 0; index < count; ++index) {
            const uint8_t byte = data[offset + index];
            if (byte == '\\' || byte == '"') {
                text[textLength++] = '\\';
                text[textLength++] = static_cast<char>(byte);
            } else if (byte >= 0x20 && byte <= 0x7e) {
                text[textLength++] = static_cast<char>(byte);
            } else {
                textLength += snprintf(
                    text + textLength,
                    sizeof(text) - textLength,
                    "\\x%02X",
                    static_cast<unsigned>(byte));
            }
            hexLength += snprintf(
                hex + hexLength, sizeof(hex) - hexLength, " %02X", static_cast<unsigned>(byte));
        }
        text[textLength] = '\0';
        hex[hexLength] = '\0';

        LOGD(kTag, "%s [%u] text: \"%s\"", label, static_cast<unsigned>(offset), text);
        LOGD(kTag, "%s [%u] hex:%s", label, static_cast<unsigned>(offset), hex);
    }
}
}  // namespace

OssmClientCallbacks::OssmClientCallbacks(OssmClientWorker& worker) : worker_(worker) {}

void OssmClientCallbacks::onDisconnect(NimBLEClient*, int reason) {
    worker_.noteDisconnectedFromCallback(reason);
}

OssmClientWorker::OssmClientWorker(
    std::atomic<OssmClient::ConnectionState>& connectionState,
    std::atomic<OssmClient::ModeState>& modeState,
    std::atomic<bool>& ready,
    std::atomic<uint32_t>& speedValidityEpoch,
    std::atomic<int>& lastError,
    std::atomic<bool>& collapsedRangeSupported)
    : connectionState_(connectionState), modeState_(modeState), ready_(ready),
      speedValidityEpoch_(speedValidityEpoch), lastError_(lastError),
      collapsedRangeSupported_(collapsedRangeSupported), callbacks_(*this) {}

bool OssmClientWorker::begin() {
    if (initialized_)
        return true;

    QueueHandle_t requestedMailbox = nullptr;
    QueueHandle_t stateNotificationMailbox = nullptr;
    QueueHandle_t observedStateMailbox = nullptr;
    QueueHandle_t patternMailbox = nullptr;

    auto cleanupQueues = [&]() {
        if (requestedMailbox)
            vQueueDelete(requestedMailbox);
        if (stateNotificationMailbox)
            vQueueDelete(stateNotificationMailbox);
        if (observedStateMailbox)
            vQueueDelete(observedStateMailbox);
        if (patternMailbox)
            vQueueDelete(patternMailbox);
    };

    requestedMailbox = xQueueCreate(1, sizeof(OssmClient::RequestedState));
    if (!requestedMailbox) {
        lastError_.store(OssmClient::kRequestedMailboxError);
        return false;
    }

    stateNotificationMailbox = xQueueCreate(1, sizeof(StateNotification));
    if (!stateNotificationMailbox) {
        cleanupQueues();
        lastError_.store(OssmClient::kRequestedMailboxError);
        return false;
    }

    observedStateMailbox = xQueueCreate(1, sizeof(OssmClient::ObservedState));
    if (!observedStateMailbox) {
        cleanupQueues();
        lastError_.store(OssmClient::kRequestedMailboxError);
        return false;
    }

    patternMailbox = xQueueCreate(1, sizeof(OssmClient::PatternList));
    if (!patternMailbox) {
        cleanupQueues();
        lastError_.store(OssmClient::kRequestedMailboxError);
        return false;
    }

    NimBLEClient* client = NimBLEDevice::createClient();
    if (!client) {
        cleanupQueues();
        lastError_.store(OssmClient::kWorkerClientCreateError);
        return false;
    }

    requestedMailbox_ = requestedMailbox;
    stateNotificationMailbox_ = stateNotificationMailbox;
    observedStateMailbox_ = observedStateMailbox;
    patternMailbox_ = patternMailbox;
    client_ = client;
    client_->setClientCallbacks(&callbacks_, false);
    nextReconcileAt_ = xTaskGetTickCount();
    initialized_ = true;
    clearConnectionState();
    return true;
}

void OssmClientWorker::loop() {
    constexpr TickType_t kStackSampleInterval = pdMS_TO_TICKS(10000);
    TickType_t lastStackSampleAt = xTaskGetTickCount() - kStackSampleInterval;
    uint32_t minimumFreeStackBytes = OssmClient::kWorkerStackSize + 1;

    for (;;) {
        const TickType_t now = xTaskGetTickCount();
        if (now - lastStackSampleAt >= kStackSampleInterval) {
            lastStackSampleAt = now;
            // ESP32 reports lifetime minimum unused stack in bytes, including completed calls.
            const uint32_t freeStackBytes = uxTaskGetStackHighWaterMark(nullptr);
            if (freeStackBytes < minimumFreeStackBytes) {
                minimumFreeStackBytes = freeStackBytes;
                LOGI(
                    kTag,
                    "Worker minimum free stack: %u bytes",
                    static_cast<unsigned>(freeStackBytes));
            }
        }

        const TickType_t wakeAt = nextWakeAt();
        const TickType_t waitTicks = wakeAt > now ? wakeAt - now : 0;

        OssmClient::RequestedState incoming;
        if (xQueueReceive(requestedMailbox_, &incoming, waitTicks) == pdPASS) {
            applyRequestedState(incoming);
            reconcileUrgentStopRequest();
        }

        handlePendingDisconnect();
        const bool observationReceived = drainLatestStateNotification();

        const TickType_t afterWait = xTaskGetTickCount();
        if (observationReceived || afterWait >= nextReconcileAt_) {
            reconcile();
            nextReconcileAt_ = xTaskGetTickCount() + OssmClient::kWorkerTickInterval;
            continue;
        }

        if (motionReady() && hasDirtyMotion() && afterWait >= nextMotionWriteAt_) {
            reconcileMotion();
        }
    }
}

bool OssmClientWorker::publishRequestedState(const OssmClient::RequestedState& requested) {
    if (!initialized_)
        return false;

    return xQueueOverwrite(requestedMailbox_, &requested) == pdPASS;
}

bool OssmClientWorker::latestObservedState(OssmClient::ObservedState& out) const {
    if (!initialized_ || !observedStateMailbox_)
        return false;

    return xQueuePeek(observedStateMailbox_, &out, 0) == pdPASS;
}

bool OssmClientWorker::patternList(OssmClient::PatternList& out) const {
    if (!initialized_ || !patternMailbox_)
        return false;

    return xQueuePeek(patternMailbox_, &out, 0) == pdPASS;
}

void OssmClientWorker::noteDisconnectedFromCallback(int reason) {
    OssmClient::ConnectionState state = connectionState_.load();
    bool expected = disconnectExpected_.load();
    if (!expected) {
        while (state != OssmClient::ConnectionState::Disconnected &&
               state != OssmClient::ConnectionState::Disconnecting &&
               !connectionState_.compare_exchange_weak(
                   state, OssmClient::ConnectionState::Disconnected)) {
        }
        expected = state == OssmClient::ConnectionState::Disconnecting;
    }
    disconnectExpected_.store(expected);
    disconnectReason_.store(reason);
    disconnectPending_.store(true);
    modeState_.store(OssmClient::ModeState::Idle);
    if (ready_.exchange(false)) {
        speedValidityEpoch_.fetch_add(1);
    }
}

void OssmClientWorker::reconcile() {
    if (!reconcileConnection())
        return;
    if (!reconcileMode())
        return;

    reconcileMotion();
}

bool OssmClientWorker::reconcileConnection() {
    const OssmClient::ConnectionRequest& request = requested_.connection;
    if (request.generation != handledConnectionGeneration_) {
        handledConnectionGeneration_ = request.generation;

        switch (request.action) {
            case OssmClient::ConnectionAction::Connect: {
                if (!client_ || request.address.isNull() ||
                    connectionState_.load() != OssmClient::ConnectionState::Connecting) {
                    return false;
                }

                const bool connected = connectNow(request.address);
                OssmClient::ConnectionState expected = OssmClient::ConnectionState::Connecting;
                if (connected && connectionState_.compare_exchange_strong(
                                     expected, OssmClient::ConnectionState::Connected)) {
                    return true;
                }

                if (connected && client_->isConnected()) {
                    client_->disconnect();
                    clearConnectionState();
                }

                expected = OssmClient::ConnectionState::Connecting;
                connectionState_.compare_exchange_strong(
                    expected, OssmClient::ConnectionState::Disconnected);
                return false;
            }

            case OssmClient::ConnectionAction::Disconnect: {
                if (connectionState_.load() != OssmClient::ConnectionState::Disconnecting) {
                    return false;
                }

                if (client_ && client_->isConnected()) {
                    if (commandCharacteristic_ && writeSetCommand("speed", 0)) {
                        motionWriteState_.speed.recordSuccessfulWrite(0);
                    }
                    disconnectExpected_.store(true);
                    client_->disconnect();
                }

                clearConnectionState();
                OssmClient::ConnectionState expected = OssmClient::ConnectionState::Disconnecting;
                connectionState_.compare_exchange_strong(
                    expected, OssmClient::ConnectionState::Disconnected);
                return false;
            }

            case OssmClient::ConnectionAction::None:
                break;
        }
    }

    return connectionState_.load() == OssmClient::ConnectionState::Connected && client_ &&
           client_->isConnected();
}

bool OssmClientWorker::reconcileMode() {
    const OssmClient::ModeRequest& request = requested_.mode;
    if (request.generation == 0 || request.target == OssmClient::ModeTarget::None) {
        setMotionReady(false);
        return false;
    }

    if (!client_ || !client_->isConnected() || !commandCharacteristic_) {
        resetModeOperation();
        return false;
    }

    if (request.generation != modeOperation_.generation) {
        modeOperation_ = {};
        modeOperation_.generation = request.generation;
        modeOperation_.startedAt = xTaskGetTickCount();
        setModeState(OssmClient::ModeState::Entering);
    }

    const OssmClient::ModeState state = modeState_.load();
    if (state == OssmClient::ModeState::Failed) {
        return false;
    }

    if (state == OssmClient::ModeState::Ready) {
        if (observedStateCategory_ == MachineStateCategory::MotionReady) {
            return true;
        }

        failMode(ModeFailure::ReadinessLost);
        return false;
    }

    const TickType_t now = xTaskGetTickCount();
    if (now - modeOperation_.startedAt >= kModeTimeout) {
        failMode(ModeFailure::TimedOut);
        return false;
    }

    switch (observedStateCategory_) {
        case MachineStateCategory::MotionReady:
            setModeState(OssmClient::ModeState::Ready);
            return true;

        case MachineStateCategory::MenuReady:
            setModeState(OssmClient::ModeState::Entering);
            if (!modeOperation_.commandAttempted) {
                modeOperation_.commandAttempted = true;
                if (!writeCommand("go:strokeEngine")) {
                    failMode(ModeFailure::CommandWriteFailed);
                    return false;
                }
            }
            return false;

        case MachineStateCategory::Waiting:
        case MachineStateCategory::NoUsableState:
            setModeState(OssmClient::ModeState::Entering);
            return false;

        case MachineStateCategory::SpeedKnobBlocked:
            setModeState(OssmClient::ModeState::SpeedKnobBlocked);
            return false;

        case MachineStateCategory::UnsupportedBlocked:
            failMode(ModeFailure::UnsupportedState);
            return false;
    }

    return false;
}

void OssmClientWorker::reconcileUrgentStopRequest() {
    if (requested_.speed != 0 || !motionReady()) {
        return;
    }

    if (writeSetCommand("speed", 0)) {
        motionWriteState_.speed.recordSuccessfulWrite(0);
    }
}

void OssmClientWorker::reconcileMotion() {
    // Normal motion writes are tick-gated, but dirty fields are sent together as a bounded burst.
    // Urgent stop is handled separately on mailbox wake.
    if (!motionReady() || !hasDirtyMotion()) {
        return;
    }

    const TickType_t now = xTaskGetTickCount();
    if (now < nextMotionWriteAt_) {
        return;
    }
    nextMotionWriteAt_ = now + OssmClient::kMotionWriteInterval;

    // Don't let failed motion updates hold up a stop or speed reduction.
    if ((requested_.speed == 0 || (motionWriteState_.speed.valid &&
                                   requested_.speed < motionWriteState_.speed.lastWrittenValue)) &&
        !motionWriteState_.speed.matchesLastWrite(requested_.speed)) {
        if (!writeSetCommand("speed", requested_.speed)) {
            return;
        }
        motionWriteState_.speed.recordSuccessfulWrite(requested_.speed);
    }

    const MotionTarget target = motionTarget();
    auto writeStroke = [&]() {
        if (motionWriteState_.stroke.matchesLastWrite(target.stroke)) {
            return true;
        }
        if (!writeSetCommand("stroke", target.stroke)) {
            return false;
        }
        motionWriteState_.stroke.recordSuccessfulWrite(target.stroke);
        return true;
    };
    auto writeDepth = [&]() {
        if (motionWriteState_.depth.matchesLastWrite(target.depth)) {
            return true;
        }
        if (!writeSetCommand("depth", target.depth)) {
            return false;
        }
        motionWriteState_.depth.recordSuccessfulWrite(target.depth);
        return true;
    };

    // Write the range before pattern and sensation so switching into or out of Go to Point
    // happens inside the range that is already in effect. Order the range writes to avoid a
    // temporary stroke overshoot.
    bool rangeWritten;
    if (!motionWriteState_.stroke.valid ||
        target.stroke < motionWriteState_.stroke.lastWrittenValue) {
        rangeWritten = writeStroke() && writeDepth();
    } else {
        rangeWritten = writeDepth() && writeStroke();
    }

    if (!rangeWritten) {
        return;
    }

    if (!motionWriteState_.pattern.matchesLastWrite(target.pattern)) {
        if (!writePatternCommand(target.pattern)) {
            return;
        }
        motionWriteState_.pattern.recordSuccessfulWrite(target.pattern);
    }

    if (!motionWriteState_.sensation.matchesLastWrite(target.sensation)) {
        if (!writeSetCommand("sensation", target.sensation)) {
            return;
        }
        motionWriteState_.sensation.recordSuccessfulWrite(target.sensation);
    }

    if (!motionWriteState_.speed.matchesLastWrite(requested_.speed)) {
        if (!writeSetCommand("speed", requested_.speed)) {
            return;
        }
        motionWriteState_.speed.recordSuccessfulWrite(requested_.speed);
    }
}

void OssmClientWorker::applyRequestedState(const OssmClient::RequestedState& incoming) {
    OssmClient::RequestedState accepted = incoming;
    if (accepted.speed > 0 &&
        (accepted.speedValidityEpoch != speedValidityEpoch_.load() || !motionReady())) {
        accepted.speed = 0;
    }
    requested_ = accepted;
}

bool OssmClientWorker::motionReady() const {
    return connectionState_.load() == OssmClient::ConnectionState::Connected && client_ &&
           client_->isConnected() && commandCharacteristic_ &&
           requested_.mode.target == OssmClient::ModeTarget::StrokeEngine &&
           requested_.mode.generation != 0 &&
           requested_.mode.generation == modeOperation_.generation &&
           modeState_.load() == OssmClient::ModeState::Ready &&
           observedStateCategory_ == MachineStateCategory::MotionReady;
}

bool OssmClientWorker::hasDirtyMotion() const {
    const MotionTarget target = motionTarget();
    return !motionWriteState_.speed.matchesLastWrite(requested_.speed) ||
           !motionWriteState_.stroke.matchesLastWrite(target.stroke) ||
           !motionWriteState_.depth.matchesLastWrite(target.depth) ||
           !motionWriteState_.sensation.matchesLastWrite(target.sensation) ||
           !motionWriteState_.pattern.matchesLastWrite(target.pattern);
}

OssmClientWorker::MotionTarget OssmClientWorker::motionTarget() const {
    MotionTarget target;
    target.stroke = requested_.stroke;
    target.depth = requested_.depth;
    target.sensation = requested_.sensation;
    target.pattern = requested_.pattern;

    if (target.stroke == 0 && usesGoToPoint()) {
        // Ordinary patterns cannot run a zero stroke safely, so hold the point with Go to Point
        // inside a 1% window. Sensation 100 selects the deep end and 0 the shallow end.
        target.pattern = goToPointPatternId_;
        target.stroke = 1;
        if (target.depth > 0) {
            target.sensation = 100;
        } else {
            target.depth = 1;
            target.sensation = 0;
        }
        return target;
    }

    if (!collapsedRangeSupported_.load()) {
        if (target.stroke < OssmClient::kMinimumOpenStroke) {
            target.stroke = OssmClient::kMinimumOpenStroke;
        }
        if (target.depth < target.stroke) {
            target.depth = target.stroke;
        }
    }

    if (strokeRelativeToDepth_) {
        // Current OSSM-RS expresses stroke as a percentage of depth, not rail length.
        if (target.depth == 0) {
            target.stroke = 0;
        } else if (target.stroke >= target.depth) {
            target.stroke = 100;
        } else {
            target.stroke = (target.stroke * 100 + target.depth / 2) / target.depth;
        }
    }
    return target;
}

bool OssmClientWorker::usesGoToPoint() const {
    return goToPointPatternId_ >= 0 &&
           (firmwareFamily_ == FirmwareFamily::Official || firmwareFamily_ == FirmwareFamily::Lite);
}

void OssmClientWorker::updateCollapsedRangeSupport() {
    // OSSM-RS holds a zero stroke at depth; official and Lite firmware need Go to Point.
    collapsedRangeSupported_.store(strokeRelativeToDepth_ || usesGoToPoint());
}

TickType_t OssmClientWorker::nextWakeAt() const {
    TickType_t wakeAt = nextReconcileAt_;
    if (motionReady() && hasDirtyMotion() && nextMotionWriteAt_ < wakeAt) {
        wakeAt = nextMotionWriteAt_;
    }
    return wakeAt;
}

bool OssmClientWorker::connectNow(const NimBLEAddress& address) {
    clearConnectionState();

    auto abortInitialization = [this](int error) {
        clearConnectionState();
        if (client_ && client_->isConnected()) {
            disconnectExpected_.store(true);
            client_->disconnect();
        }
        recordError(error);
    };

    const bool connected = client_->connect(address, true, false, true);
    if (!connected) {
        clearConnectionState();
        recordError(client_->getLastError());
        return false;
    }
    LOGI(kTag, "Negotiated MTU: %u", client_->getMTU());

    NimBLERemoteService* service = client_->getService(kOssmServiceUuid);
    if (!client_->isConnected()) {
        clearConnectionState();
        recordError(client_->getLastError());
        return false;
    }
    if (!service) {
        abortInitialization(OssmClient::kServiceNotFoundError);
        return false;
    }

    NimBLERemoteCharacteristic* command = service->getCharacteristic(kCommandCharacteristicUuid);
    NimBLERemoteCharacteristic* speedKnob =
        service->getCharacteristic(kSpeedKnobCharacteristicUuid);
    NimBLERemoteCharacteristic* state = service->getCharacteristic(kStateCharacteristicUuid);
    LOGD(kTag, "Pattern list characteristic lookup started");
    NimBLERemoteCharacteristic* patternList =
        service->getCharacteristic(kPatternListCharacteristicUuid);
    if (!client_->isConnected()) {
        clearConnectionState();
        recordError(client_->getLastError());
        return false;
    }

    if (!command || (!command->canWrite() && !command->canWriteNoResponse())) {
        abortInitialization(OssmClient::kCommandCharacteristicError);
        return false;
    }

    if (!speedKnob || !speedKnob->canWrite()) {
        abortInitialization(OssmClient::kCommandCharacteristicError);
        return false;
    }

    if (!state || !state->canRead() || !state->canNotify()) {
        abortInitialization(OssmClient::kStateCharacteristicError);
        return false;
    }

    if (!patternList) {
        LOGE(kTag, "Pattern list characteristic missing");
        abortInitialization(OssmClient::kPatternListCharacteristicError);
        return false;
    }

    if (!patternList->canRead()) {
        LOGE(kTag, "Pattern list characteristic found but not readable");
        abortInitialization(OssmClient::kPatternListCharacteristicError);
        return false;
    }
    LOGD(kTag, "Pattern list characteristic accepted for read");

    commandCharacteristic_ = command;
    speedKnobCharacteristic_ = speedKnob;
    stateCharacteristic_ = state;
    patternListCharacteristic_ = patternList;

    const bool subscribed = state->subscribe(
        true, [this](NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
            if (!stateNotificationMailbox_ || !data || length == 0 ||
                length > kStateNotificationCapacity) {
                return;
            }

            StateNotification notification{};
            notification.length = length;
            std::memcpy(notification.data, data, length);
            xQueueOverwrite(stateNotificationMailbox_, &notification);
        });
    if (!client_->isConnected()) {
        clearConnectionState();
        recordError(client_->getLastError());
        return false;
    }
    if (!subscribed) {
        abortInitialization(OssmClient::kStateCharacteristicError);
        return false;
    }

    const bool speedKnobWritten =
        writeTextValue(*speedKnobCharacteristic_, kSpeedKnobDisabled, true);
    logRemoteWrite("speed-knob", kSpeedKnobDisabled, speedKnobWritten);
    if (!speedKnobWritten) {
        abortInitialization(OssmClient::kCommandCharacteristicError);
        return false;
    }

    readDeviceInformation();
    if (!client_->isConnected()) {
        clearConnectionState();
        recordError(client_->getLastError());
        return false;
    }

    if (!loadPatterns()) {
        abortInitialization(OssmClient::kPatternListCharacteristicError);
        return false;
    }
    updateCollapsedRangeSupport();

    resetModeOperation();
    LOGI(kTag, "Connected; reading machine state");
    readInitialState();
    if (!observedStateValid_) {
        LOGI(kTag, "Connected; waiting for machine state");
    }

    // Give the machine more time after initialization before issuing further commands.
    vTaskDelay(pdMS_TO_TICKS(100));
    return true;
}

void OssmClientWorker::clearConnectionState() {
    resetModeOperation();
    requested_.speed = 0;
    commandCharacteristic_ = nullptr;
    speedKnobCharacteristic_ = nullptr;
    stateCharacteristic_ = nullptr;
    patternListCharacteristic_ = nullptr;
    observedStateValid_ = false;
    observedStateCategory_ = MachineStateCategory::NoUsableState;
    motionWriteState_ = {};
    strokeRelativeToDepth_ = false;
    firmwareFamily_ = FirmwareFamily::Unknown;
    goToPointPatternId_ = -1;
    collapsedRangeSupported_.store(false);
    nextMotionWriteAt_ = 0;

    if (initialized_) {
        xQueueReset(stateNotificationMailbox_);
        xQueueReset(observedStateMailbox_);
        xQueueReset(patternMailbox_);
    }
}

void OssmClientWorker::handlePendingDisconnect() {
    if (!disconnectPending_.exchange(false))
        return;

    const bool expectedDisconnect = disconnectExpected_.exchange(false);
    clearConnectionState();

    const OssmClient::ConnectionState state = connectionState_.load();
    if (expectedDisconnect || state == OssmClient::ConnectionState::Disconnecting) {
        OssmClient::ConnectionState expected = OssmClient::ConnectionState::Disconnecting;
        connectionState_.compare_exchange_strong(
            expected, OssmClient::ConnectionState::Disconnected);
    } else {
        connectionState_.store(OssmClient::ConnectionState::Disconnected);
        recordError(disconnectReason_.load());
    }
}

void OssmClientWorker::setModeState(OssmClient::ModeState state) {
    modeState_.store(state);
    setMotionReady(state == OssmClient::ModeState::Ready);
}

void OssmClientWorker::failMode(ModeFailure failure) {
    modeOperation_.failure = failure;
    LOGW(
        kTag,
        "Mode failure: cause=%s category=%s error=%d",
        modeFailureName(failure),
        machineStateCategoryName(observedStateCategory_),
        lastError_.load());
    setModeState(OssmClient::ModeState::Failed);
}

void OssmClientWorker::resetModeOperation() {
    modeOperation_ = {};
    setModeState(OssmClient::ModeState::Idle);
}

void OssmClientWorker::setMotionReady(bool ready) {
    if (!ready) {
        invalidateSpeed();
        return;
    }

    ready_.store(true);
}

void OssmClientWorker::invalidateSpeed() {
    if (ready_.exchange(false)) {
        speedValidityEpoch_.fetch_add(1);
    }
    requested_.speed = 0;
}

bool OssmClientWorker::drainLatestStateNotification() {
    StateNotification notification;
    if (xQueueReceive(stateNotificationMailbox_, &notification, 0) == pdPASS) {
        parseStateNotification(notification);
        return true;
    }
    return false;
}

void OssmClientWorker::readInitialState() {
    if (!stateCharacteristic_)
        return;

    const NimBLEAttValue value = stateCharacteristic_->readValue();
    const size_t length = value.length();
    if (length == 0) {
        LOGW(kTag, "Initial state read returned no data");
        return;
    }

    if (length > kStateNotificationCapacity) {
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        LOGW(kTag, "Initial state read too large: %u bytes", static_cast<unsigned>(length));
        return;
    }

    StateNotification notification{};
    notification.length = length;
    std::memcpy(notification.data, value.data(), length);
    parseStateNotification(notification);
}

// Identifies firmware that offers Go to Point; unidentified firmware keeps conservative behavior.
void OssmClientWorker::readDeviceInformation() {
    firmwareFamily_ = FirmwareFamily::Unknown;

    NimBLERemoteService* service = client_->getService(kDeviceInformationServiceUuid);
    if (!service) {
        LOGI(kTag, "Device information service missing");
        return;
    }

    if (characteristicTextEquals(*service, kModelNumberCharacteristicUuid, kLiteModelNumber)) {
        firmwareFamily_ = FirmwareFamily::Lite;
        LOGI(kTag, "Firmware identified as OSSM Lite");
    } else if (
        characteristicTextEquals(
            *service, kManufacturerNameCharacteristicUuid, kOfficialManufacturerName)) {
        firmwareFamily_ = FirmwareFamily::Official;
        LOGI(kTag, "Firmware identified as official");
    } else {
        LOGI(kTag, "Firmware not identified from device information");
    }
}

bool OssmClientWorker::loadPatterns() {
    if (!patternListCharacteristic_) {
        LOGE(kTag, "Pattern load failed: missing characteristic");
        return false;
    }

    if (!patternMailbox_) {
        LOGE(kTag, "Pattern load failed: missing mailbox");
        return false;
    }

    const NimBLEAttValue value = patternListCharacteristic_->readValue();
    const size_t length = value.length();
    if (length == 0) {
        LOGE(kTag, "Pattern list read returned no data");
        return false;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, value.data(), length);
    if (error) {
        LOGE(kTag, "Pattern list parse failed: %s", error.c_str());
        logPayload("Pattern list contents", value.data(), length);
        return false;
    }

    if (!document.is<JsonArray>()) {
        LOGE(kTag, "Pattern list parse failed: root is not an array");
        logPayload("Pattern list contents", value.data(), length);
        return false;
    }

    goToPointPatternId_ = -1;
    OssmClient::PatternList patterns{};
    size_t inspectedEntries = 0;
    size_t skippedEntries = 0;
    for (JsonVariantConst item : document.as<JsonArrayConst>()) {
        const size_t entryIndex = inspectedEntries++;
        int patternId = static_cast<int>(entryIndex);
        const char* patternName = nullptr;

        if (!item.is<JsonObjectConst>()) {
            ++skippedEntries;
            continue;
        }

        const JsonVariantConst name = item["name"];
        const JsonVariantConst idx = item["idx"];
        if (!name.is<const char*>()) {
            ++skippedEntries;
            continue;
        }

        if (!idx.is<int>()) {
            ++skippedEntries;
            continue;
        }

        patternId = idx.as<int>();
        patternName = name.as<const char*>();
        const size_t nameLength = patternName ? std::strlen(patternName) : 0;
        if (patternId < 0) {
            ++skippedEntries;
            continue;
        }

        if (nameLength == 0 || nameLength >= OssmClient::kPatternNameCapacity) {
            ++skippedEntries;
            continue;
        }

        if ((firmwareFamily_ == FirmwareFamily::Official ||
             firmwareFamily_ == FirmwareFamily::Lite) &&
            std::strcmp(patternName, kGoToPointPatternName) == 0) {
            goToPointPatternId_ = patternId;
        }

        if (patterns.count >= OssmClient::kMaxPatternCount) {
            ++skippedEntries;
            continue;
        }

        OssmClient::PatternInfo& pattern = patterns.patterns[patterns.count++];
        pattern.id = patternId;
        std::memcpy(pattern.name, patternName, nameLength + 1);
    }

    if (patterns.count == 0) {
        LOGE(
            kTag,
            "Pattern list contained no usable entries; inspected=%u skipped=%u",
            static_cast<unsigned>(inspectedEntries),
            static_cast<unsigned>(skippedEntries));
        return false;
    }

    xQueueOverwrite(patternMailbox_, &patterns);
    LOGI(
        kTag,
        "Loaded %u patterns (%u skipped)",
        static_cast<unsigned>(patterns.count),
        static_cast<unsigned>(skippedEntries));
    return true;
}

void OssmClientWorker::parseStateNotification(const StateNotification& notification) {
    // The state characteristic also carries plain-text command responses.
    if (startsWith(notification.data, notification.length, "ok:")) {
        LOGD(
            kTag,
            "Ignored protocol response: %.*s",
            static_cast<int>(notification.length),
            reinterpret_cast<const char*>(notification.data));
        return;
    }

    if (startsWith(notification.data, notification.length, "fail:")) {
        LOGW(
            kTag,
            "Protocol failure response: %.*s",
            static_cast<int>(notification.length),
            reinterpret_cast<const char*>(notification.data));
        return;
    }

    JsonDocument document;
    const DeserializationError error =
        deserializeJson(document, notification.data, notification.length);
    if (error) {
        unsigned firstByte = 0;
        unsigned lastByte = 0;
        if (notification.length > 0) {
            firstByte = static_cast<unsigned>(notification.data[0]);
            lastByte = static_cast<unsigned>(notification.data[notification.length - 1]);
        }
        LOGW(
            kTag,
            "State notification parse failed: %s; length=%u first=0x%02X last=0x%02X",
            error.c_str(),
            static_cast<unsigned>(notification.length),
            firstByte,
            lastByte);
        logPayload("State payload", notification.data, notification.length);
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    if (!document.is<JsonObject>()) {
        LOGW(kTag, "State notification parse failed: root is not an object");
        logPayload("State payload", notification.data, notification.length);
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    const JsonVariantConst state = document["state"];
    if (!state.is<const char*>()) {
        LOGW(kTag, "State notification parse failed: missing or invalid state");
        logPayload("State payload", notification.data, notification.length);
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    const char* stateText = state.as<const char*>();
    const size_t stateLength = std::strlen(stateText);
    if (stateLength == 0 || stateLength >= OssmClient::kObservedStateCapacity) {
        LOGW(
            kTag,
            "State notification parse failed: invalid state length=%u",
            static_cast<unsigned>(stateLength));
        logPayload("State payload", notification.data, notification.length);
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    OssmClient::ObservedState observed{};
    std::memcpy(observed.state, stateText, stateLength + 1);

    // OSSM-RS 0.9 currently defines stroke as percentage of depth. Until that is fixed,
    // we need this quirk. OSSM Lite also reports "idle" before homing, so it is excluded.
    if (firmwareFamily_ != FirmwareFamily::Lite && !strokeRelativeToDepth_ &&
        (std::strcmp(stateText, "playing") == 0 || std::strcmp(stateText, "idle") == 0)) {
        strokeRelativeToDepth_ = true;
        updateCollapsedRangeSupport();
    }

    observedStateValid_ = true;
    observedStateCategory_ = classifyMachineState(observed.state);
    xQueueOverwrite(observedStateMailbox_, &observed);

    LOGD(kTag, "Observed state: %s", observed.state);
}

// Collapse protocol-level state strings into the small set of states the worker can act on.
OssmClientWorker::MachineStateCategory
OssmClientWorker::classifyMachineState(const char* state) const {
    if (!state || state[0] == '\0') {
        return MachineStateCategory::NoUsableState;
    }

    if (std::strcmp(state, "menu.idle") == 0 ||  // Official
        std::strcmp(state, "menu") == 0 ||       // OSSM-RS (old)
        std::strcmp(state, "idle") == 0 ||       // OSSM-RS (current)
        std::strcmp(state, "ready") == 0) {      // OSSM-RS (current)
        return MachineStateCategory::MenuReady;
    }

    if (std::strcmp(state, "strokeEngine.idle") == 0 ||     // Official
        std::strcmp(state, "strokeEngine.pattern") == 0 ||  // Official
        std::strcmp(state, "strokeEngine") == 0 ||          // OSSM-RS (old)
        std::strcmp(state, "playing") == 0) {               // OSSM-RS (current)
        return MachineStateCategory::MotionReady;
    }

    if (std::strcmp(state, "strokeEngine.preflight") == 0) {
        return MachineStateCategory::SpeedKnobBlocked;
    }

    if (startsWith(state, "homing")) {
        return MachineStateCategory::Waiting;
    }

    return MachineStateCategory::UnsupportedBlocked;
}

bool OssmClientWorker::writeCommand(const char* command) {
    if (!commandCharacteristic_ || !command)
        return false;

    const bool response = !commandCharacteristic_->canWriteNoResponse();
    const bool success = writeTextValue(*commandCharacteristic_, command, response);
    logRemoteWrite("command", command, success);
    return success;
}

bool OssmClientWorker::writeSetCommand(const char* field, int value) {
    if (!field || value < 0 || value > 100)
        return false;

    char command[32] = {};
    const int written = snprintf(command, sizeof(command), "set:%s:%d", field, value);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(command))
        return false;

    return writeCommand(command);
}

bool OssmClientWorker::writePatternCommand(int patternId) {
    if (patternId < 0)
        return false;

    char command[32] = {};
    const int written = snprintf(command, sizeof(command), "set:pattern:%d", patternId);
    if (written <= 0 || static_cast<size_t>(written) >= sizeof(command))
        return false;

    return writeCommand(command);
}

const char* OssmClientWorker::modeFailureName(ModeFailure failure) {
    switch (failure) {
        case ModeFailure::None:
            return "none";
        case ModeFailure::CommandWriteFailed:
            return "command-write-failed";
        case ModeFailure::UnsupportedState:
            return "unsupported-state";
        case ModeFailure::TimedOut:
            return "timed-out";
        case ModeFailure::ReadinessLost:
            return "readiness-lost";
    }

    return "unknown";
}

const char* OssmClientWorker::machineStateCategoryName(MachineStateCategory category) {
    switch (category) {
        case MachineStateCategory::NoUsableState:
            return "no-usable-state";
        case MachineStateCategory::MenuReady:
            return "menu-ready";
        case MachineStateCategory::MotionReady:
            return "motion-ready";
        case MachineStateCategory::Waiting:
            return "waiting";
        case MachineStateCategory::SpeedKnobBlocked:
            return "speed-knob-blocked";
        case MachineStateCategory::UnsupportedBlocked:
            return "unsupported-blocked";
    }

    return "unknown";
}

void OssmClientWorker::recordError(int error) {
    lastError_.store(error);
}

void OssmClientWorker::logRemoteWrite(const char* target, const char* payload, bool success) const {
    const char* targetName = target ? target : "unknown";
    const char* payloadText = payload ? payload : "";
    if (success) {
        LOGD(kTag, "Write %s: %s -> ok", targetName, payloadText);
    } else {
        LOGW(kTag, "Write %s: %s -> fail", targetName, payloadText);
    }
}

}  // namespace ossm
