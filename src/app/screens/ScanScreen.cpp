#include "ScanScreen.h"

#include <Arduino.h>

#include <cstdio>
#include <cstring>

#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

constexpr std::int32_t kSpinnerSize = 14;
constexpr std::int32_t kSpinnerLabelGap = 6;
constexpr std::int32_t kSpinnerArcWidth = 2;
constexpr std::uint32_t kSpinnerDurationMs = 1000;
constexpr std::uint32_t kSpinnerArcSweepDegrees = 200;

const char* deviceName(const ossm::DiscoveredOssm& device) {
    return device.name[0] == '\0' ? "Unnamed OSSM" : device.name;
}

void formatDeviceText(
    const ossm::DiscoveredOssm& device, int rssi, char* text, std::size_t textSize) {
    std::snprintf(text, textSize, "%s  %d dBm", deviceName(device), rssi);
}

void styleDeviceRow(lv_obj_t* row, lv_obj_t* list) {
    const lv_style_selector_t normal = LV_PART_MAIN | LV_STATE_DEFAULT;
    const lv_style_selector_t selected = LV_PART_MAIN | LV_STATE_CHECKED;

    lv_obj_set_style_bg_color(row, lv_obj_get_style_bg_color(list, LV_PART_MAIN), normal);
    lv_obj_set_style_bg_opa(row, lv_obj_get_style_bg_opa(list, LV_PART_MAIN), normal);
    lv_obj_set_style_bg_color(row, lv_theme_get_color_primary(row), selected);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, selected);
    lv_obj_set_style_text_color(row, lv_color_white(), selected);
}

}  // namespace

ScanScreen::ScanScreen(ossm::OssmDiscovery& discovery) : discovery_(discovery) {}

void ScanScreen::begin() {
    activitySpinner_ = lv_spinner_create(objects.scan);
    lv_obj_set_size(activitySpinner_, kSpinnerSize, kSpinnerSize);
    lv_spinner_set_anim_params(activitySpinner_, kSpinnerDurationMs, kSpinnerArcSweepDegrees);

    const lv_color_t primaryColor = lv_theme_get_color_primary(activitySpinner_);
    lv_obj_set_style_arc_color(activitySpinner_, primaryColor, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(activitySpinner_, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_arc_width(activitySpinner_, kSpinnerArcWidth, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(activitySpinner_, true, LV_PART_MAIN);
    lv_obj_set_style_arc_color(activitySpinner_, primaryColor, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(activitySpinner_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(activitySpinner_, kSpinnerArcWidth, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(activitySpinner_, true, LV_PART_INDICATOR);

    lv_obj_set_pos(objects.scan_status_label, 30, 183);
    lv_obj_set_width(objects.scan_status_label, 280);
    lv_label_set_long_mode(objects.scan_status_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_update_layout(objects.scan);
    lv_obj_align_to(
        activitySpinner_, objects.scan_status_label, LV_ALIGN_OUT_LEFT_MID, -kSpinnerLabelGap, 0);

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
    if (state_ == State::Scanning) {
        discovery_.update();
        updateScanResults();

        if (!deviceRows_.empty() && events.encoderSteps[3] != 0) {
            const std::int64_t current = selectedIndex_ == kNoSelection ? 0 : selectedIndex_;
            const std::int64_t last = static_cast<std::int64_t>(deviceRows_.size() - 1);
            std::int64_t next = current + events.encoderSteps[3];
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

    if (!discovery_.startScan()) {
        setState(State::ScanFailed);
        lv_label_set_text(objects.scan_status_label, "Unable to start scan");
        return;
    }

    setState(State::Scanning);
    lv_label_set_text(objects.scan_status_label, "Scanning for OSSM...");
}

void ScanScreen::setState(State state) {
    state_ = state;
    if (!activitySpinner_) {
        return;
    }

    const bool busy = state == State::Scanning;
    if (busy) {
        lv_obj_remove_flag(activitySpinner_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(activitySpinner_, LV_OBJ_FLAG_HIDDEN);
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
            char text[64];
            formatDeviceText(device, device.rssi, text, sizeof(text));

            lv_obj_t* row = lv_list_add_button(objects.scan_device_list, nullptr, text);
            styleDeviceRow(row, objects.scan_device_list);
            lv_obj_add_event_cb(row, handleDeviceRowEvent, LV_EVENT_CLICKED, this);
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
            displayed.filteredRssi += (device.rssi - displayed.filteredRssi) / kRssiFilterDivisor;
        }
        displayed.latest = device;

        const bool rssiRefreshDue = displayed.filteredRssi != displayed.renderedRssi &&
                                    now - displayed.renderedAtMs >= kRssiRefreshIntervalMs;
        if (nameChanged || rssiRefreshDue) {
            char text[64];
            formatDeviceText(device, displayed.filteredRssi, text, sizeof(text));
            lv_list_set_button_text(objects.scan_device_list, deviceRows_[index], text);
            displayed.renderedRssi = displayed.filteredRssi;
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
        lv_obj_remove_state(deviceRows_[selectedIndex_], LV_STATE_CHECKED);
    }

    selectedIndex_ = index;
    lv_obj_add_state(deviceRows_[selectedIndex_], LV_STATE_CHECKED);
    lv_obj_scroll_to_view(deviceRows_[selectedIndex_], LV_ANIM_ON);
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
    if (state_ != State::Scanning) {
        return;
    }

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
        if (deviceRows_[index] == row) {
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
