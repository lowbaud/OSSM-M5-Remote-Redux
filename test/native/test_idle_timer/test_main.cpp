#include <cstdint>
#include <limits>

#include <unity.h>

#include "app/IdleTimer.h"

namespace {

void test_uninitialized_timer_reports_no_idle_time() {
    m5_redux::IdleTimer timer;

    TEST_ASSERT_EQUAL_UINT32(0, timer.idleForMs(1000));
}

void test_reset_before_begin_does_not_start_timer() {
    m5_redux::IdleTimer timer;
    timer.reset(100);

    TEST_ASSERT_EQUAL_UINT32(0, timer.idleForMs(200));
}

void test_begin_starts_tracking_elapsed_time() {
    m5_redux::IdleTimer timer;
    timer.begin(100);

    TEST_ASSERT_EQUAL_UINT32(0, timer.idleForMs(100));
    TEST_ASSERT_EQUAL_UINT32(250, timer.idleForMs(350));
}

void test_activity_resets_idle_time() {
    m5_redux::IdleTimer timer;
    timer.begin(100);
    timer.reset(350);

    TEST_ASSERT_EQUAL_UINT32(0, timer.idleForMs(350));
    TEST_ASSERT_EQUAL_UINT32(50, timer.idleForMs(400));
}

void test_begin_restarts_an_initialized_timer() {
    m5_redux::IdleTimer timer;
    timer.begin(100);
    timer.begin(400);

    TEST_ASSERT_EQUAL_UINT32(0, timer.idleForMs(400));
    TEST_ASSERT_EQUAL_UINT32(25, timer.idleForMs(425));
}

void test_elapsed_time_survives_clock_wraparound() {
    m5_redux::IdleTimer timer;
    timer.begin(std::numeric_limits<std::uint32_t>::max() - 9);

    TEST_ASSERT_EQUAL_UINT32(0, timer.idleForMs(UINT32_MAX - 9));
    TEST_ASSERT_EQUAL_UINT32(9, timer.idleForMs(UINT32_MAX));
    TEST_ASSERT_EQUAL_UINT32(10, timer.idleForMs(0));
    TEST_ASSERT_EQUAL_UINT32(15, timer.idleForMs(5));

    timer.reset(5);
    TEST_ASSERT_EQUAL_UINT32(5, timer.idleForMs(10));
}

}  // namespace

void setUp() {}

void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_uninitialized_timer_reports_no_idle_time);
    RUN_TEST(test_reset_before_begin_does_not_start_timer);
    RUN_TEST(test_begin_starts_tracking_elapsed_time);
    RUN_TEST(test_activity_resets_idle_time);
    RUN_TEST(test_begin_restarts_an_initialized_timer);
    RUN_TEST(test_elapsed_time_survives_clock_wraparound);
    return UNITY_END();
}
