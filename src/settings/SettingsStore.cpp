#include "SettingsStore.h"

#include <cstring>
#include <type_traits>

namespace m5_redux {

namespace {

constexpr char kPreferencesNamespace[] = "m5-redux";
constexpr char kBrightnessKey[] = "brightness";
constexpr char kIdleDimKey[] = "idle_dim";
constexpr char kIdlePowerOffKey[] = "idle_power_off";
constexpr char kAutoConnectKey[] = "auto_connect";
constexpr char kStrokeDirectionKey[] = "stroke_reverse";
constexpr char kOssmConnectionKey[] = "ossm_conn";
constexpr std::uint8_t kOssmConnectionVersion = 1;

struct StoredOssmConnection {
    std::uint64_t address = 0;
    char name[32] = {};
    std::uint8_t version = kOssmConnectionVersion;
    std::uint8_t addressType = 0;
    std::uint8_t reserved[6] = {};
};

static_assert(
    std::is_trivially_copyable<StoredOssmConnection>::value,
    "Stored OSSM connection must be safe to persist as a blob");
static_assert(sizeof(StoredOssmConnection) == 48, "Stored OSSM connection layout changed");

const BrightnessOption kBrightnessOptions[] = {
    {BrightnessLevel::Low, "Low", 25},
    {BrightnessLevel::Medium, "Medium", 50},
    {BrightnessLevel::High, "High", 75},
    {BrightnessLevel::Maximum, "Maximum", 100},
};

constexpr IdleDimOption kIdleDimOptions[] = {
    {IdleDimTimeout::Never, "Never"},
    {IdleDimTimeout::Seconds30, "30 sec"},
    {IdleDimTimeout::Minute1, "1 min"},
    {IdleDimTimeout::Minutes5, "5 min"},
};
constexpr std::size_t kDefaultIdleDimOptionIndex = 2;

constexpr IdlePowerOffOption kIdlePowerOffOptions[] = {
    {IdlePowerOffTimeout::Never, "Never"},
    {IdlePowerOffTimeout::Minutes15, "15 min"},
    {IdlePowerOffTimeout::Minutes30, "30 min"},
    {IdlePowerOffTimeout::Minutes60, "60 min"},
};
constexpr std::size_t kDefaultIdlePowerOffOptionIndex = 2;

constexpr AutoConnectOption kAutoConnectOptions[] = {
    {true, "Yes"},
    {false, "No"},
};

constexpr StrokeDirectionOption kStrokeDirectionOptions[] = {
    {false, "Right decreases"},
    {true, "Right increases"},
};

static_assert(
    sizeof(kPreferencesNamespace) - 1 <= 15, "NVS namespace names are limited to 15 characters");
static_assert(sizeof(kBrightnessKey) - 1 <= 15, "NVS key names are limited to 15 characters");
static_assert(sizeof(kIdleDimKey) - 1 <= 15, "NVS key names are limited to 15 characters");
static_assert(sizeof(kIdlePowerOffKey) - 1 <= 15, "NVS key names are limited to 15 characters");
static_assert(sizeof(kAutoConnectKey) - 1 <= 15, "NVS key names are limited to 15 characters");
static_assert(
    sizeof(kStrokeDirectionKey) - 1 <= 15, "NVS key names are limited to 15 characters");
static_assert(sizeof(kOssmConnectionKey) - 1 <= 15, "NVS key names are limited to 15 characters");
static_assert(
    sizeof(kBrightnessOptions) / sizeof(kBrightnessOptions[0]) ==
        SettingsStore::kBrightnessOptionCount,
    "Brightness option count does not match its catalog");
static_assert(
    sizeof(kIdleDimOptions) / sizeof(kIdleDimOptions[0]) == SettingsStore::kIdleDimOptionCount,
    "Idle dim option count does not match its catalog");
static_assert(
    sizeof(kIdlePowerOffOptions) / sizeof(kIdlePowerOffOptions[0]) ==
        SettingsStore::kIdlePowerOffOptionCount,
    "Idle power-off option count does not match its catalog");
static_assert(
    sizeof(kAutoConnectOptions) / sizeof(kAutoConnectOptions[0]) ==
        SettingsStore::kAutoConnectOptionCount,
    "Auto-connect option count does not match its catalog");
static_assert(
    sizeof(kStrokeDirectionOptions) / sizeof(kStrokeDirectionOptions[0]) ==
        SettingsStore::kStrokeDirectionOptionCount,
    "Stroke direction option count does not match its catalog");
static_assert(
    kIdleDimOptions[kDefaultIdleDimOptionIndex].timeout == SettingsStore::kDefaultIdleDimTimeout,
    "Default idle dim option must remain one minute");
static_assert(
    kIdlePowerOffOptions[kDefaultIdlePowerOffOptionIndex].timeout ==
        SettingsStore::kDefaultIdlePowerOffTimeout,
    "Default idle power-off option must remain 30 minutes");

}  // namespace

bool SettingsStore::begin() {
    if (initialized_) {
        return true;
    }

    if (!preferences_.begin(kPreferencesNamespace, false)) {
        return false;
    }

    initialized_ = true;
    const std::uint8_t stored =
        preferences_.getUChar(kBrightnessKey, static_cast<std::uint8_t>(kDefaultBrightnessLevel));
    if (isValidBrightnessLevel(stored)) {
        brightnessLevel_ = static_cast<BrightnessLevel>(stored);
    }

    const std::uint32_t storedIdleDim =
        preferences_.getUInt(kIdleDimKey, static_cast<std::uint32_t>(kDefaultIdleDimTimeout));
    if (isValidIdleDimTimeout(storedIdleDim)) {
        idleDimTimeout_ = static_cast<IdleDimTimeout>(storedIdleDim);
    }

    const std::uint32_t storedIdlePowerOff = preferences_.getUInt(
        kIdlePowerOffKey, static_cast<std::uint32_t>(kDefaultIdlePowerOffTimeout));
    if (isValidIdlePowerOffTimeout(storedIdlePowerOff)) {
        idlePowerOffTimeout_ = static_cast<IdlePowerOffTimeout>(storedIdlePowerOff);
    }

    autoConnectEnabled_ = preferences_.getBool(kAutoConnectKey, kDefaultAutoConnectEnabled);
    strokeEncoderReversed_ =
        preferences_.getBool(kStrokeDirectionKey, kDefaultStrokeEncoderReversed);

    StoredOssmConnection storedConnection;
    if (preferences_.getBytesLength(kOssmConnectionKey) == sizeof(storedConnection) &&
        preferences_.getBytes(kOssmConnectionKey, &storedConnection, sizeof(storedConnection)) ==
            sizeof(storedConnection) &&
        storedConnection.version == kOssmConnectionVersion) {
        SavedOssmConnection connection;
        connection.address = storedConnection.address;
        connection.addressType = storedConnection.addressType;
        std::memcpy(connection.name, storedConnection.name, sizeof(connection.name));
        connection.name[sizeof(connection.name) - 1] = '\0';

        if (isValidSavedOssmConnection(connection) &&
            std::memchr(storedConnection.name, '\0', sizeof(storedConnection.name))) {
            savedOssmConnection_ = connection;
            hasSavedOssmConnection_ = true;
        }
    }

    return true;
}

BrightnessLevel SettingsStore::brightnessLevel() const {
    return brightnessLevel_;
}

bool SettingsStore::setBrightnessLevel(BrightnessLevel level) {
    const std::uint8_t value = static_cast<std::uint8_t>(level);
    if (!initialized_ || !isValidBrightnessLevel(value)) {
        return false;
    }

    if (level == brightnessLevel_) {
        return true;
    }

    if (preferences_.putUChar(kBrightnessKey, value) != sizeof(value)) {
        return false;
    }

    brightnessLevel_ = level;
    return true;
}

IdleDimTimeout SettingsStore::idleDimTimeout() const {
    return idleDimTimeout_;
}

bool SettingsStore::setIdleDimTimeout(IdleDimTimeout timeout) {
    const std::uint32_t seconds = static_cast<std::uint32_t>(timeout);
    if (!initialized_ || !isValidIdleDimTimeout(seconds)) {
        return false;
    }

    if (timeout == idleDimTimeout_) {
        return true;
    }

    if (preferences_.putUInt(kIdleDimKey, seconds) != sizeof(seconds)) {
        return false;
    }

    idleDimTimeout_ = timeout;
    return true;
}

IdlePowerOffTimeout SettingsStore::idlePowerOffTimeout() const {
    return idlePowerOffTimeout_;
}

bool SettingsStore::setIdlePowerOffTimeout(IdlePowerOffTimeout timeout) {
    const std::uint32_t seconds = static_cast<std::uint32_t>(timeout);
    if (!initialized_ || !isValidIdlePowerOffTimeout(seconds)) {
        return false;
    }

    if (timeout == idlePowerOffTimeout_) {
        return true;
    }

    if (preferences_.putUInt(kIdlePowerOffKey, seconds) != sizeof(seconds)) {
        return false;
    }

    idlePowerOffTimeout_ = timeout;
    return true;
}

bool SettingsStore::autoConnectEnabled() const {
    return autoConnectEnabled_;
}

bool SettingsStore::setAutoConnectEnabled(bool enabled) {
    if (!initialized_) {
        return false;
    }

    if (enabled == autoConnectEnabled_) {
        return true;
    }

    if (preferences_.putBool(kAutoConnectKey, enabled) != sizeof(enabled)) {
        return false;
    }

    autoConnectEnabled_ = enabled;
    return true;
}

bool SettingsStore::strokeEncoderReversed() const {
    return strokeEncoderReversed_;
}

bool SettingsStore::setStrokeEncoderReversed(bool reversed) {
    if (!initialized_) {
        return false;
    }

    if (reversed == strokeEncoderReversed_) {
        return true;
    }

    if (preferences_.putBool(kStrokeDirectionKey, reversed) != sizeof(reversed)) {
        return false;
    }

    strokeEncoderReversed_ = reversed;
    return true;
}

bool SettingsStore::savedOssmConnection(SavedOssmConnection& connection) const {
    if (!hasSavedOssmConnection_) {
        return false;
    }

    connection = savedOssmConnection_;
    return true;
}

bool SettingsStore::setSavedOssmConnection(const SavedOssmConnection& connection) {
    if (!initialized_ || !isValidSavedOssmConnection(connection)) {
        return false;
    }

    if (hasSavedOssmConnection_ && savedOssmConnection_.address == connection.address &&
        savedOssmConnection_.addressType == connection.addressType &&
        std::strncmp(savedOssmConnection_.name, connection.name, sizeof(connection.name)) == 0) {
        return true;
    }

    StoredOssmConnection stored;
    stored.address = connection.address;
    stored.addressType = connection.addressType;
    std::memcpy(stored.name, connection.name, sizeof(stored.name));
    stored.name[sizeof(stored.name) - 1] = '\0';

    if (preferences_.putBytes(kOssmConnectionKey, &stored, sizeof(stored)) != sizeof(stored)) {
        return false;
    }

    savedOssmConnection_ = connection;
    savedOssmConnection_.name[sizeof(savedOssmConnection_.name) - 1] = '\0';
    hasSavedOssmConnection_ = true;
    return true;
}

const BrightnessOption& SettingsStore::brightnessOption(std::size_t index) {
    if (index >= kBrightnessOptionCount) {
        index = brightnessOptionIndex(kDefaultBrightnessLevel);
    }
    return kBrightnessOptions[index];
}

std::size_t SettingsStore::brightnessOptionIndex(BrightnessLevel level) {
    for (std::size_t index = 0; index < kBrightnessOptionCount; ++index) {
        if (kBrightnessOptions[index].level == level) {
            return index;
        }
    }
    return static_cast<std::size_t>(kDefaultBrightnessLevel);
}

const IdleDimOption& SettingsStore::idleDimOption(std::size_t index) {
    if (index >= kIdleDimOptionCount) {
        index = kDefaultIdleDimOptionIndex;
    }
    return kIdleDimOptions[index];
}

std::size_t SettingsStore::idleDimOptionIndex(IdleDimTimeout timeout) {
    for (std::size_t index = 0; index < kIdleDimOptionCount; ++index) {
        if (kIdleDimOptions[index].timeout == timeout) {
            return index;
        }
    }
    return kDefaultIdleDimOptionIndex;
}

const IdlePowerOffOption& SettingsStore::idlePowerOffOption(std::size_t index) {
    if (index >= kIdlePowerOffOptionCount) {
        index = kDefaultIdlePowerOffOptionIndex;
    }
    return kIdlePowerOffOptions[index];
}

std::size_t SettingsStore::idlePowerOffOptionIndex(IdlePowerOffTimeout timeout) {
    for (std::size_t index = 0; index < kIdlePowerOffOptionCount; ++index) {
        if (kIdlePowerOffOptions[index].timeout == timeout) {
            return index;
        }
    }
    return kDefaultIdlePowerOffOptionIndex;
}

const AutoConnectOption& SettingsStore::autoConnectOption(std::size_t index) {
    if (index >= kAutoConnectOptionCount) {
        index = autoConnectOptionIndex(kDefaultAutoConnectEnabled);
    }
    return kAutoConnectOptions[index];
}

std::size_t SettingsStore::autoConnectOptionIndex(bool enabled) {
    for (std::size_t index = 0; index < kAutoConnectOptionCount; ++index) {
        if (kAutoConnectOptions[index].enabled == enabled) {
            return index;
        }
    }
    return 0;
}

const StrokeDirectionOption& SettingsStore::strokeDirectionOption(std::size_t index) {
    if (index >= kStrokeDirectionOptionCount) {
        index = strokeDirectionOptionIndex(kDefaultStrokeEncoderReversed);
    }
    return kStrokeDirectionOptions[index];
}

std::size_t SettingsStore::strokeDirectionOptionIndex(bool reversed) {
    for (std::size_t index = 0; index < kStrokeDirectionOptionCount; ++index) {
        if (kStrokeDirectionOptions[index].reversed == reversed) {
            return index;
        }
    }
    return 0;
}

bool SettingsStore::isValidBrightnessLevel(std::uint8_t value) {
    return value <= static_cast<std::uint8_t>(BrightnessLevel::Maximum);
}

bool SettingsStore::isValidIdleDimTimeout(std::uint32_t seconds) {
    for (const IdleDimOption& option : kIdleDimOptions) {
        if (static_cast<std::uint32_t>(option.timeout) == seconds) {
            return true;
        }
    }
    return false;
}

bool SettingsStore::isValidIdlePowerOffTimeout(std::uint32_t seconds) {
    for (const IdlePowerOffOption& option : kIdlePowerOffOptions) {
        if (static_cast<std::uint32_t>(option.timeout) == seconds) {
            return true;
        }
    }
    return false;
}

bool SettingsStore::isValidSavedOssmConnection(const SavedOssmConnection& connection) {
    constexpr std::uint64_t kBleAddressMask = 0x0000FFFFFFFFFFFFULL;
    const bool addressValid =
        connection.address != 0 && (connection.address & ~kBleAddressMask) == 0;
    // NimBLE defines public, random, public-identity, and random-identity
    // address types as values 0 through 3.
    const bool typeValid = connection.addressType <= 3;
    const bool nameTerminated =
        std::memchr(connection.name, '\0', sizeof(connection.name)) != nullptr;
    return addressValid && typeValid && nameTerminated;
}

}  // namespace m5_redux
