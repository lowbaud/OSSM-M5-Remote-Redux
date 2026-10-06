#include "ScanScreen.h"

#include <Arduino.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/ListRowStyle.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

const char* deviceName(const ossm::DiscoveredOssm& device) {
    return device.name[0] == '\0' ? "Unnamed OSSM" : device.name;
}

void formatSignalText(int rssi, char* text, std::size_t textSize) {
    std::snprintf(text, textSize, "%d dBm", rssi);
}

}  // namespace

ScanScreen::ScanScreen(ossm::OssmDiscovery& discovery) : discovery_(discovery) {}

void ScanScreen::begin() {
    styleList(objects.scan_device_list);
    setState(State::Idle);
}

void ScanScreen::enter() {
    if (lv_screen_active() != objects.scan) {
        loadScreen(SCREEN_ID_SCAN);
    }

    startFreshScan();
}

void ScanScreen::leave() {
    if (discovery_.isScanning()) {
        discovery_.stopScan();
    }

    pendingAction_ = ScanScreenAction::None;
    setState(State::Idle);
}

ScanScreenAction ScanScreen::update(const RemoteInputEvents& events) {
    if ((state_ == State::Starting || state_ == State::ScanFailed) &&
        millis() - lastScanAttemptAtMs_ >= kScanRetryIntervalMs) {
        tryStartScan();
    }

    if (state_ == State::Scanning) {
        discovery_.update();
        updateScanResults();

        const std::int32_t steps = events.menuSteps();
        if (!deviceRows_.empty() && steps != 0) {
            const std::int64_t current = selectedIndex_ == kNoSelection ? 0 : selectedIndex_;
            const std::int64_t last = static_cast<std::int64_t>(deviceRows_.size() - 1);
            std::int64_t next = current + steps;
            if (next < 0) {
                next = 0;
            } else if (next > last) {
                next = last;
            }
            selectDevice(static_cast<std::size_t>(next));
        }

        if (events.rightClick) {
            requestConnect();
        }
    }

    if (events.leftClick) {
        requestCancel();
    }

    const ScanScreenAction action = pendingAction_;
    pendingAction_ = ScanScreenAction::None;
    return action;
}

void ScanScreen::requestCancel() {
    pendingAction_ = ScanScreenAction::Cancel;
}

void ScanScreen::requestConnect() {
    if (state_ == State::Scanning && selectedIndex_ != kNoSelection) {
        pendingAction_ = ScanScreenAction::Connect;
    }
}

bool ScanScreen::selectedDevice(ossm::DiscoveredOssm& device) const {
    if (state_ != State::Scanning || selectedIndex_ == kNoSelection ||
        !discovery_.deviceAt(selectedIndex_, device)) {
        return false;
    }

    return true;
}

void ScanScreen::startFreshScan() {
    if (discovery_.isScanning()) {
        discovery_.stopScan();
    }

    lv_obj_clean(objects.scan_device_list);
    deviceRows_.clear();
    displayedDevices_.clear();
    selectedIndex_ = kNoSelection;
    pendingAction_ = ScanScreenAction::None;
    updateConnectButton();

    scanRequestedAtMs_ = millis();
    setState(State::Starting);
    lv_label_set_text(objects.scan_status_label, "Scanning for OSSM...");
    tryStartScan();
}

// Scanning is unavailable while a cancelled connection attempt is still winding down, so keep
// retrying instead of failing on the first attempt.
void ScanScreen::tryStartScan() {
    lastScanAttemptAtMs_ = millis();
    if (!discovery_.startScan()) {
        if (state_ == State::Starting &&
            lastScanAttemptAtMs_ - scanRequestedAtMs_ >= kScanStartGraceMs) {
            setState(State::ScanFailed);
            lv_label_set_text(objects.scan_status_label, "Unable to start scan");
        }
        return;
    }

    setState(State::Scanning);
    statusDeviceCount_ = 0;
    lv_label_set_text(objects.scan_status_label, "Scanning for OSSM...");
}

