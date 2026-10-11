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

void test_pause_stops_and_resume_ramps_back_to_the_paused_speed() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({40, 0, 0, 0});
    client.commands.clear();

    control.togglePause(1000);
    TEST_ASSERT_TRUE(control.action() == OssmAction::Paused);
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_INT(40, control.pausedSpeed());
    assertSingleCommand(client.commands, CommandKind::Speed, 0);
    TEST_ASSERT_FALSE(control.update(5000));

    control.togglePause(10000);
    TEST_ASSERT_TRUE(control.action() == OssmAction::Resuming);
    TEST_ASSERT_TRUE(control.update(11000));
    TEST_ASSERT_EQUAL_INT(20, control.values().speed);
    TEST_ASSERT_TRUE(control.update(12000));
    TEST_ASSERT_EQUAL_INT(40, control.values().speed);
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_FALSE(control.update(13000));
}

void test_pause_needs_motion_and_pausing_while_resuming_keeps_the_paused_speed() {
    FakeOssmClient client;
    OssmControl control(client);
    control.togglePause(0);
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);

    control.apply({40, 0, 0, 0});
    control.togglePause(0);
    control.togglePause(1000);
    control.update(2000);
    control.togglePause(2000);
    TEST_ASSERT_TRUE(control.action() == OssmAction::Paused);
    TEST_ASSERT_EQUAL_INT(40, control.pausedSpeed());
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
}

void test_speed_knob_ends_a_pause_while_range_and_sensation_keep_it() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({40, 0, 0, 0});
    control.togglePause(0);

    TEST_ASSERT_TRUE(control.apply({0, 5, 5, 5}));
    TEST_ASSERT_TRUE(control.action() == OssmAction::Paused);
    TEST_ASSERT_EQUAL_INT(15, control.values().depth);
    TEST_ASSERT_TRUE(control.setPattern(3));
    TEST_ASSERT_TRUE(control.action() == OssmAction::Paused);

    TEST_ASSERT_TRUE(control.apply({5, 0, 0, 0}));
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_EQUAL_INT(5, control.values().speed);
    TEST_ASSERT_EQUAL_INT(0, control.pausedSpeed());
}

void test_ramp_raises_the_speed_over_its_duration_and_stops_at_the_maximum() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({30, 0, 0, 0});

    control.toggleRamp(20, 10000, 0);
    TEST_ASSERT_TRUE(control.action() == OssmAction::Ramping);
    TEST_ASSERT_TRUE(control.update(5000));
    TEST_ASSERT_EQUAL_INT(40, control.values().speed);
    TEST_ASSERT_FALSE(control.update(5100));
    TEST_ASSERT_TRUE(control.update(10000));
    TEST_ASSERT_EQUAL_INT(50, control.values().speed);
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);

    control.apply({45, 0, 0, 0});
    control.toggleRamp(20, 10000, 0);
    control.update(10000);
    TEST_ASSERT_EQUAL_INT(100, control.values().speed);
    control.toggleRamp(20, 10000, 0);
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
}

void test_ramp_toggles_off_and_the_speed_knob_takes_over() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({30, 0, 0, 0});
    control.toggleRamp(20, 10000, 0);
    control.update(5000);

    control.toggleRamp(20, 10000, 5000);
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_FALSE(control.update(10000));
    TEST_ASSERT_EQUAL_INT(40, control.values().speed);

    control.toggleRamp(20, 10000, 0);
    TEST_ASSERT_TRUE(control.apply({0, 0, 5, 5}));
    TEST_ASSERT_TRUE(control.action() == OssmAction::Ramping);
    TEST_ASSERT_TRUE(control.apply({-2, 0, 0, 0}));
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_EQUAL_INT(38, control.values().speed);
}

void test_pause_during_a_ramp_keeps_the_speed_reached() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({30, 0, 0, 0});
    control.toggleRamp(20, 10000, 0);
    control.update(5000);

    control.togglePause(5000);
    TEST_ASSERT_TRUE(control.action() == OssmAction::Paused);
    TEST_ASSERT_EQUAL_INT(40, control.pausedSpeed());
}

void test_pull_out_moves_to_the_start_ignores_input_and_then_stops() {
    FakeOssmClient client;
    client.collapsedRangeSupported = true;
    OssmControl control(client);
    control.apply({40, 20, 40, 0});
    client.commands.clear();

    control.pullOut(0);
    TEST_ASSERT_TRUE(control.action() == OssmAction::PullingOut);
    TEST_ASSERT_EQUAL_INT(2, control.values().depth);
    TEST_ASSERT_EQUAL_INT(0, control.values().stroke);
    TEST_ASSERT_EQUAL_INT(10, control.values().speed);
    assertSingleCommand(client.commands, CommandKind::Range, 2, 0);
    assertSingleCommand(client.commands, CommandKind::Speed, 10);

    client.commands.clear();
    TEST_ASSERT_FALSE(control.apply({10, 10, 10, 10}));
    TEST_ASSERT_FALSE(control.setPattern(3));
    control.togglePause(500);
    control.toggleRamp(20, 1000, 500);
    TEST_ASSERT_TRUE(client.commands.empty());
    TEST_ASSERT_TRUE(control.action() == OssmAction::PullingOut);

    TEST_ASSERT_FALSE(control.update(1999));
    TEST_ASSERT_TRUE(control.update(2000));
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
    TEST_ASSERT_EQUAL_INT(2, control.values().depth);
    assertSingleCommand(client.commands, CommandKind::Speed, 0);
}

void test_official_firmware_pulls_out_to_a_short_stroke() {
    FakeOssmClient client;
    client.collapsedRangeSupported = true;
    client.officialFirmware = true;
    OssmControl control(client);

    control.pullOut(0);
    TEST_ASSERT_EQUAL_INT(7, control.values().depth);
    TEST_ASSERT_EQUAL_INT(5, control.values().stroke);
}

void test_stop_and_a_refused_speed_end_the_running_action() {
    FakeOssmClient client;
    OssmControl control(client);
    control.apply({40, 0, 0, 0});
    control.togglePause(0);
    control.stop();
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_EQUAL_INT(0, control.pausedSpeed());

    control.pullOut(0);
    control.stop();
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);

    control.toggleRamp(20, 1000, 0);
    client.acceptSpeed = false;
    TEST_ASSERT_TRUE(control.update(500));
    TEST_ASSERT_TRUE(control.action() == OssmAction::None);
    TEST_ASSERT_EQUAL_INT(0, control.values().speed);
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
    RUN_TEST(test_pause_stops_and_resume_ramps_back_to_the_paused_speed);
    RUN_TEST(test_pause_needs_motion_and_pausing_while_resuming_keeps_the_paused_speed);
    RUN_TEST(test_speed_knob_ends_a_pause_while_range_and_sensation_keep_it);
    RUN_TEST(test_ramp_raises_the_speed_over_its_duration_and_stops_at_the_maximum);
    RUN_TEST(test_ramp_toggles_off_and_the_speed_knob_takes_over);
    RUN_TEST(test_pause_during_a_ramp_keeps_the_speed_reached);
    RUN_TEST(test_pull_out_moves_to_the_start_ignores_input_and_then_stops);
    RUN_TEST(test_official_firmware_pulls_out_to_a_short_stroke);
    RUN_TEST(test_stop_and_a_refused_speed_end_the_running_action);
    return UNITY_END();
}
