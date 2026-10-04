#include "ConnectScreen.h"

#include <Arduino.h>
#include <lvgl.h>

#include "ui/ThemeColors.h"
#include "ui/generated/screens.h"
#include "ui/generated/ui.h"

namespace m5_redux {

namespace {

void setStatus(const char* text, Colors color) {
    lv_label_set_text(objects.connect_status_lbl, text);
    lv_obj_set_style_text_color(objects.connect_status_lbl, themeColor(color), LV_PART_MAIN);
}

}  // namespace

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
    setStatus("Connecting...", COLOR_ID_TEXT_PRIMARY);
}

void ConnectScreen::connectionEstablished() {
    retryPending_ = false;
    setStatus("Preparing OSSM...", COLOR_ID_TEXT_PRIMARY);
}

void ConnectScreen::connectionFailed() {
    retryPending_ = true;
    retryAtMs_ = millis() + kRetryDelayMs;
    // Stands out from normal progress until the next attempt resets it.
    setStatus("Connection failed. Retrying...", COLOR_ID_DANGER);
}

}  // namespace m5_redux