void ScanScreen::setState(State state) {
    state_ = state;
    const bool busy = state == State::Starting || state == State::Scanning;
    if (busy) {
        lv_obj_remove_flag(objects.scan_spinner, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(objects.scan_spinner, LV_OBJ_FLAG_HIDDEN);
    }
}

void ScanScreen::updateScanResults() {
    const std::size_t deviceCount = discovery_.deviceCount();
    const std::uint32_t now = millis();
    for (std::size_t index = 0; index < deviceCount; ++index) {
        ossm::DiscoveredOssm device;
        if (!discovery_.deviceAt(index, device)) {
            continue;
        }

        if (index >= deviceRows_.size()) {
            char signalText[16];
            formatSignalText(device.rssi, signalText, sizeof(signalText));

            DeviceRow row;
            row.button = lv_list_add_button(objects.scan_device_list, nullptr, nullptr);
            styleListRow(row.button);
            row.nameLabel = addListRowName(row.button, deviceName(device));
            row.signalLabel = addListRowValue(row.button, signalText);
            lv_obj_add_event_cb(row.button, handleDeviceRowEvent, LV_EVENT_CLICKED, this);
            deviceRows_.push_back(row);

            DisplayedDevice displayed;
            displayed.latest = device;
            displayed.filteredRssi = device.rssi;
            displayed.renderedRssi = device.rssi;
            displayed.renderedAtMs = now;
            displayedDevices_.push_back(displayed);

            if (selectedIndex_ == kNoSelection) {
                selectDevice(0);
            }
            continue;
        }

        DisplayedDevice& displayed = displayedDevices_[index];
        const bool nameChanged = std::strcmp(displayed.latest.name, device.name) != 0;

        if (displayed.latest.lastSeenMs != device.lastSeenMs) {
            // Keep fractional changes so small RSSI shifts can accumulate.
            displayed.filteredRssi += (device.rssi - displayed.filteredRssi) / kRssiFilterDivisor;
        }
        displayed.latest = device;

        const int roundedRssi = static_cast<int>(std::lround(displayed.filteredRssi));
        const bool rssiRefreshDue = roundedRssi != displayed.renderedRssi &&
                                    now - displayed.renderedAtMs >= kRssiRefreshIntervalMs;
        if (nameChanged) {
            lv_label_set_text(deviceRows_[index].nameLabel, deviceName(device));
        }
        if (rssiRefreshDue) {
            char signalText[16];
            formatSignalText(roundedRssi, signalText, sizeof(signalText));
            lv_label_set_text(deviceRows_[index].signalLabel, signalText);
            displayed.renderedRssi = roundedRssi;
            displayed.renderedAtMs = now;
        }
    }

    setStatusForDeviceCount(deviceCount);
}

void ScanScreen::selectDevice(std::size_t index) {
    if (index >= deviceRows_.size()) {
        return;
    }

    if (selectedIndex_ != kNoSelection && selectedIndex_ < deviceRows_.size()) {
        setListRowSelected(deviceRows_[selectedIndex_].button, false);
    }

    selectedIndex_ = index;
    setListRowSelected(deviceRows_[selectedIndex_].button, true);
    lv_obj_scroll_to_view(deviceRows_[selectedIndex_].button, LV_ANIM_ON);
    updateConnectButton();
}

void ScanScreen::updateConnectButton() {
    if (state_ == State::Scanning && selectedIndex_ != kNoSelection) {
        lv_obj_remove_state(objects.scan_connect_btn, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(objects.scan_connect_btn, LV_STATE_DISABLED);
    }
}

void ScanScreen::setStatusForDeviceCount(std::size_t count) {
    if (state_ != State::Scanning || count == statusDeviceCount_) {
        return;
    }

    statusDeviceCount_ = count;
    if (count == 0) {
        lv_label_set_text(objects.scan_status_label, "Scanning for OSSM...");
    } else {
        lv_label_set_text_fmt(
            objects.scan_status_label,
            "%u %s found",
            static_cast<unsigned int>(count),
            count == 1 ? "device" : "devices");
    }
}

void ScanScreen::handleDeviceClicked(lv_obj_t* row) {
    if (state_ != State::Scanning) {
        return;
    }

    for (std::size_t index = 0; index < deviceRows_.size(); ++index) {
        if (deviceRows_[index].button == row) {
            selectDevice(index);
            requestConnect();
            return;
        }
    }
}

void ScanScreen::handleDeviceRowEvent(lv_event_t* event) {
    auto* screen = static_cast<ScanScreen*>(lv_event_get_user_data(event));
    auto* row = static_cast<lv_obj_t*>(lv_event_get_target(event));
    screen->handleDeviceClicked(row);
}

}  // namespace m5_redux
