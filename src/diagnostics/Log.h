#pragma once

#include <cstdint>

// Application logging, independent of CORE_DEBUG_LEVEL so framework verbosity can be tuned
// separately. Lines use the ESP-IDF layout: "I (12345) tag: message".
#define REDUX_LOG_LEVEL_NONE 0
#define REDUX_LOG_LEVEL_ERROR 1
#define REDUX_LOG_LEVEL_WARN 2
#define REDUX_LOG_LEVEL_INFO 3
#define REDUX_LOG_LEVEL_DEBUG 4

#ifndef REDUX_LOG_LEVEL
#define REDUX_LOG_LEVEL REDUX_LOG_LEVEL_INFO
#endif

namespace m5_redux {
namespace logging {

enum class Level : std::uint8_t {
    Error = REDUX_LOG_LEVEL_ERROR,
    Warn = REDUX_LOG_LEVEL_WARN,
    Info = REDUX_LOG_LEVEL_INFO,
    Debug = REDUX_LOG_LEVEL_DEBUG,
};

// Formats one complete line and writes it atomically. Calls from an ISR are dropped.
void write(Level level, const char* tag, const char* format, ...)
    __attribute__((format(printf, 3, 4)));

}  // namespace logging
}  // namespace m5_redux

// Disabled levels are removed by the compiler but their arguments are still type-checked.
#define REDUX_LOG_AT(level, tag, ...)                                                              \
    do {                                                                                           \
        if (REDUX_LOG_LEVEL >= REDUX_LOG_LEVEL_##level) {                                          \
            ::m5_redux::logging::write(                                                            \
                static_cast<::m5_redux::logging::Level>(REDUX_LOG_LEVEL_##level),                  \
                tag,                                                                               \
                __VA_ARGS__);                                                                      \
        }                                                                                          \
    } while (0)

#define LOGE(tag, ...) REDUX_LOG_AT(ERROR, tag, __VA_ARGS__)
#define LOGW(tag, ...) REDUX_LOG_AT(WARN, tag, __VA_ARGS__)
#define LOGI(tag, ...) REDUX_LOG_AT(INFO, tag, __VA_ARGS__)
#define LOGD(tag, ...) REDUX_LOG_AT(DEBUG, tag, __VA_ARGS__)
