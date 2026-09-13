#include "LvglPort.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <esp_timer.h>
#include <lvgl.h>

#include <cstddef>
#include <cstdint>

namespace m5_redux {

namespace {

constexpr int32_t kDisplayWidth = 320;
constexpr int32_t kDisplayHeight = 240;
constexpr int32_t kBufferRows = 15;

alignas(LV_DRAW_BUF_ALIGN) lv_color_t displayBuffer[kDisplayWidth * kBufferRows];
lv_indev_t* touchInput = nullptr;
bool touchActivity = false;
bool touchscreenEnabled = true;

uint32_t tickMilliseconds() {
    return static_cast<uint32_t>(esp_timer_get_time() / 1000LL);
}

void flushDisplay(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const uint32_t width = area->x2 - area->x1 + 1;
    const uint32_t height = area->y2 - area->y1 + 1;

    lv_draw_sw_rgb565_swap(pixels, width * height);
    M5.Display.pushImage(
        area->x1, area->y1, width, height, reinterpret_cast<const uint16_t*>(pixels));
    lv_display_flush_ready(display);
}

void readTouch(lv_indev_t*, lv_indev_data_t* data) {
    if (!touchscreenEnabled || M5.Touch.getCount() == 0) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    const auto touch = M5.Touch.getDetail(0);
    touchActivity = true;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touch.x;
    data->point.y = touch.y;
}

}  // namespace

namespace lvgl_port {

void begin() {
    touchActivity = false;
    lv_init();
    lv_tick_set_cb(tickMilliseconds);

    lv_display_t* display = lv_display_create(kDisplayWidth, kDisplayHeight);
    lv_display_set_flush_cb(display, flushDisplay);
    lv_display_set_buffers(
        display, displayBuffer, nullptr, sizeof(displayBuffer), LV_DISPLAY_RENDER_MODE_PARTIAL);

    touchInput = lv_indev_create();
    lv_indev_set_type(touchInput, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(touchInput, readTouch);
}

void update() {
    lv_timer_handler();
    delay(5);
}

bool takeTouchActivity() {
    const bool activity = touchActivity;
    touchActivity = false;
    return activity;
}

void setTouchscreenEnabled(bool enabled) {
    if (touchscreenEnabled == enabled) {
        return;
    }
    touchscreenEnabled = enabled;
    touchActivity = false;
    if (touchInput != nullptr) {
        // Cancel any pending gesture so disabling touch cannot turn it into a click.
        lv_indev_reset(touchInput, nullptr);
        lv_indev_wait_release(touchInput);
    }
}

}  // namespace lvgl_port

}  // namespace m5_redux
