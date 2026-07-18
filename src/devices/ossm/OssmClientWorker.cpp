#include "OssmClientWorker.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cmath>
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

bool tryReadFloat(JsonVariantConst value, float& result) {
    if (!value.is<float>())
        return false;

    const float number = value.as<float>();
    if (!std::isfinite(number))
        return false;

    result = number;
    return true;
}

bool writeTextValue(
    const NimBLERemoteCharacteristic& characteristic, const char* payload, bool response) {
    return payload && characteristic.writeValue(payload, std::strlen(payload), response);
}

#ifdef SERIAL_INFO
void logPayload(const uint8_t* data, size_t length) {
    Serial.print("OSSM state payload text: \"");
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

    Serial.print("OSSM state payload hex:");
    for (size_t index = 0; index < length; ++index) {
        Serial.printf(" %02X", static_cast<unsigned>(data[index]));
    }
    Serial.println();
}
#endif
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
                        lastWrittenMotion_.speed = 0;
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

void OssmClientWorker::applyRequestedState(const OssmClient::RequestedState& incoming) {
    OssmClient::RequestedState accepted = incoming;
    if (accepted.speed > 0 &&
        (accepted.speedValidityEpoch != speedValidityEpoch_.load() || !motionReady())) {
        accepted.speed = 0;
    }
    requested_ = accepted;
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
    lastWrittenMotion_ = {};
    motionBaselineWritten_ = false;
    nextMotionWriteAt_ = 0;

    if (initialized_) {
        xQueueReset(stateNotificationMailbox_);
        xQueueReset(observedStateMailbox_);
        xQueueReset(patternMailbox_);
    }
}

