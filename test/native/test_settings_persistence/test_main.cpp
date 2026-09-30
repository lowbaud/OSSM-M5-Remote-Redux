#include <algorithm>
#include <cstring>

#include <unity.h>

#include "settings/SettingsStore.h"

using namespace m5_redux;

namespace {

SavedOssmConnection connection() {
    SavedOssmConnection value;
    value.address = 0x123456789ABCULL;
    value.addressType = 1;
    std::strcpy(value.name, "Test OSSM");
    return value;
}

template <typename T>
void checkScalarPersistence(
    bool (SettingsStore::*setter)(T), T (SettingsStore::*getter)() const, T next) {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    const T original = (store.*getter)();
    TEST_ASSERT_TRUE(original != next);

    test_support::preferences.failWrites = true;
    TEST_ASSERT_FALSE((store.*setter)(next));
    TEST_ASSERT_TRUE((store.*getter)() == original);
    SettingsStore afterFailure;
    TEST_ASSERT_TRUE(afterFailure.begin());
    TEST_ASSERT_TRUE((afterFailure.*getter)() == original);

    test_support::preferences.failWrites = false;
    TEST_ASSERT_TRUE((store.*setter)(next));
    TEST_ASSERT_TRUE((store.*getter)() == next);
    const auto writes = test_support::preferences.writeCalls;
    TEST_ASSERT_TRUE((store.*setter)(next));
    TEST_ASSERT_EQUAL_UINT(writes, test_support::preferences.writeCalls);

    SettingsStore reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_TRUE((reopened.*getter)() == next);
}

void test_all_scalar_settings_persist_and_failed_writes_keep_previous_values() {
    checkScalarPersistence(
        &SettingsStore::setBrightnessLevel,
        &SettingsStore::brightnessLevel,
        BrightnessLevel::Maximum);
    checkScalarPersistence(
        &SettingsStore::setIdleDimTimeout, &SettingsStore::idleDimTimeout, IdleDimTimeout::Never);
    checkScalarPersistence(
        &SettingsStore::setIdlePowerOffTimeout,
        &SettingsStore::idlePowerOffTimeout,
        IdlePowerOffTimeout::Minutes60);
    checkScalarPersistence(
        &SettingsStore::setAutoConnectEnabled, &SettingsStore::autoConnectEnabled, false);
    checkScalarPersistence(
        &SettingsStore::setTouchscreenEnabled, &SettingsStore::touchscreenEnabled, false);
    checkScalarPersistence(
        &SettingsStore::setDepthControlMode,
        &SettingsStore::depthControlMode,
        DepthControlMode::MinMax);
    checkScalarPersistence(
        &SettingsStore::setStrokeEncoderReversed, &SettingsStore::strokeEncoderReversed, true);
}

void test_uninitialized_store_rejects_all_writes() {
    SettingsStore store;
    TEST_ASSERT_FALSE(store.setBrightnessLevel(BrightnessLevel::High));
    TEST_ASSERT_FALSE(store.setIdleDimTimeout(IdleDimTimeout::Never));
    TEST_ASSERT_FALSE(store.setIdlePowerOffTimeout(IdlePowerOffTimeout::Never));
    TEST_ASSERT_FALSE(store.setAutoConnectEnabled(false));
    TEST_ASSERT_FALSE(store.setTouchscreenEnabled(false));
    TEST_ASSERT_FALSE(store.setDepthControlMode(DepthControlMode::MinMax));
    TEST_ASSERT_FALSE(store.setStrokeEncoderReversed(true));
    TEST_ASSERT_FALSE(store.setDefaultPattern("Wave"));
    TEST_ASSERT_FALSE(store.setSavedOssmConnection(connection()));
    TEST_ASSERT_EQUAL_UINT(0, test_support::preferences.writeCalls);
}

void test_failed_begin_can_be_retried_and_successful_begin_is_idempotent() {
    SettingsStore store;
    test_support::preferences.failBegin = true;
    TEST_ASSERT_FALSE(store.begin());
    TEST_ASSERT_FALSE(store.setAutoConnectEnabled(false));
    test_support::preferences.failBegin = false;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_EQUAL_UINT(2, test_support::preferences.beginCalls);
}

void test_invalid_values_are_rejected_without_writes() {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_FALSE(store.setBrightnessLevel(static_cast<BrightnessLevel>(255)));
    TEST_ASSERT_FALSE(store.setIdleDimTimeout(static_cast<IdleDimTimeout>(31)));
    TEST_ASSERT_FALSE(store.setIdlePowerOffTimeout(static_cast<IdlePowerOffTimeout>(1)));
    TEST_ASSERT_FALSE(store.setDepthControlMode(static_cast<DepthControlMode>(255)));
    TEST_ASSERT_EQUAL_UINT(0, test_support::preferences.writeCalls);
    TEST_ASSERT_TRUE(store.brightnessLevel() == SettingsStore::kDefaultBrightnessLevel);
    TEST_ASSERT_TRUE(store.idleDimTimeout() == SettingsStore::kDefaultIdleDimTimeout);
    TEST_ASSERT_TRUE(store.idlePowerOffTimeout() == SettingsStore::kDefaultIdlePowerOffTimeout);
    TEST_ASSERT_TRUE(store.depthControlMode() == SettingsStore::kDefaultDepthControlMode);
}

void test_invalid_stored_values_fall_back_to_defaults() {
    Preferences seed;
    TEST_ASSERT_TRUE(seed.begin("m5-redux", false));
    seed.putUChar("brightness", 255);
    seed.putUInt("idle_dim", 31);
    seed.putUInt("idle_power_off", 1);
    seed.putUChar("depth_control", 255);
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.brightnessLevel() == SettingsStore::kDefaultBrightnessLevel);
    TEST_ASSERT_TRUE(store.idleDimTimeout() == SettingsStore::kDefaultIdleDimTimeout);
    TEST_ASSERT_TRUE(store.idlePowerOffTimeout() == SettingsStore::kDefaultIdlePowerOffTimeout);
    TEST_ASSERT_TRUE(store.depthControlMode() == SettingsStore::kDefaultDepthControlMode);
}

