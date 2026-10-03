#include "diagnostics/SystemInfo.h"

#ifdef ARDUINO
#include <esp_system.h>
#endif

namespace m5_redux {
namespace system_info {

#ifdef ARDUINO

const char* resetReasonName() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:
            return "Power on";
        case ESP_RST_EXT:
            return "External";
        case ESP_RST_SW:
            return "Software";
        case ESP_RST_PANIC:
            return "Crash";
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return "Watchdog";
        case ESP_RST_DEEPSLEEP:
            return "Wake from sleep";
        case ESP_RST_BROWNOUT:
            return "Brownout";
        case ESP_RST_USB:
            return "USB";
        default:
            return "Other";
    }
}

bool resetWasAbnormal() {
    switch (esp_reset_reason()) {
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_BROWNOUT:
            return true;
        default:
            return false;
    }
}

bool resetPreservesMemory() {
    switch (esp_reset_reason()) {
        case ESP_RST_SW:
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
            return true;
        default:
            return false;
    }
}

#else

const char* resetReasonName() {
    return "Power on";
}

bool resetWasAbnormal() {
    return false;
}

bool resetPreservesMemory() {
    return false;
}

#endif

}  // namespace system_info
}  // namespace m5_redux
