#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace test_support {

// Persistent across Preferences instances to model reopening the same NVS namespace.
struct FakePreferencesState {
    std::map<std::string, std::vector<std::uint8_t>> values;
    bool failBegin = false;
    bool failWrites = false;
    unsigned beginCalls = 0;
    unsigned writeCalls = 0;
};

inline FakePreferencesState preferences;

}  // namespace test_support

// Only the Preferences API used by SettingsStore; this header is native-only.
class Preferences {
  public:
    bool begin(const char* name, bool) {
        ++test_support::preferences.beginCalls;
        opened_ = !test_support::preferences.failBegin;
        namespace_ = name;
        return opened_;
    }

    std::uint8_t getUChar(const char* key, std::uint8_t fallback) {
        return getScalar(key, fallback);
    }
    std::uint32_t getUInt(const char* key, std::uint32_t fallback) {
        return getScalar(key, fallback);
    }
    bool getBool(const char* key, bool fallback) {
        return getScalar(key, fallback);
    }
    std::size_t putUChar(const char* key, std::uint8_t value) {
        return putBytes(key, &value, sizeof(value));
    }
    std::size_t putUInt(const char* key, std::uint32_t value) {
        return putBytes(key, &value, sizeof(value));
    }
    std::size_t putBool(const char* key, bool value) {
        return putBytes(key, &value, sizeof(value));
    }
    std::string getString(const char* key, const char* fallback) {
        const auto& values = test_support::preferences.values;
        const auto found = values.find(fullKey(key));
        if (!opened_ || found == values.end() || found->second.empty()) {
            return fallback;
        }
        return std::string(found->second.begin(), found->second.end() - 1);
    }
    std::size_t putString(const char* key, const char* value) {
        const auto length = std::strlen(value);
        return putBytes(key, value, length + 1) == length + 1 ? length : 0;
    }
    std::size_t getBytesLength(const char* key) {
        const auto& values = test_support::preferences.values;
        const auto found = values.find(fullKey(key));
        return opened_ && found != values.end() ? found->second.size() : 0;
    }
    std::size_t getBytes(const char* key, void* output, std::size_t capacity) {
        const auto length = getBytesLength(key);
        if (length == 0 || length > capacity) {
            return 0;
        }
        std::memcpy(output, test_support::preferences.values.at(fullKey(key)).data(), length);
        return length;
    }
    std::size_t putBytes(const char* key, const void* data, std::size_t length) {
        auto& state = test_support::preferences;
        ++state.writeCalls;
        if (!opened_ || state.failWrites) {
            return 0;
        }
        const auto* bytes = static_cast<const std::uint8_t*>(data);
        state.values[fullKey(key)] = std::vector<std::uint8_t>(bytes, bytes + length);
        return length;
    }
    bool remove(const char* key) {
        auto& state = test_support::preferences;
        ++state.writeCalls;
        return opened_ && !state.failWrites && state.values.erase(fullKey(key)) != 0;
    }

  private:
    bool opened_ = false;
    std::string namespace_;

    std::string fullKey(const char* key) const {
        return namespace_ + "/" + key;
    }
    template <typename T> T getScalar(const char* key, T fallback) {
        T value{};
        return getBytes(key, &value, sizeof(value)) == sizeof(value) ? value : fallback;
    }
};
