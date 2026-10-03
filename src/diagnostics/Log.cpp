#include "diagnostics/Log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "diagnostics/LogBuffer.h"

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <chrono>
#endif

namespace m5_redux {
namespace logging {

namespace {
constexpr size_t kLineCapacity = 256;
constexpr char kTruncationMarker[] = "...";

// Constant-initialized, so it is safe to use from any task before setup() runs.
std::mutex outputMutex;

unsigned long uptimeMs() {
#ifdef ARDUINO
    return millis();
#else
    static const auto start = std::chrono::steady_clock::now();
    return static_cast<unsigned long>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now() - start)
                                          .count());
#endif
}

void writeLine(const char* line, size_t length) {
#ifdef ARDUINO
    Serial.write(reinterpret_cast<const uint8_t*>(line), length);
#else
    std::fwrite(line, 1, length, stdout);
#endif
}
}  // namespace

char levelLetter(Level level) {
    switch (level) {
        case Level::Error:
            return 'E';
        case Level::Warn:
            return 'W';
        case Level::Info:
            return 'I';
        case Level::Debug:
            return 'D';
    }

    return '?';
}

void write(Level level, const char* tag, const char* format, ...) {
#ifdef ARDUINO
    if (xPortInIsrContext())
        return;
#endif

    char line[kLineCapacity];
    // Reserve room for the trailing newline and NUL.
    constexpr size_t kTextCapacity = kLineCapacity - 2;

    const unsigned long timeMs = uptimeMs();
    int prefixLength = std::snprintf(
        line, kTextCapacity, "%c (%lu) %s: ", levelLetter(level), timeMs, tag ? tag : "-");
    if (prefixLength < 0)
        return;
    size_t length = static_cast<size_t>(prefixLength);
    if (length >= kTextCapacity)
        length = kTextCapacity - 1;
    const size_t messageStart = length;

    va_list args;
    va_start(args, format);
    const int messageLength = std::vsnprintf(line + length, kTextCapacity - length, format, args);
    va_end(args);
    if (messageLength < 0)
        return;

    if (length + static_cast<size_t>(messageLength) >= kTextCapacity) {
        length = kTextCapacity - 1;
        constexpr size_t kMarkerLength = sizeof(kTruncationMarker) - 1;
        std::memcpy(line + length - kMarkerLength, kTruncationMarker, kMarkerLength);
    } else {
        length += static_cast<size_t>(messageLength);
    }

    line[length] = '\0';
    if (level <= Level::Info) {
        appendRecord(level, static_cast<std::uint32_t>(timeMs), tag, line + messageStart);
    }

    line[length++] = '\n';
    line[length] = '\0';

    std::lock_guard<std::mutex> lock(outputMutex);
    writeLine(line, length);
}

void writeOutput(const char* text, size_t length) {
#ifdef ARDUINO
    if (xPortInIsrContext())
        return;
#endif

    std::lock_guard<std::mutex> lock(outputMutex);
    writeLine(text, length);
}

}  // namespace logging
}  // namespace m5_redux
