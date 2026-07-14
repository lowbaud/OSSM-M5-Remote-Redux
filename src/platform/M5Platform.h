#pragma once

namespace m5_redux {
namespace m5_platform {

void begin();
void update();
int batteryLevelPercent();
bool externalPowerPresent();
void setDisplayBrightnessPercent(int percent);
void powerOff();

}  // namespace m5_platform
}  // namespace m5_redux
