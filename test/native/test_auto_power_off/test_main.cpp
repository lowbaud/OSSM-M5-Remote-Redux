#include <cstdint>
#include <limits>

#include <unity.h>

#include "FakeM5Platform.h"
#include "app/AutoPowerOffController.h"

using m5_redux::AutoPowerOffController;

namespace {

void test_uninitialized_and_disabled_controllers_do_not_shutdown() {
    AutoPowerOffController controller;
    controller.update(10000, 10000, false);
    controller.begin(0, 0, false);
    controller.update(10000, 10000, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
}

void test_shutdown_requires_both_idle_and_battery_timeouts() {
    AutoPowerOffController controller;
    controller.begin(1, 100, false);
    controller.update(1099, 1000, false);
    controller.update(1100, 999, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.update(1100, 1000, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
    controller.update(5000, 4900, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
}

void test_external_power_blocks_shutdown_and_unplugging_restarts_grace_period() {
    AutoPowerOffController controller;
    controller.begin(1, 0, true);
    controller.update(10000, 10000, true);
    controller.update(10000, 10000, false);
    controller.update(10999, 10999, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.update(11000, 11000, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
}

void test_reconnecting_external_power_restarts_next_battery_period() {
    AutoPowerOffController controller;
    controller.begin(1, 0, false);
    controller.update(900, 900, true);
    controller.update(2000, 2000, false);
    controller.update(2999, 2999, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.update(3000, 3000, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
}

void test_activity_prevents_shutdown_until_idle_timeout() {
    AutoPowerOffController controller;
    controller.begin(1, 0, false);
    controller.update(5000, 0, false);
    controller.update(5999, 999, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.update(6000, 1000, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
}

void test_battery_grace_period_survives_clock_wraparound() {
    AutoPowerOffController controller;
    controller.begin(1, std::numeric_limits<std::uint32_t>::max() - 499, false);
    controller.update(499, 1000, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.update(500, 1000, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
}

void test_timeout_changes_disable_and_rearm_shutdown() {
    AutoPowerOffController controller;
    controller.begin(1, 0, false);
    controller.setTimeoutSeconds(0);
    controller.update(2000, 2000, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.setTimeoutSeconds(3);
    controller.update(2999, 2999, false);
    TEST_ASSERT_EQUAL_UINT(0, test_support::powerOffCalls);
    controller.update(3000, 3000, false);
    TEST_ASSERT_EQUAL_UINT(1, test_support::powerOffCalls);
    controller.setTimeoutSeconds(4);
    controller.update(4000, 4000, false);
    TEST_ASSERT_EQUAL_UINT(2, test_support::powerOffCalls);
}

}  // namespace

void setUp() {
    test_support::powerOffCalls = 0;
}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_uninitialized_and_disabled_controllers_do_not_shutdown);
    RUN_TEST(test_shutdown_requires_both_idle_and_battery_timeouts);
    RUN_TEST(test_external_power_blocks_shutdown_and_unplugging_restarts_grace_period);
    RUN_TEST(test_reconnecting_external_power_restarts_next_battery_period);
    RUN_TEST(test_activity_prevents_shutdown_until_idle_timeout);
    RUN_TEST(test_battery_grace_period_survives_clock_wraparound);
    RUN_TEST(test_timeout_changes_disable_and_rearm_shutdown);
    return UNITY_END();
}