void test_default_pattern_round_trip_and_failed_replacement() {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.setDefaultPattern("Wave"));
    const auto writes = test_support::preferences.writeCalls;
    TEST_ASSERT_TRUE(store.setDefaultPattern("Wave"));
    TEST_ASSERT_EQUAL_UINT(writes, test_support::preferences.writeCalls);
    test_support::preferences.failWrites = true;
    TEST_ASSERT_FALSE(store.setDefaultPattern("Pulse"));
    TEST_ASSERT_EQUAL_STRING("Wave", store.defaultPattern().c_str());
    SettingsStore reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_EQUAL_STRING("Wave", reopened.defaultPattern().c_str());
}

void test_clearing_default_pattern_removes_key_only_after_success() {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.setDefaultPattern("Wave"));
    test_support::preferences.failWrites = true;
    TEST_ASSERT_FALSE(store.setDefaultPattern(""));
    TEST_ASSERT_EQUAL_STRING("Wave", store.defaultPattern().c_str());
    test_support::preferences.failWrites = false;
    TEST_ASSERT_TRUE(store.setDefaultPattern(""));
    TEST_ASSERT_EQUAL_UINT(0, test_support::preferences.values.count("m5-redux/default_pattern"));
    SettingsStore reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_TRUE(reopened.defaultPattern().empty());
}

