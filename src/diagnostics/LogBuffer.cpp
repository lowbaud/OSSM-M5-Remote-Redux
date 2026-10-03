#include "diagnostics/LogBuffer.h"

#include <cstring>
#include <mutex>

#include "diagnostics/SystemInfo.h"

#ifdef ARDUINO
#include <esp_attr.h>
#else
#define __NOINIT_ATTR
#endif

namespace m5_redux {
namespace logging {

namespace {

constexpr std::uint32_t kCapacity = 128;
constexpr std::uint32_t kMagic = 0x52444C47;  // "RDLG"

struct StoredRecord {
    std::uint32_t timeMs;
    std::uint16_t bootId;
    std::uint8_t level;
    std::uint8_t reserved;
    char tag[LogRecord::kTagCapacity];
    char message[LogRecord::kMessageCapacity];
};

static_assert(sizeof(StoredRecord) == 128, "Stored log records should stay compact");

// Changes whenever the stored layout does, so records from other firmware are discarded.
constexpr std::uint32_t kLayout =
    (static_cast<std::uint32_t>(sizeof(StoredRecord)) << 16) | kCapacity;

struct Storage {
    std::uint32_t magic;
    std::uint32_t layout;
    std::uint32_t nextSequence;
    std::uint32_t count;
    std::uint32_t bootId;
    std::uint32_t check;
    StoredRecord records[kCapacity];
};

// Not cleared at startup; validated before use because it holds garbage after power-on.
__NOINIT_ATTR Storage storage;

std::mutex bufferMutex;
bool initialized = false;
LogCounts counts;

std::uint32_t headerCheck() {
    return kMagic ^ kLayout ^ (storage.nextSequence * 2654435761u) ^ (storage.count << 8) ^
           (storage.bootId << 20);
}

bool headerValid() {
    return storage.magic == kMagic && storage.layout == kLayout && storage.count <= kCapacity &&
           storage.bootId <= 0xFFFF && storage.check == headerCheck();
}

void ensureInitialized() {
    if (initialized)
        return;
    initialized = true;

    if (system_info::resetPreservesMemory() && headerValid()) {
        storage.bootId = (storage.bootId + 1) & 0xFFFF;
    } else {
        storage.magic = kMagic;
        storage.layout = kLayout;
        storage.nextSequence = 0;
        storage.count = 0;
        storage.bootId = 0;
    }
    storage.check = headerCheck();
}

void copyText(char* destination, std::size_t capacity, const char* source) {
    std::strncpy(destination, source ? source : "", capacity - 1);
    destination[capacity - 1] = '\0';
}

}  // namespace

void appendRecord(Level level, std::uint32_t timeMs, const char* tag, const char* message) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    ensureInitialized();

    StoredRecord& record = storage.records[storage.nextSequence % kCapacity];
    record.timeMs = timeMs;
    record.bootId = static_cast<std::uint16_t>(storage.bootId);
    record.level = static_cast<std::uint8_t>(level);
    record.reserved = 0;
    copyText(record.tag, sizeof(record.tag), tag);
    copyText(record.message, sizeof(record.message), message);

    ++storage.nextSequence;
    if (storage.count < kCapacity)
        ++storage.count;
    storage.check = headerCheck();

    if (level == Level::Error) {
        ++counts.errors;
    } else if (level == Level::Warn) {
        ++counts.warnings;
    }
}

void recordRange(std::uint32_t& first, std::uint32_t& end) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    ensureInitialized();
    end = storage.nextSequence;
    first = end - storage.count;
}

bool readRecord(std::uint32_t sequence, LogRecord& out) {
    std::lock_guard<std::mutex> lock(bufferMutex);
    ensureInitialized();

    const std::uint32_t first = storage.nextSequence - storage.count;
    if (sequence - first >= storage.count)
        return false;

    const StoredRecord& record = storage.records[sequence % kCapacity];
    if (record.level < static_cast<std::uint8_t>(Level::Error) ||
        record.level > static_cast<std::uint8_t>(Level::Debug)) {
        return false;
    }

    out.timeMs = record.timeMs;
    out.level = static_cast<Level>(record.level);
    out.previousBoot = record.bootId != storage.bootId;
    // Records from before a crash may be torn, so never trust their terminators.
    std::memcpy(out.tag, record.tag, sizeof(out.tag));
    out.tag[sizeof(out.tag) - 1] = '\0';
    std::memcpy(out.message, record.message, sizeof(out.message));
    out.message[sizeof(out.message) - 1] = '\0';
    return true;
}

LogCounts currentBootCounts() {
    std::lock_guard<std::mutex> lock(bufferMutex);
    return counts;
}

}  // namespace logging
}  // namespace m5_redux
