#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <NimBLEDevice.h>

class OssmDiscovery;

class OssmScanCallbacks : public NimBLEScanCallbacks {
  public:
    explicit OssmScanCallbacks(OssmDiscovery& discovery);

    void onResult(const NimBLEAdvertisedDevice* device) override;

  private:
    OssmDiscovery& discovery_;
};

struct DiscoveredOssm {
    NimBLEAddress address;
    char name[32] = {};
    int rssi = 0;
    uint32_t lastSeenMs = 0;
};

static_assert(std::is_trivially_copyable<DiscoveredOssm>::value,
              "DiscoveredOssm must be safe to copy through a FreeRTOS queue");

class OssmDiscovery {
  public:
    OssmDiscovery();
    OssmDiscovery(const OssmDiscovery&) = delete;
    OssmDiscovery& operator=(const OssmDiscovery&) = delete;
    OssmDiscovery(OssmDiscovery&&) = delete;
    OssmDiscovery& operator=(OssmDiscovery&&) = delete;

    bool begin();

    bool startScan(uint32_t durationMs = 0);
    bool stopScan();
    bool isScanning() const;
    void update();

    size_t deviceCount() const;
    bool deviceAt(size_t index, DiscoveredOssm& result) const;

    void handleAdvertisedDevice(const NimBLEAdvertisedDevice* device);

  private:
    static constexpr size_t kMaxDevices = 8;
    static constexpr size_t kEventQueueCapacity = 16;

    NimBLEScan* scan_ = nullptr;
    OssmScanCallbacks scanCallbacks_;
    QueueHandle_t eventQueue_ = nullptr;

    std::vector<DiscoveredOssm> devices_;
    uint32_t scanStartedAtMs_ = 0;
    uint32_t scanDurationMs_ = 0;

    void upsertDevice(const DiscoveredOssm& discovered);
    bool isOssmDevice(const NimBLEAdvertisedDevice* device) const;

    void printDevices() const;
};
