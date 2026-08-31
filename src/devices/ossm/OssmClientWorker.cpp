#include "OssmClientWorker.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

namespace ossm {

namespace {
const NimBLEUUID kOssmServiceUuid("522B443A-4F53-534D-0001-420BADBABE69");
const NimBLEUUID kCommandCharacteristicUuid("522B443A-4F53-534D-1000-420BADBABE69");
const NimBLEUUID kSpeedKnobCharacteristicUuid("522B443A-4F53-534D-1010-420BADBABE69");
const NimBLEUUID kStateCharacteristicUuid("522B443A-4F53-534D-2000-420BADBABE69");
const NimBLEUUID kPatternListCharacteristicUuid("522B443A-4F53-534D-3000-420BADBABE69");
constexpr const char* kSpeedKnobDisabled = "false";

bool startsWith(const char* value, const char* prefix) {
    return value && prefix && std::strncmp(value, prefix, std::strlen(prefix)) == 0;
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

void logPayload(const char* label, const uint8_t* data, size_t length) {
    Serial.printf("%s text: \"", label);
    for (size_t index = 0; index < length; ++index) {
        const uint8_t byte = data[index];
        if (byte == '\\' || byte == '"') {
            Serial.write('\\');
            Serial.write(byte);
        } else if (byte >= 0x20 && byte <= 0x7e) {
            Serial.write(byte);
        } else {
            Serial.printf("\\x%02X", static_cast<unsigned>(byte));
        }
    }
    Serial.println("\"");

    Serial.printf("%s hex:", label);
    for (size_t index = 0; index < length; ++index) {
        Serial.printf(" %02X", static_cast<unsigned>(data[index]));
    }
    Serial.println();
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
    std::atomic<int>& lastError)
    : connectionState_(connectionState), modeState_(modeState), ready_(ready),
      speedValidityEpoch_(speedValidityEpoch), lastError_(lastError), callbacks_(*this) {}

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
    for (;;) {
        const TickType_t now = xTaskGetTickCount();
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

    if (!motionWriteState_.speed.matchesLastWrite(requested_.speed)) {
        if (writeSetCommand("speed", requested_.speed)) {
            motionWriteState_.speed.recordSuccessfulWrite(requested_.speed);
        }
    }

    if (!motionWriteState_.stroke.matchesLastWrite(requested_.stroke)) {
        if (writeSetCommand("stroke", requested_.stroke)) {
            motionWriteState_.stroke.recordSuccessfulWrite(requested_.stroke);
        }
    }

    if (!motionWriteState_.depth.matchesLastWrite(requested_.depth)) {
        if (writeSetCommand("depth", requested_.depth)) {
            motionWriteState_.depth.recordSuccessfulWrite(requested_.depth);
        }
    }

    if (!motionWriteState_.sensation.matchesLastWrite(requested_.sensation)) {
        if (writeSetCommand("sensation", requested_.sensation)) {
            motionWriteState_.sensation.recordSuccessfulWrite(requested_.sensation);
        }
    }

    if (!motionWriteState_.pattern.matchesLastWrite(requested_.pattern)) {
        if (writePatternCommand(requested_.pattern)) {
            motionWriteState_.pattern.recordSuccessfulWrite(requested_.pattern);
        }
    }

    nextMotionWriteAt_ = xTaskGetTickCount() + OssmClient::kMotionWriteInterval;
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
    return !motionWriteState_.speed.matchesLastWrite(requested_.speed) ||
           !motionWriteState_.stroke.matchesLastWrite(requested_.stroke) ||
           !motionWriteState_.depth.matchesLastWrite(requested_.depth) ||
           !motionWriteState_.sensation.matchesLastWrite(requested_.sensation) ||
           !motionWriteState_.pattern.matchesLastWrite(requested_.pattern);
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
    Serial.printf("OSSM negotiated MTU: %u\n", client_->getMTU());

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
    Serial.println("OSSM pattern list characteristic lookup started");
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
        Serial.println("OSSM pattern list characteristic missing");
        abortInitialization(OssmClient::kPatternListCharacteristicError);
        return false;
    }

    if (!patternList->canRead()) {
        Serial.println("OSSM pattern list characteristic found but not readable");
        abortInitialization(OssmClient::kPatternListCharacteristicError);
        return false;
    }
    Serial.println("OSSM pattern list characteristic accepted for read");

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

    if (!loadPatterns()) {
        abortInitialization(OssmClient::kPatternListCharacteristicError);
        return false;
    }

    resetModeOperation();
    Serial.println("OSSM connected; reading machine state");
    readInitialState();
    if (!observedStateValid_) {
        Serial.println("OSSM connected; waiting for machine state");
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
    Serial.printf(
        "OSSM mode failure: cause=%s category=%s error=%d\n",
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
        Serial.println("OSSM initial state read returned no data");
        return;
    }

    if (length > kStateNotificationCapacity) {
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        Serial.printf(
            "OSSM initial state read too large: %u bytes\n", static_cast<unsigned>(length));
        return;
    }

    StateNotification notification{};
    notification.length = length;
    std::memcpy(notification.data, value.data(), length);
    parseStateNotification(notification);
}

bool OssmClientWorker::loadPatterns() {
    if (!patternListCharacteristic_) {
        Serial.println("OSSM pattern load failed: missing characteristic");
        return false;
    }

    if (!patternMailbox_) {
        Serial.println("OSSM pattern load failed: missing mailbox");
        return false;
    }

    const NimBLEAttValue value = patternListCharacteristic_->readValue();
    const size_t length = value.length();
    if (length == 0) {
        Serial.println("OSSM pattern list read returned no data");
        return false;
    }

    JsonDocument document;
    const DeserializationError error = deserializeJson(document, value.data(), length);
    if (error) {
        Serial.printf("OSSM pattern list parse failed: %s\n", error.c_str());
        logPayload("OSSM pattern list contents", value.data(), length);
        return false;
    }

    if (!document.is<JsonArray>()) {
        Serial.println("OSSM pattern list parse failed: root is not an array");
        logPayload("OSSM pattern list contents", value.data(), length);
        return false;
    }

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

        if (patterns.count >= OssmClient::kMaxPatternCount) {
            ++skippedEntries;
            continue;
        }

        OssmClient::PatternInfo& pattern = patterns.patterns[patterns.count++];
        pattern.id = patternId;
        std::memcpy(pattern.name, patternName, nameLength + 1);
    }

    if (patterns.count == 0) {
        Serial.printf(
            "OSSM pattern list contained no usable entries; inspected=%u skipped=%u\n",
            static_cast<unsigned>(inspectedEntries),
            static_cast<unsigned>(skippedEntries));
        return false;
    }

    xQueueOverwrite(patternMailbox_, &patterns);
    Serial.printf("OSSM loaded %u patterns", static_cast<unsigned>(patterns.count));
    if (skippedEntries > 0) {
        Serial.printf(" (%u skipped)", static_cast<unsigned>(skippedEntries));
    }
    Serial.println();
    return true;
}

void OssmClientWorker::parseStateNotification(const StateNotification& notification) {
    // The state characteristic also carries plain-text command responses.
    if (startsWith(notification.data, notification.length, "ok:")) {
#ifdef SERIAL_INFO
        Serial.printf(
            "OSSM ignored protocol response: %.*s\n",
            static_cast<int>(notification.length),
            reinterpret_cast<const char*>(notification.data));
#endif
        return;
    }

    if (startsWith(notification.data, notification.length, "fail:")) {
        Serial.printf(
            "OSSM protocol failure response: %.*s\n",
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
        Serial.printf(
            "OSSM state notification parse failed: %s; length=%u first=0x%02X last=0x%02X\n",
            error.c_str(),
            static_cast<unsigned>(notification.length),
            firstByte,
            lastByte);
#ifdef SERIAL_INFO
        logPayload("OSSM state payload", notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    if (!document.is<JsonObject>()) {
        Serial.println("OSSM state notification parse failed: root is not an object");
#ifdef SERIAL_INFO
        logPayload("OSSM state payload", notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    const JsonVariantConst state = document["state"];
    if (!state.is<const char*>()) {
        Serial.println("OSSM state notification parse failed: missing or invalid state");
#ifdef SERIAL_INFO
        logPayload("OSSM state payload", notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    const char* stateText = state.as<const char*>();
    const size_t stateLength = std::strlen(stateText);
    if (stateLength == 0 || stateLength >= OssmClient::kObservedStateCapacity) {
        Serial.printf(
            "OSSM state notification parse failed: invalid state length=%u\n",
            static_cast<unsigned>(stateLength));
#ifdef SERIAL_INFO
        logPayload("OSSM state payload", notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    OssmClient::ObservedState observed{};
    std::memcpy(observed.state, stateText, stateLength + 1);

    observedStateValid_ = true;
    observedStateCategory_ = classifyMachineState(observed.state);
    xQueueOverwrite(observedStateMailbox_, &observed);

#ifdef SERIAL_INFO
    Serial.printf("OSSM observed state: %s\n", observed.state);
#endif
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
#ifndef SERIAL_INFO
    if (success)
        return;
#endif

    Serial.printf(
        "OSSM write[%lu] %s: %s -> %s\n",
        static_cast<unsigned long>(millis()),
        target ? target : "unknown",
        payload ? payload : "",
        success ? "ok" : "fail");
}

}  // namespace ossm
