#include "OssmClient.h"
#include "OssmClientWorker.h"
#include <new>

namespace {
int clampPercent(int value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return value;
}
} // namespace

bool OssmClient::begin() {
    if (initialized_) return true;

    if (!worker_) {
        worker_ = new (std::nothrow)
            OssmClientWorker(connectionState_, modeState_, ready_, speedValidityEpoch_,
                             lastError_);
        if (!worker_) {
            lastError_.store(kWorkerStartError);
            return false;
        }
    }

    if (!worker_->begin()) return false;

    const BaseType_t taskCreated = xTaskCreate(workerEntry, "ossm-worker", kWorkerStackSize,
                                               worker_, kWorkerPriority, &workerTask_);
    if (taskCreated != pdPASS) {
        workerTask_ = nullptr;
        lastError_.store(kWorkerStartError);
        return false;
    }

    lastError_.store(kNoError);
    initialized_ = true;
    return true;
}

bool OssmClient::connect(const NimBLEAddress& address) {
    if (!initialized_ || address.isNull()) return false;

    ConnectionState expected = ConnectionState::Disconnected;
    if (!connectionState_.compare_exchange_strong(expected, ConnectionState::Connecting)) {
        return false;
    }

    requestedState_.connection.generation = ++nextConnectionGeneration_;
    requestedState_.connection.action = ConnectionAction::Connect;
    requestedState_.connection.address = address;
    requestedState_.mode = {};
    modeState_.store(ModeState::Idle);
    requestedState_.speed = 0;
    requestedState_.speedValidityEpoch = speedValidityEpoch_.load();
    lastError_.store(kNoError);
    if (publishRequestedState()) return true;

    expected = ConnectionState::Connecting;
    connectionState_.compare_exchange_strong(expected, ConnectionState::Disconnected);
    return false;
}

bool OssmClient::disconnect() {
    if (!initialized_) return false;

    ConnectionState state = connectionState_.load();
    if (state == ConnectionState::Disconnected || state == ConnectionState::Disconnecting) {
        return true;
    }

    if (!connectionState_.compare_exchange_strong(state, ConnectionState::Disconnecting)) {
        return false;
    }

    const ModeRequest previousModeRequest = requestedState_.mode;
    const ModeState previousModeState = modeState_.exchange(ModeState::Idle);
    requestedState_.connection.generation = ++nextConnectionGeneration_;
    requestedState_.connection.action = ConnectionAction::Disconnect;
    requestedState_.speed = 0;
    requestedState_.speedValidityEpoch = speedValidityEpoch_.load();
    requestedState_.mode = {};
    if (publishRequestedState()) return true;

    requestedState_.mode = previousModeRequest;
    modeState_.store(previousModeState);
    ConnectionState expected = ConnectionState::Disconnecting;
    connectionState_.compare_exchange_strong(expected, state);
    return false;
}

bool OssmClient::enterStrokeEngine() {
    if (!initialized_ || connectionState_.load() != ConnectionState::Connected) {
        return false;
    }

    ModeState state = modeState_.load();
    if (state == ModeState::Entering || state == ModeState::SpeedKnobBlocked ||
        state == ModeState::Ready) {
        return true;
    }

    if (!modeState_.compare_exchange_strong(state, ModeState::Entering)) {
        return false;
    }

    const ModeRequest previousRequest = requestedState_.mode;
    requestedState_.mode.generation = ++nextModeGeneration_;
    requestedState_.mode.target = ModeTarget::StrokeEngine;
    if (publishRequestedState()) return true;

    requestedState_.mode = previousRequest;
    ModeState expected = ModeState::Entering;
    modeState_.compare_exchange_strong(expected, state);
    return false;
}

OssmClient::ConnectionState OssmClient::connectionState() const {
    return connectionState_.load();
}

OssmClient::ModeState OssmClient::modeState() const { return modeState_.load(); }

bool OssmClient::isReady() const {
    return connectionState_.load() == ConnectionState::Connected &&
           modeState_.load() == ModeState::Ready && ready_.load();
}

int OssmClient::lastError() const { return lastError_.load(); }

bool OssmClient::latestObservedState(ObservedState& out) const {
    return initialized_ && worker_ && worker_->latestObservedState(out);
}

bool OssmClient::patternList(PatternList& out) const {
    return initialized_ && worker_ && worker_->patternList(out);
}

bool OssmClient::setSpeed(int speed) {
    if (!initialized_) return false;

    const int requestedSpeed = clampPercent(speed);
    uint32_t epoch = speedValidityEpoch_.load();
    if (requestedSpeed > 0) {
        if (connectionState_.load() != ConnectionState::Connected ||
            modeState_.load() != ModeState::Ready || !ready_.load()) {
            return false;
        }

        const uint32_t confirmedEpoch = speedValidityEpoch_.load();
        if (confirmedEpoch != epoch ||
            connectionState_.load() != ConnectionState::Connected ||
            modeState_.load() != ModeState::Ready || !ready_.load()) {
            return false;
        }
        epoch = confirmedEpoch;
    }

    requestedState_.speed = requestedSpeed;
    requestedState_.speedValidityEpoch = epoch;
    return publishRequestedState();
}

void OssmClient::setDepth(int depth) {
    if (!initialized_) return;

    requestedState_.depth = clampPercent(depth);
    publishRequestedState();
}

void OssmClient::setStroke(int stroke) {
    if (!initialized_) return;

    requestedState_.stroke = clampPercent(stroke);
    publishRequestedState();
}

void OssmClient::setSensation(int sensation) {
    if (!initialized_) return;

    requestedState_.sensation = clampPercent(sensation);
    publishRequestedState();
}

void OssmClient::setPattern(int patternId) {
    if (!initialized_) return;

    requestedState_.pattern = patternId < 0 ? 0 : patternId;
    publishRequestedState();
}

void OssmClient::stop() {
    if (!initialized_) return;

    setSpeed(0);
}

void OssmClient::workerEntry(void* context) {
    OssmClientWorker* worker = static_cast<OssmClientWorker*>(context);
    worker->loop();
    vTaskDelete(nullptr);
}

bool OssmClient::publishRequestedState() {
    return initialized_ && worker_ && worker_->publishRequestedState(requestedState_);
}
