#include <unity.h>

#include "FakeOssmClient.h"
#include "devices/ossm/OssmControl.h"

using namespace m5_redux;
using namespace test_support;

namespace {

void assertCommand(const Command& command, CommandKind kind, int value, int secondValue = 0) {
    TEST_ASSERT_EQUAL_INT(static_cast<int>(kind), static_cast<int>(command.kind));
    TEST_ASSERT_EQUAL_INT(value, command.value);
    TEST_ASSERT_EQUAL_INT(secondValue, command.secondValue);
}

void test_disconnected_changes_are_rejected_without_commands() {
    FakeOssmClient client;
    client.ready = false;
    OssmControl control(client);
    TEST_ASSERT_FALSE(control.apply({10, 10, 10, 10}));
    TEST_ASSERT_FALSE(control.setPattern(2));
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_INT(10, control.values().depth);
    TEST_ASSERT_EQUAL_INT(10, control.values().stroke);
    TEST_ASSERT_EQUAL_INT(50, control.values().sensation);
    TEST_ASSERT_EQUAL_INT(0, control.values().pattern);
    TEST_ASSERT_TRUE(client.commands.empty());
}

void test_adjustments_clamp_percentages_and_minimum_motion_range() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({1000, 1000, 1000, 1000}));
    TEST_ASSERT_EQUAL_INT(100, control.values().speed);
    TEST_ASSERT_EQUAL_INT(100, control.values().stroke);
    TEST_ASSERT_EQUAL_INT(100, control.values().depth);
    TEST_ASSERT_EQUAL_INT(100, control.values().sensation);
    TEST_ASSERT_TRUE(control.apply({-1000, -1000, -1000, -1000}));
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_INT(3, control.values().stroke);
    TEST_ASSERT_EQUAL_INT(3, control.values().depth);
    TEST_ASSERT_EQUAL_INT(0, control.values().sensation);
    client.commands.clear();
    TEST_ASSERT_FALSE(control.apply({-1, -1, -1, -1}));
    TEST_ASSERT_TRUE(client.commands.empty());
}

void test_depth_reduction_clamps_stroke_and_sends_one_combined_range() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({0, 0, -5, 0}));
    TEST_ASSERT_EQUAL_INT(5, control.values().depth);
    TEST_ASSERT_EQUAL_INT(5, control.values().stroke);
    TEST_ASSERT_EQUAL_UINT(1, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Range, 5, 5);
}

void test_individual_range_changes_use_individual_commands() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({0, 0, 10, 0}));
    TEST_ASSERT_TRUE(control.apply({0, 5, 0, 0}));
    TEST_ASSERT_EQUAL_UINT(2, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Depth, 20);
    assertCommand(client.commands[1], CommandKind::Stroke, 15);
}

void test_unchanged_values_do_not_send_commands() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_FALSE(control.apply({}));
    TEST_ASSERT_TRUE(control.setPattern(0));
    TEST_ASSERT_FALSE(control.setPattern(-1));
    TEST_ASSERT_TRUE(client.commands.empty());
}

void test_motion_parameters_are_sent_before_speed() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({20, 5, 10, -10}));
    TEST_ASSERT_EQUAL_UINT(3, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Range, 20, 15);
    assertCommand(client.commands[1], CommandKind::Sensation, 40);
    assertCommand(client.commands[2], CommandKind::Speed, 20);
    TEST_ASSERT_EQUAL_INT(20, control.values().speed);
    client.commands.clear();
    client.acceptSpeed = false;
    TEST_ASSERT_TRUE(control.apply({-20, 0, 0, 0}));
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_UINT(1, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Speed, 0);
}

void test_rejected_speed_resets_local_speed_and_requests_stop() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({20, 0, 0, 0}));
    client.commands.clear();
    client.acceptSpeed = false;
    TEST_ASSERT_TRUE(control.apply({5, 0, 0, 0}));
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_UINT(2, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Speed, 25);
    assertCommand(client.commands[1], CommandKind::Stop, 0);
}

void test_stop_requests_zero_speed_even_when_not_ready() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({20, 0, 0, 0});
    client.commands.clear();
    client.ready = false;
    control.stop();
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_UINT(1, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Speed, 0);
}

void test_pattern_change_resets_sensation_and_repeated_selection_is_noop() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({0, 0, 0, 25});
    client.commands.clear();
    TEST_ASSERT_TRUE(control.setPattern(7));
    TEST_ASSERT_EQUAL_INT(7, control.values().pattern);
    TEST_ASSERT_EQUAL_INT(50, control.values().sensation);
    TEST_ASSERT_EQUAL_UINT(2, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Pattern, 7);
    assertCommand(client.commands[1], CommandKind::Sensation, 50);
    client.commands.clear();
    TEST_ASSERT_TRUE(control.setPattern(7));
    TEST_ASSERT_TRUE(client.commands.empty());
}

void test_readiness_loss_restores_defaults_and_requests_zero_speed() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({30, 20, 40, 25});
    control.setPattern(7);
    client.commands.clear();
    client.ready = false;
    control.handleReadinessLost();
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_INT(10, control.values().stroke);
    TEST_ASSERT_EQUAL_INT(10, control.values().depth);
    TEST_ASSERT_EQUAL_INT(50, control.values().sensation);
    TEST_ASSERT_EQUAL_INT(0, control.values().pattern);
    TEST_ASSERT_EQUAL_UINT(4, client.commands.size());
    assertCommand(client.commands[0], CommandKind::Range, 10, 10);
    assertCommand(client.commands[1], CommandKind::Sensation, 50);
    assertCommand(client.commands[2], CommandKind::Pattern, 0);
    assertCommand(client.commands[3], CommandKind::Speed, 0);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_disconnected_changes_are_rejected_without_commands);
    RUN_TEST(test_adjustments_clamp_percentages_and_minimum_motion_range);
    RUN_TEST(test_depth_reduction_clamps_stroke_and_sends_one_combined_range);
    RUN_TEST(test_individual_range_changes_use_individual_commands);
    RUN_TEST(test_unchanged_values_do_not_send_commands);
    RUN_TEST(test_motion_parameters_are_sent_before_speed);
    RUN_TEST(test_rejected_speed_resets_local_speed_and_requests_stop);
    RUN_TEST(test_stop_requests_zero_speed_even_when_not_ready);
    RUN_TEST(test_pattern_change_resets_sensation_and_repeated_selection_is_noop);
    RUN_TEST(test_readiness_loss_restores_defaults_and_requests_zero_speed);
    return UNITY_END();
}
