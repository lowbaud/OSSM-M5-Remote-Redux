#include "OssmDiscovery.h"
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include <string>

namespace ossm {

namespace {
const NimBLEUUID kOssmServiceUuid("522B443A-4F53-534D-0001-420BADBABE69");
}

OssmScanCallbacks::OssmScanCallbacks(OssmDiscovery& discovery) : discovery_(discovery) {}

void OssmScanCallbacks::onResult(const NimBLEAdvertisedDevice* device) {
    discovery_.handleAdvertisedDevice(device);
}

OssmDiscovery::OssmDiscovery() : scanCallbacks_(*this) {
    devices_.reserve(kMaxDevices);
}

bool OssmDiscovery::begin() {
    if (!eventQueue_) {
        eventQueue_ = xQueueCreate(kEventQueueCapacity, sizeof(DiscoveredOssm));
        if (!eventQueue_)
            return false;
    }

    scan_ = NimBLEDevice::getScan();
    if (!scan_)
        return false;

    scan_->setScanCallbacks(&scanCallbacks_, true);
    scan_->setActiveScan(true);
    scan_->setInterval(100);
    scan_->setWindow(80);
    // Results are copied into the event queue, so NimBLE does not need to retain them.
    scan_->setMaxResults(0);
    return true;
}

bool OssmDiscovery::startScan(uint32_t durationMs) {
    if (!scan_ || !eventQueue_)
        return false;
    if (isScanning())
        return false;

    xQueueReset(eventQueue_);
    devices_.clear();

    if (!scan_->start(0, false)) {
        return false;
    }

    scanStartedAtMs_ = millis();
    scanDurationMs_ = durationMs;

    Serial.println("Scan started");

    return true;
}

bool OssmDiscovery::stopScan() {
    if (!scan_)
        return false;
    if (!isScanning())
        return true;

    if (!scan_->stop()) {
        return false;
    }

    scanStartedAtMs_ = 0;
    scanDurationMs_ = 0;

    Serial.println("Scan stopped");

    return true;
}

void OssmDiscovery::update() {
    DiscoveredOssm discovered;
    while (eventQueue_ && xQueueReceive(eventQueue_, &discovered, 0) == pdPASS) {
        upsertDevice(discovered);
    }

    if (!isScanning())
        return;

    if (scanDurationMs_ > 0 && millis() - scanStartedAtMs_ >= scanDurationMs_) {
        stopScan();
    }
}

size_t OssmDiscovery::deviceCount() const {
    return devices_.size();
}

bool OssmDiscovery::deviceAt(size_t index, DiscoveredOssm& result) const {
    if (index >= devices_.size())
        return false;

    result = devices_[index];
    return true;
}

bool OssmDiscovery::isScanning() const {
    return scan_ && scan_->isScanning();
}

void OssmDiscovery::handleAdvertisedDevice(const NimBLEAdvertisedDevice* device) {
    if (!eventQueue_ || !device)
        return;
    if (!isOssmDevice(device))
        return;

    DiscoveredOssm discovered{};
    discovered.address = device->getAddress();
    discovered.rssi = device->getRSSI();
    discovered.lastSeenMs = millis();

    const std::string name = device->getName();
    const size_t nameLength = std::min(name.size(), sizeof(discovered.name) - 1);
    std::memcpy(discovered.name, name.data(), nameLength);
    discovered.name[nameLength] = '\0';

    xQueueSend(eventQueue_, &discovered, 0);
}

void OssmDiscovery::upsertDevice(const DiscoveredOssm& discovered) {
    for (auto& known : devices_) {
        if (known.address == discovered.address) {
            known = discovered;
            return;
        }
    }

    if (devices_.size() < kMaxDevices) {
        devices_.push_back(discovered);
    }
}

bool OssmDiscovery::isOssmDevice(const NimBLEAdvertisedDevice* device) const {
    return device && device->isAdvertisingService(kOssmServiceUuid);
}

void OssmDiscovery::printDevices() const {
    Serial.print("Devices found: ");
    Serial.println(devices_.size());

    for (const DiscoveredOssm& device : devices_) {
        Serial.print("Address: ");
        Serial.print(device.address.toString().c_str());

        Serial.print(" | Name: ");
        Serial.print(device.name);

        Serial.print(" | RSSI: ");
        Serial.print(device.rssi);

        Serial.print(" | Last seen ms: ");
        Serial.println(device.lastSeenMs);
    }
}

}  // namespace ossm