void test_saved_connection_round_trip_and_failed_replacement() {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    const auto saved = connection();
    TEST_ASSERT_TRUE(store.setSavedOssmConnection(saved));
    const auto writes = test_support::preferences.writeCalls;
    TEST_ASSERT_TRUE(store.setSavedOssmConnection(saved));
    TEST_ASSERT_EQUAL_UINT(writes, test_support::preferences.writeCalls);
    auto changed = saved;
    changed.address = 0xAABBCCDDEEFFULL;
    test_support::preferences.failWrites = true;
    TEST_ASSERT_FALSE(store.setSavedOssmConnection(changed));
    SavedOssmConnection actual;
    TEST_ASSERT_TRUE(store.savedOssmConnection(actual));
    TEST_ASSERT_EQUAL_UINT64(saved.address, actual.address);
    SettingsStore reopened;
    TEST_ASSERT_TRUE(reopened.begin());
    TEST_ASSERT_TRUE(reopened.savedOssmConnection(actual));
    TEST_ASSERT_EQUAL_UINT64(saved.address, actual.address);
    TEST_ASSERT_EQUAL_UINT8(saved.addressType, actual.addressType);
    TEST_ASSERT_EQUAL_STRING(saved.name, actual.name);
}

void test_invalid_saved_connections_are_rejected_without_writes() {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    auto invalid = connection();
    invalid.address = 0;
    TEST_ASSERT_FALSE(store.setSavedOssmConnection(invalid));
    invalid = connection();
    invalid.address = 0x1000000000000ULL;
    TEST_ASSERT_FALSE(store.setSavedOssmConnection(invalid));
    invalid = connection();
    invalid.addressType = 4;
    TEST_ASSERT_FALSE(store.setSavedOssmConnection(invalid));
    invalid = connection();
    std::memset(invalid.name, 'x', sizeof(invalid.name));
    TEST_ASSERT_FALSE(store.setSavedOssmConnection(invalid));
    TEST_ASSERT_FALSE(store.savedOssmConnection(invalid));
    TEST_ASSERT_EQUAL_UINT(0, test_support::preferences.writeCalls);
}

void test_corrupt_connection_blobs_are_ignored() {
    SettingsStore store;
    TEST_ASSERT_TRUE(store.begin());
    TEST_ASSERT_TRUE(store.setSavedOssmConnection(connection()));
    const auto valid = test_support::preferences.values.at("m5-redux/ossm_conn");
    TEST_ASSERT_EQUAL_UINT(48, valid.size());
    // The persisted schema is address[8], name[32], version[1], type[1], reserved[6].
    for (int corruption = 0; corruption < 5; ++corruption) {
        auto blob = valid;
        switch (corruption) {
            case 0:
                blob.pop_back();
                break;
            case 1:
                blob[40] = 2;
                break;
            case 2:
                blob[41] = 4;
                break;
            case 3:
                std::fill(blob.begin() + 8, blob.begin() + 40, 'x');
                break;
            case 4:
                std::fill(blob.begin(), blob.begin() + 8, 0);
                break;
        }
        test_support::preferences.values["m5-redux/ossm_conn"] = blob;
        SettingsStore reopened;
        TEST_ASSERT_TRUE(reopened.begin());
        SavedOssmConnection actual;
        TEST_ASSERT_FALSE(reopened.savedOssmConnection(actual));
    }
}

}  // namespace

void setUp() {
    test_support::preferences = {};
}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_all_scalar_settings_persist_and_failed_writes_keep_previous_values);
    RUN_TEST(test_uninitialized_store_rejects_all_writes);
    RUN_TEST(test_failed_begin_can_be_retried_and_successful_begin_is_idempotent);
    RUN_TEST(test_invalid_values_are_rejected_without_writes);
    RUN_TEST(test_invalid_stored_values_fall_back_to_defaults);
    RUN_TEST(test_default_pattern_round_trip_and_failed_replacement);
    RUN_TEST(test_clearing_default_pattern_removes_key_only_after_success);
    RUN_TEST(test_saved_connection_round_trip_and_failed_replacement);
    RUN_TEST(test_invalid_saved_connections_are_rejected_without_writes);
    RUN_TEST(test_corrupt_connection_blobs_are_ignored);
    return UNITY_END();
}
