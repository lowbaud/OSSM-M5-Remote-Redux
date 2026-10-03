#pragma once

#include <cstddef>
#include <cstdint>

#include "diagnostics/Log.h"

namespace m5_redux {
namespace logging {

struct LogRecord {
    static constexpr std::size_t kTagCapacity = 10;
    static constexpr std::size_t kMessageCapacity = 110;

    std::uint32_t timeMs = 0;
    Level level = Level::Info;
    // Written before the most recent reset.
    bool previousBoot = false;
    char tag[kTagCapacity] = {};
    char message[kMessageCapacity] = {};
};

struct LogCounts {
    std::uint32_t errors = 0;
    std::uint32_t warnings = 0;
};

// Recent Info-and-above records, kept in RAM that survives software resets, crashes and
// watchdog resets. Records are addressed by sequence numbers that increase with every append.
void appendRecord(Level level, std::uint32_t timeMs, const char* tag, const char* message);
void recordRange(std::uint32_t& first, std::uint32_t& end);
// Returns false once the record has been overwritten.
bool readRecord(std::uint32_t sequence, LogRecord& out);
// Errors and warnings recorded since this boot.
LogCounts currentBootCounts();

}  // namespace logging
}  // namespace m5_redux
