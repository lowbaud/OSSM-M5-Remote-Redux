#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include <lvgl.h>

#include "devices/ossm/OssmDiscovery.h"
#include "platform/RemoteInput.h"

namespace m5_redux {

enum class ScanScreenAction : std::uint8_t {
    None,
    Cancel,
    Connect,
};

class ScanScreen {
  public:
    explicit ScanScreen(ossm::OssmDiscovery& discovery);

    void begin();
    void enter();
    void leave();
    ScanScreenAction update(const RemoteInputEvents& events);

    void requestCancel();
    void requestConnect();
    bool selectedDevice(ossm::DiscoveredOssm& device) const;

  private:
    enum class State : std::uint8_t {
        Idle,
        Scanning,
        ScanFailed,
    };

    struct DisplayedDevice {
        ossm::DiscoveredOssm latest{};
        int filteredRssi = 0;
        int renderedRssi = 0;
        std::uint32_t renderedAtMs = 0;
    };

    static constexpr std::size_t kNoSelection = static_cast<std::size_t>(-1);
    static constexpr std::uint32_t kRssiRefreshIntervalMs = 1000;
    static constexpr int kRssiFilterDivisor = 4;

    ossm::OssmDiscovery& discovery_;
    std::vector<lv_obj_t*> deviceRows_;
    std::vector<DisplayedDevice> displayedDevices_;
    lv_obj_t* activitySpinner_ = nullptr;
    std::size_t selectedIndex_ = kNoSelection;
    ScanScreenAction pendingAction_ = ScanScreenAction::None;
    State state_ = State::Idle;

    void startFreshScan();
    void setState(State state);
    void updateScanResults();
    void selectDevice(std::size_t index);
    void updateConnectButton();
    void setStatusForDeviceCount(std::size_t count);
    void handleDeviceClicked(lv_obj_t* row);

    static void handleDeviceRowEvent(lv_event_t* event);
};

}  // namespace m5_redux
