#include "MigrationDiagnostics.h"

#if defined(MIGRATION_DIAGNOSTICS) && MIGRATION_DIAGNOSTICS

#include <Arduino.h>

#include <esp_arduino_version.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace m5_redux {
namespace migration_diagnostics {

namespace {

constexpr std::uint32_t kRuntimeSnapshotDelayMs = 5U * 60U * 1000U;

unsigned int currentTaskStackMinimumFree() {
#if INCLUDE_uxTaskGetStackHighWaterMark
    return static_cast<unsigned int>(uxTaskGetStackHighWaterMark(nullptr));
#else
    return 0;
#endif
}

}  // namespace

void printSnapshot(const char* checkpoint) {
    const char* safeCheckpoint = checkpoint ? checkpoint : "unspecified";

    Serial.printf(
        "[migration] checkpoint=%s arduino=%d.%d.%d idf=%s reset_reason=%d "
        "task_stack_min_free_bytes=%u\n",
        safeCheckpoint,
        ESP_ARDUINO_VERSION_MAJOR,
        ESP_ARDUINO_VERSION_MINOR,
        ESP_ARDUINO_VERSION_PATCH,
        esp_get_idf_version(),
        static_cast<int>(esp_reset_reason()),
        currentTaskStackMinimumFree());
    Serial.printf(
        "[migration] checkpoint=%s internal_free=%u internal_min=%u internal_largest=%u "
        "dma_free=%u "
        "psram_free=%u psram_min=%u psram_largest=%u\n",
        safeCheckpoint,
        static_cast<unsigned int>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned int>(
            heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned int>(
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
        static_cast<unsigned int>(heap_caps_get_free_size(MALLOC_CAP_DMA)),
        static_cast<unsigned int>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned int>(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)),
        static_cast<unsigned int>(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
}

void printRuntimeSnapshotOnce(std::uint32_t nowMs) {
    static bool printed = false;
    if (printed || nowMs < kRuntimeSnapshotDelayMs) {
        return;
    }

    printed = true;
    printSnapshot("five-minute-runtime");
}

}  // namespace migration_diagnostics
}  // namespace m5_redux

#endif
