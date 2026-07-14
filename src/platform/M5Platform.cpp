#include "M5Platform.h"

#include <M5Unified.h>

#ifndef BATTERY_CHARGE_CURRENT
#error "BATTERY_CHARGE_CURRENT must be provided by the PlatformIO build configuration"
#endif

namespace m5_redux {
namespace m5_platform {

void begin() {
    auto config = M5.config();
    config.serial_baudrate = 115200;
    // The remote PCB does not use the external 5 V rail.
    config.output_power = false;
    M5.begin(config);

    Serial.println("Serial diagnostics ready");
    M5.Power.setChargeCurrent(BATTERY_CHARGE_CURRENT);
}

void update() {
    M5.update();
}

int batteryLevelPercent() {
    return M5.Power.getBatteryLevel();
}

bool externalPowerPresent() {
    switch (M5.Power.getType()) {
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
        case m5::Power_Class::pmic_axp192:
            return M5.Power.Axp192.isACIN();
#endif
        case m5::Power_Class::pmic_axp2101:
            return M5.Power.Axp2101.isVBUS();
        default:
            return false;
    }
}

void setDisplayBrightnessPercent(int percent) {
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }

    M5.Display.setBrightness((percent * 255 + 50) / 100);
}

void powerOff() {
    M5.Power.powerOff();
}

}  // namespace m5_platform
}  // namespace m5_redux
