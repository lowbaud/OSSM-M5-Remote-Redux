#include <unity.h>

#include <vector>

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

void assertSingleCommand(
    const std::vector<Command>& commands, CommandKind kind, int value, int secondValue = 0) {
    const Command* match = nullptr;
    for (const Command& command : commands) {
        if (command.kind == kind) {
            TEST_ASSERT_NULL_MESSAGE(match, "Command kind was requested more than once");
            match = &command;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(match, "Command kind was not requested");
    assertCommand(*match, kind, value, secondValue);
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

void test_collapsed_range_support_lowers_the_stroke_floor_to_zero() {
    FakeOssmClient client;
    client.collapsedRangeSupported = true;
    OssmControl control(client);
    TEST_ASSERT_EQUAL_INT(0, control.minimumStroke());
    TEST_ASSERT_TRUE(control.apply({0, -1000, -1000, 0}));
    TEST_ASSERT_EQUAL_INT(0, control.values().stroke);
    TEST_ASSERT_EQUAL_INT(0, control.values().depth);
    assertSingleCommand(client.commands, CommandKind::Range, 0, 0);
    client.collapsedRangeSupported = false;
    TEST_ASSERT_EQUAL_INT(ossm::OssmControlClient::kMinimumOpenStroke, control.minimumStroke());
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

// Each setter publishes the full requested state, and the worker decides BLE write order, so
// tests that check several values do not assert call order.
void test_combined_adjustment_requests_each_changed_value() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({20, 5, 10, -10}));
    TEST_ASSERT_EQUAL_UINT(3, client.commands.size());
    assertSingleCommand(client.commands, CommandKind::Range, 20, 15);
    assertSingleCommand(client.commands, CommandKind::Sensation, 40);
    assertSingleCommand(client.commands, CommandKind::Speed, 20);
    TEST_ASSERT_EQUAL_INT(20, control.values().speed);
}

void test_rejected_zero_speed_does_not_request_stop() {
    FakeOssmClient client;
    OssmControl control(client);
    TEST_ASSERT_TRUE(control.apply({20, 0, 0, 0}));
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
    assertSingleCommand(client.commands, CommandKind::Pattern, 7);
    assertSingleCommand(client.commands, CommandKind::Sensation, 50);
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
    assertSingleCommand(client.commands, CommandKind::Range, 10, 10);
    assertSingleCommand(client.commands, CommandKind::Sensation, 50);
    assertSingleCommand(client.commands, CommandKind::Pattern, 0);
    assertSingleCommand(client.commands, CommandKind::Speed, 0);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_disconnected_changes_are_rejected_without_commands);
    RUN_TEST(test_adjustments_clamp_percentages_and_minimum_motion_range);
    RUN_TEST(test_collapsed_range_support_lowers_the_stroke_floor_to_zero);
    RUN_TEST(test_depth_reduction_clamps_stroke_and_sends_one_combined_range);
    RUN_TEST(test_individual_range_changes_use_individual_commands);
    RUN_TEST(test_unchanged_values_do_not_send_commands);
    RUN_TEST(test_combined_adjustment_requests_each_changed_value);
    RUN_TEST(test_rejected_zero_speed_does_not_request_stop);
    RUN_TEST(test_rejected_speed_resets_local_speed_and_requests_stop);
    RUN_TEST(test_stop_requests_zero_speed_even_when_not_ready);
    RUN_TEST(test_pattern_change_resets_sensation_and_repeated_selection_is_noop);
    RUN_TEST(test_readiness_loss_restores_defaults_and_requests_zero_speed);
    return UNITY_END();
}
