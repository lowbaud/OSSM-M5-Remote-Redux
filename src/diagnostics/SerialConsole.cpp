#include "diagnostics/SerialConsole.h"

#include <Arduino.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "diagnostics/Log.h"
#include "diagnostics/LogBuffer.h"

namespace m5_redux {
namespace serial_console {

namespace {

constexpr std::size_t kLineCapacity = 160;
constexpr std::size_t kMaxLinesPerUpdate = 4;
// A UART without a TX buffer only reports its hardware FIFO as free, so wait for part of a
// line and let the write block briefly for the rest.
constexpr int kMinimumWriteSpace = 64;
constexpr std::uint32_t kStallTimeoutMs = 2000;

constexpr char kHint[] = "Send 'd' to print the diagnostic log or '?' for help.\n";
constexpr char kHelp[] = "Diagnostic commands:\n"
                         "  d  print the diagnostic log\n"
                         "  ?  show this help\n";

struct DumpState {
    bool active = false;
    std::uint32_t next = 0;
    std::uint32_t end = 0;
    bool previousHeaderWritten = false;
    bool currentHeaderWritten = false;
    bool endWritten = false;
    char line[kLineCapacity] = {};
    std::size_t length = 0;
    std::uint32_t lastProgressAtMs = 0;
};

DumpState dump;

void writeText(const char* text) {
    logging::writeOutput(text, std::strlen(text));
}

void setLine(const char* text) {
    const int written = std::snprintf(dump.line, sizeof(dump.line), "%s", text);
    dump.length =
        written > 0 ? std::min(static_cast<std::size_t>(written), sizeof(dump.line) - 1) : 0;
}

void startDump() {
    std::uint32_t first = 0;
    std::uint32_t end = 0;
    logging::recordRange(first, end);

    dump = {};
    dump.active = true;
    dump.next = first;
    dump.end = end;
    dump.lastProgressAtMs = millis();

    char header[64];
    std::snprintf(
        header,
        sizeof(header),
        "--- diagnostic log: %lu entries ---\n",
        static_cast<unsigned long>(end - first));
    setLine(header);
}

// Prepares the next line of the dump; returns false once the dump is complete.
bool prepareNextLine() {
    while (dump.end - dump.next > 0) {
        logging::LogRecord record;
        if (!logging::readRecord(dump.next, record)) {
            // Overwritten while the dump was in progress.
            ++dump.next;
            continue;
        }

        if (record.previousBoot && !dump.previousHeaderWritten) {
            dump.previousHeaderWritten = true;
            setLine("--- previous boot ---\n");
            return true;
        }
        if (!record.previousBoot && dump.previousHeaderWritten && !dump.currentHeaderWritten) {
            dump.currentHeaderWritten = true;
            setLine("--- current boot ---\n");
            return true;
        }

        ++dump.next;
        const int written = std::snprintf(
            dump.line,
            sizeof(dump.line),
            "%c (%lu) %s: %s\n",
            logging::levelLetter(record.level),
            static_cast<unsigned long>(record.timeMs),
            record.tag,
            record.message);
        dump.length =
            written > 0 ? std::min(static_cast<std::size_t>(written), sizeof(dump.line) - 1) : 0;
        return true;
    }

    if (!dump.endWritten) {
        dump.endWritten = true;
        setLine("--- end of diagnostic log ---\n");
        return true;
    }

    return false;
}

void advanceDump() {
    if (!dump.active)
        return;

    for (std::size_t lines = 0; lines < kMaxLinesPerUpdate; ++lines) {
        if (dump.length == 0 && !prepareNextLine()) {
            dump.active = false;
            return;
        }

        const int required = std::min(static_cast<int>(dump.length), kMinimumWriteSpace);
        if (Serial.availableForWrite() < required) {
            // Give up if nobody is reading, rather than holding the dump open forever.
            if (millis() - dump.lastProgressAtMs >= kStallTimeoutMs) {
                dump.active = false;
            }
            return;
        }

        logging::writeOutput(dump.line, dump.length);
        dump.length = 0;
        dump.lastProgressAtMs = millis();
    }
}

}  // namespace

void begin() {
    writeText(kHint);
}

void update() {
    while (Serial.available() > 0) {
        const int command = Serial.read();
        if (command == 'd' || command == 'D') {
            startDump();
        } else if (command == '?') {
            writeText(kHelp);
        }
    }

    advanceDump();
}

}  // namespace serial_console
}  // namespace m5_redux
