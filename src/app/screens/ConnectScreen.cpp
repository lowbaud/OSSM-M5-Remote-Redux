#include "ConnectScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

void ConnectScreen::begin() {
    lv_label_set_long_mode(objects.connect_device_name_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_long_mode(objects.connect_device_address_lbl, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_long_mode(objects.connect_status_lbl, LV_LABEL_LONG_MODE_DOTS);
}

void ConnectScreen::configure(const char* deviceName, const char* deviceAddress) {
    lv_label_set_text(
        objects.connect_device_name_lbl,
        deviceName && deviceName[0] != '\0' ? deviceName : "Unnamed OSSM");
    lv_label_set_text(objects.connect_device_address_lbl, deviceAddress);
    connectionStarted();
}

void ConnectScreen::enter() {
    pendingAction_ = ConnectScreenAction::None;
    if (lv_screen_active() != objects.connect) {
        loadScreen(SCREEN_ID_CONNECT);
    }
}

void ConnectScreen::leave() {
    pendingAction_ = ConnectScreenAction::None;
    retryPending_ = false;
}

ConnectScreenAction ConnectScreen::update(const RemoteInputEvents& events) {
    if (events.leftClick) {
        requestCancel();
    }

    if (pendingAction_ == ConnectScreenAction::None && retryPending_ &&
        static_cast<std::int32_t>(millis() - retryAtMs_) >= 0) {
        retryPending_ = false;
        pendingAction_ = ConnectScreenAction::Retry;
    }

    const ConnectScreenAction action = pendingAction_;
    pendingAction_ = ConnectScreenAction::None;
    return action;
}

void ConnectScreen::requestCancel() {
    retryPending_ = false;
    pendingAction_ = ConnectScreenAction::Cancel;
}

void ConnectScreen::connectionStarted() {
    retryPending_ = false;
    lv_label_set_text(objects.connect_status_lbl, "Connecting...");
}

void ConnectScreen::connectionEstablished() {
    retryPending_ = false;
    lv_label_set_text(objects.connect_status_lbl, "Preparing OSSM...");
}

void ConnectScreen::connectionFailed() {
    retryPending_ = true;
    retryAtMs_ = millis() + kRetryDelayMs;
    lv_label_set_text(objects.connect_status_lbl, "Connection failed. Retrying...");
}

}  // namespace m5_redux
