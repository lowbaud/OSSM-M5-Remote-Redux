#include "OssmConnectionController.h"

#include <Arduino.h>
#include <NimBLEDevice.h>

namespace m5_redux {

OssmConnectionController::OssmConnectionController(ossm::OssmClient& client) : client_(client) {}

bool OssmConnectionController::begin(const char* localDeviceName) {
    NimBLEDevice::init(localDeviceName);

    initialized_ = client_.begin();

    if (!initialized_) {
        Serial.printf("OSSM client initialization failed: error=%d\n", client_.lastError());
        return false;
    }

    return true;
}

bool OssmConnectionController::connect(const NimBLEAddress& address) {
    if (!initialized_ ||
        client_.connectionState() != ossm::OssmClient::ConnectionState::Disconnected) {
        return false;
    }

    if (!client_.connect(address)) {
        return false;
    }

    connectionPending_ = true;
    failurePending_ = false;
    wasConnected_ = false;
    return true;
}

void OssmConnectionController::disconnect() {
    connectionPending_ = false;
    failurePending_ = false;
    wasConnected_ = false;
    wasReady_ = false;

    const ossm::OssmClient::ConnectionState state = client_.connectionState();
    if (state != ossm::OssmClient::ConnectionState::Disconnected &&
        state != ossm::OssmClient::ConnectionState::Disconnecting) {
        client_.disconnect();
    }
}

OssmConnectionEvents OssmConnectionController::update() {
    OssmConnectionEvents events;
    if (!initialized_) {
        return events;
    }

    const bool ready = client_.isReady();
    if (wasReady_ && !ready) {
        wasReady_ = false;
        events.readinessLost = true;
        failConnection();
        return events;
    }

    const ossm::OssmClient::ConnectionState connectionState = client_.connectionState();
    if (connectionState == ossm::OssmClient::ConnectionState::Disconnected) {
        wasConnected_ = false;
        if (connectionPending_ || failurePending_) {
            connectionPending_ = false;
            failurePending_ = false;
            events.connectionFailed = true;
        }
        return events;
    }

    if (connectionState == ossm::OssmClient::ConnectionState::Connecting ||
        connectionState == ossm::OssmClient::ConnectionState::Disconnecting) {
        return events;
    }

    if (!wasConnected_) {
        wasConnected_ = true;
        events.becameConnected = true;
    }

    const ossm::OssmClient::ModeState modeState = client_.modeState();
    if (modeState == ossm::OssmClient::ModeState::Failed) {
        failConnection();
        return events;
    }

    if (modeState == ossm::OssmClient::ModeState::Idle) {
        if (!client_.enterStrokeEngine()) {
            failConnection();
            return events;
        }
    }

    if (client_.isReady() && !wasReady_) {
        wasReady_ = true;
        connectionPending_ = false;
        failurePending_ = false;
        events.becameReady = true;
    }

    return events;
}

bool OssmConnectionController::isReady() const {
    return client_.isReady();
}

void OssmConnectionController::failConnection() {
    connectionPending_ = false;
    failurePending_ = true;

    const ossm::OssmClient::ConnectionState state = client_.connectionState();
    if (state != ossm::OssmClient::ConnectionState::Disconnected &&
        state != ossm::OssmClient::ConnectionState::Disconnecting) {
        client_.disconnect();
    }
}

}  // namespace m5_redux
