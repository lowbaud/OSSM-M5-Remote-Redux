#pragma once

#include <Preferences.h>

#include <cstddef>
#include <cstdint>

namespace m5_redux {

enum class BrightnessLevel : std::uint8_t {
    Low = 0,
    Medium = 1,
    High = 2,
    Maximum = 3,
};

enum class IdleDimTimeout : std::uint32_t {
    Never = 0,
    Seconds30 = 30,
    Minute1 = 60,
    Minutes5 = 300,
};

enum class IdlePowerOffTimeout : std::uint32_t {
    Never = 0,
    Minutes15 = 900,
    Minutes30 = 1800,
    Minutes60 = 3600,
};

struct BrightnessOption {
    BrightnessLevel level;
    const char* name;
    int percent;
};

struct IdleDimOption {
    IdleDimTimeout timeout;
    const char* name;
};

struct IdlePowerOffOption {
    IdlePowerOffTimeout timeout;
    const char* name;
};

struct AutoConnectOption {
    bool enabled;
    const char* name;
};

struct StrokeDirectionOption {
    bool reversed;
    const char* name;
};

struct SavedOssmConnection {
    std::uint64_t address = 0;
    std::uint8_t addressType = 0;
    char name[32] = {};
};

class SettingsStore {
  public:
    static constexpr BrightnessLevel kDefaultBrightnessLevel = BrightnessLevel::Medium;
    static constexpr IdleDimTimeout kDefaultIdleDimTimeout = IdleDimTimeout::Minute1;
    static constexpr IdlePowerOffTimeout kDefaultIdlePowerOffTimeout =
        IdlePowerOffTimeout::Minutes30;
    static constexpr bool kDefaultAutoConnectEnabled = true;
    static constexpr bool kDefaultStrokeEncoderReversed = false;

    static constexpr std::size_t kBrightnessOptionCount = 4;
    static constexpr std::size_t kIdleDimOptionCount = 4;
    static constexpr std::size_t kIdlePowerOffOptionCount = 4;
    static constexpr std::size_t kAutoConnectOptionCount = 2;
    static constexpr std::size_t kStrokeDirectionOptionCount = 2;

    bool begin();
    BrightnessLevel brightnessLevel() const;
    bool setBrightnessLevel(BrightnessLevel level);
    IdleDimTimeout idleDimTimeout() const;
    bool setIdleDimTimeout(IdleDimTimeout timeout);
    IdlePowerOffTimeout idlePowerOffTimeout() const;
    bool setIdlePowerOffTimeout(IdlePowerOffTimeout timeout);
    bool autoConnectEnabled() const;
    bool setAutoConnectEnabled(bool enabled);
    bool strokeEncoderReversed() const;
    bool setStrokeEncoderReversed(bool reversed);
    bool savedOssmConnection(SavedOssmConnection& connection) const;
    bool setSavedOssmConnection(const SavedOssmConnection& connection);

    static const BrightnessOption& brightnessOption(std::size_t index);
    static std::size_t brightnessOptionIndex(BrightnessLevel level);
    static const IdleDimOption& idleDimOption(std::size_t index);
    static std::size_t idleDimOptionIndex(IdleDimTimeout timeout);
    static const IdlePowerOffOption& idlePowerOffOption(std::size_t index);
    static std::size_t idlePowerOffOptionIndex(IdlePowerOffTimeout timeout);
    static const AutoConnectOption& autoConnectOption(std::size_t index);
    static std::size_t autoConnectOptionIndex(bool enabled);
    static const StrokeDirectionOption& strokeDirectionOption(std::size_t index);
    static std::size_t strokeDirectionOptionIndex(bool reversed);

  private:
    Preferences preferences_;
    BrightnessLevel brightnessLevel_ = kDefaultBrightnessLevel;
    IdleDimTimeout idleDimTimeout_ = kDefaultIdleDimTimeout;
    IdlePowerOffTimeout idlePowerOffTimeout_ = kDefaultIdlePowerOffTimeout;
    bool autoConnectEnabled_ = kDefaultAutoConnectEnabled;
    bool strokeEncoderReversed_ = kDefaultStrokeEncoderReversed;
    SavedOssmConnection savedOssmConnection_{};
    bool hasSavedOssmConnection_ = false;
    bool initialized_ = false;

    static bool isValidBrightnessLevel(std::uint8_t value);
    static bool isValidIdleDimTimeout(std::uint32_t seconds);
    static bool isValidIdlePowerOffTimeout(std::uint32_t seconds);
    static bool isValidSavedOssmConnection(const SavedOssmConnection& connection);
};

}  // namespace m5_redux