void OssmClientWorker::setModeState(OssmClient::ModeState state) {
    modeState_.store(state);
    setMotionReady(state == OssmClient::ModeState::Ready);
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

void OssmClientWorker::failMode(ModeFailure failure) {
    modeOperation_.failure = failure;
    Serial.printf(
        "OSSM mode failure: cause=%s category=%s error=%d\n",
        modeFailureName(failure),
        machineStateCategoryName(observedStateCategory_),
        lastError_.load());
    setModeState(OssmClient::ModeState::Failed);
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

void OssmClientWorker::resetModeOperation() {
    modeOperation_ = {};
    setModeState(OssmClient::ModeState::Idle);
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

void OssmClientWorker::reconcileUrgentStopRequest() {
    if (requested_.speed != 0 || !motionReady()) {
        return;
    }

    if (writeSetCommand("speed", 0)) {
        lastWrittenMotion_.speed = 0;
    }
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
        return false;
    }

    if (!document.is<JsonArray>()) {
        Serial.println("OSSM pattern list parse failed: root is not an array");
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
            error.c_str(), static_cast<unsigned>(notification.length), firstByte, lastByte);
#ifdef SERIAL_INFO
        logPayload(notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    if (!document.is<JsonObject>()) {
        Serial.println("OSSM state notification parse failed: root is not an object");
#ifdef SERIAL_INFO
        logPayload(notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    const JsonVariantConst timestamp = document["timestamp"];
    const JsonVariantConst state = document["state"];
    const JsonVariantConst speed = document["speed"];
    const JsonVariantConst stroke = document["stroke"];
    const JsonVariantConst depth = document["depth"];
    const JsonVariantConst sensation = document["sensation"];
    const JsonVariantConst buffer = document["buffer"];
    const JsonVariantConst pattern = document["pattern"];
    const JsonVariantConst position = document["position"];
    const JsonVariantConst sessionId = document["sessionId"];
    const bool hasPosition = position.is<JsonVariantConst>();
    const bool hasSessionId = sessionId.is<JsonVariantConst>();
    float speedValue = 0;
    float strokeValue = 0;
    float depthValue = 0;
    float sensationValue = 0;
    float bufferValue = 0;
    const bool speedValid = tryReadFloat(speed, speedValue);
    const bool strokeValid = tryReadFloat(stroke, strokeValue);
    const bool depthValid = tryReadFloat(depth, depthValue);
    const bool sensationValid = tryReadFloat(sensation, sensationValue);
    const bool bufferValid = tryReadFloat(buffer, bufferValue);

    if (!timestamp.is<uint32_t>() || !state.is<const char*>() || !speedValid ||
        !strokeValid || !depthValid || !sensationValid || !bufferValid || !pattern.is<int>() ||
        (hasPosition && !position.is<float>()) ||
        (hasSessionId && !sessionId.is<const char*>())) {
        Serial.println("OSSM state notification parse failed: invalid field");
#ifdef SERIAL_INFO
        if (!timestamp.is<uint32_t>())
            Serial.println("OSSM invalid field: timestamp");
        if (!state.is<const char*>())
            Serial.println("OSSM invalid field: state");
        if (!speedValid)
            Serial.println("OSSM invalid field: speed");
        if (!strokeValid)
            Serial.println("OSSM invalid field: stroke");
        if (!depthValid)
            Serial.println("OSSM invalid field: depth");
        if (!sensationValid)
            Serial.println("OSSM invalid field: sensation");
        if (!bufferValid)
            Serial.println("OSSM invalid field: buffer");
        if (!pattern.is<int>())
            Serial.println("OSSM invalid field: pattern");
        if (hasPosition && !position.is<float>())
            Serial.println("OSSM invalid field: position");
        if (hasSessionId && !sessionId.is<const char*>())
            Serial.println("OSSM invalid field: sessionId");
        logPayload(notification.data, notification.length);
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
        logPayload(notification.data, notification.length);
#endif
        observedStateValid_ = false;
        observedStateCategory_ = MachineStateCategory::NoUsableState;
        return;
    }

    OssmClient::ObservedState observed{};
    observed.timestamp = timestamp.as<uint32_t>();
    std::memcpy(observed.state, stateText, stateLength + 1);
    observed.speed = speedValue;
    observed.stroke = strokeValue;
    observed.depth = depthValue;
    observed.sensation = sensationValue;
    observed.buffer = bufferValue;
    observed.pattern = pattern.as<int>();
    if (hasPosition)
        observed.position = position.as<float>();
    if (hasSessionId) {
        const char* sessionIdText = sessionId.as<const char*>();
        const size_t sessionIdLength = std::strlen(sessionIdText);
        if (sessionIdLength == 0 ||
            sessionIdLength >= OssmClient::kObservedSessionIdCapacity) {
            Serial.printf(
                "OSSM state notification parse failed: invalid session length=%u\n",
                static_cast<unsigned>(sessionIdLength));
#ifdef SERIAL_INFO
            logPayload(notification.data, notification.length);
#endif
            observedStateValid_ = false;
            observedStateCategory_ = MachineStateCategory::NoUsableState;
            return;
        }
        std::memcpy(observed.sessionId, sessionIdText, sessionIdLength + 1);
    }

    observedStateValid_ = true;
    observedStateCategory_ = classifyMachineState(observed.state);
    xQueueOverwrite(observedStateMailbox_, &observed);

#ifdef SERIAL_INFO
    Serial.printf(
        "OSSM observed: %s speed=%.2f stroke=%.2f depth=%.2f sensation=%.2f "
        "buffer=%.2f pattern=%d position=%.2f session=%s timestamp=%lu\n",
        observed.state,
        observed.speed,
        observed.stroke,
        observed.depth,
        observed.sensation,
        observed.buffer,
        observed.pattern,
        observed.position,
        observed.sessionId,
        static_cast<unsigned long>(observed.timestamp));
#endif
}

// Collapse protocol-level state strings into the small set of states the worker can act on.
OssmClientWorker::MachineStateCategory
OssmClientWorker::classifyMachineState(const char* state) const {
    if (!state || state[0] == '\0') {
        return MachineStateCategory::NoUsableState;
    }

    if (std::strcmp(state, "menu") == 0 || std::strcmp(state, "menu.idle") == 0 ||
        std::strcmp(state, "idle") == 0) {
        return MachineStateCategory::MenuReady;
    }

    if (std::strcmp(state, "strokeEngine.idle") == 0 ||
        std::strcmp(state, "strokeEngine.pattern") == 0) {
        return MachineStateCategory::MotionReady;
    }

    if (std::strcmp(state, "strokeEngine.preflight") == 0) {
        return MachineStateCategory::SpeedKnobBlocked;
    }

    if (std::strcmp(state, "strokeEngine") == 0 || startsWith(state, "homing")) {
        return MachineStateCategory::Waiting;
    }

    return MachineStateCategory::UnsupportedBlocked;
}

void OssmClientWorker::recordError(int error) {
    lastError_.store(error);
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

    bool attemptedWrite = false;
    bool initialWriteComplete = true;

    if (!motionBaselineWritten_ || requested_.speed != lastWrittenMotion_.speed) {
        attemptedWrite = true;
        if (writeSetCommand("speed", requested_.speed)) {
            lastWrittenMotion_.speed = requested_.speed;
        } else if (!motionBaselineWritten_) {
            initialWriteComplete = false;
        }
    }

    if (!motionBaselineWritten_ || requested_.stroke != lastWrittenMotion_.stroke) {
        attemptedWrite = true;
        if (writeSetCommand("stroke", requested_.stroke)) {
            lastWrittenMotion_.stroke = requested_.stroke;
        } else if (!motionBaselineWritten_) {
            initialWriteComplete = false;
        }
    }

    if (!motionBaselineWritten_ || requested_.depth != lastWrittenMotion_.depth) {
        attemptedWrite = true;
        if (writeSetCommand("depth", requested_.depth)) {
            lastWrittenMotion_.depth = requested_.depth;
        } else if (!motionBaselineWritten_) {
            initialWriteComplete = false;
        }
    }

    if (!motionBaselineWritten_ || requested_.sensation != lastWrittenMotion_.sensation) {
        attemptedWrite = true;
        if (writeSetCommand("sensation", requested_.sensation)) {
            lastWrittenMotion_.sensation = requested_.sensation;
        } else if (!motionBaselineWritten_) {
            initialWriteComplete = false;
        }
    }

    if (!motionBaselineWritten_ || requested_.pattern != lastWrittenMotion_.pattern) {
        attemptedWrite = true;
        if (writePatternCommand(requested_.pattern)) {
            lastWrittenMotion_.pattern = requested_.pattern;
        } else if (!motionBaselineWritten_) {
            initialWriteComplete = false;
        }
    }

    if (!motionBaselineWritten_ && initialWriteComplete) {
        motionBaselineWritten_ = true;
    }

    if (attemptedWrite) {
        nextMotionWriteAt_ = xTaskGetTickCount() + OssmClient::kMotionWriteInterval;
    }
}

bool OssmClientWorker::hasDirtyMotion() const {
    return !motionBaselineWritten_ || requested_.speed != lastWrittenMotion_.speed ||
           requested_.stroke != lastWrittenMotion_.stroke ||
           requested_.depth != lastWrittenMotion_.depth ||
           requested_.sensation != lastWrittenMotion_.sensation ||
           requested_.pattern != lastWrittenMotion_.pattern;
}

TickType_t OssmClientWorker::nextWakeAt() const {
    TickType_t wakeAt = nextReconcileAt_;
    if (motionReady() && hasDirtyMotion() && nextMotionWriteAt_ < wakeAt) {
        wakeAt = nextMotionWriteAt_;
    }
    return wakeAt;
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
