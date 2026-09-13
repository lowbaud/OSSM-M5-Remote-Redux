#pragma once

namespace m5_redux {
namespace lvgl_port {

void begin();
void update();
void setTouchscreenEnabled(bool enabled);
bool takeTouchActivity();

}  // namespace lvgl_port
}  // namespace m5_redux
