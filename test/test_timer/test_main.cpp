// Unit tests for timer formatting and the countdown state machine.
#include <unity.h>
#include <string.h>
#include "timer_format.h"
#include "countdown.h"

void setUp() {}
void tearDown() {}

static void expectFmt(long s, const char* want) {
  char out[5];
  formatTimer(s, out);
  TEST_ASSERT_EQUAL_STRING(want, out);
}

void test_format() {
  expectFmt(300, "0500");
  expectFmt(59, "0059");
  expectFmt(5999, "9959");
  expectFmt(6000, "0140");
  expectFmt(359940, "9959");
}

void test_clamp() {
  TEST_ASSERT_EQUAL(359940, clampTimerSeconds(400000));
  TEST_ASSERT_EQUAL(0, clampTimerSeconds(-5));
  TEST_ASSERT_EQUAL(300, clampTimerSeconds(300));
}

static void expectFrame(Countdown& c, unsigned long now, CountdownPhase p, long secs, bool changed) {
  CountdownFrame f = c.tick(now);
  TEST_ASSERT_EQUAL((int)p, (int)f.phase);
  TEST_ASSERT_EQUAL(secs, f.secondsLeft);
  TEST_ASSERT_EQUAL(changed, f.changed);
}

void test_countdown_sequence() {
  Countdown c(10000, 500);
  c.start(1000, 3);
  expectFrame(c, 1000, CountdownPhase::Running, 3, true);
  expectFrame(c, 1500, CountdownPhase::Running, 3, false);
  expectFrame(c, 2001, CountdownPhase::Running, 2, true);
  expectFrame(c, 3999, CountdownPhase::Running, 1, true);
  expectFrame(c, 4000, CountdownPhase::AlarmOn, 0, true);
  expectFrame(c, 4500, CountdownPhase::AlarmOff, 0, true);
  expectFrame(c, 5000, CountdownPhase::AlarmOn, 0, true);
  TEST_ASSERT_TRUE(c.active());
  expectFrame(c, 14000, CountdownPhase::Finished, 0, true);
  expectFrame(c, 14001, CountdownPhase::Idle, 0, false);
  TEST_ASSERT_FALSE(c.active());
}

void test_millis_overflow() {
  Countdown c(10000, 500);
  c.start(0xFFFFFC18UL /* -1000 */, 5);
  CountdownFrame f = c.tick(0xFFFFFFFFUL);
  TEST_ASSERT_EQUAL((int)CountdownPhase::Running, (int)f.phase);
  TEST_ASSERT_EQUAL(5, f.secondsLeft);
  f = c.tick(3000);
  TEST_ASSERT_EQUAL((int)CountdownPhase::Running, (int)f.phase);
  TEST_ASSERT_EQUAL(1, f.secondsLeft);
}

void test_cancel() {
  Countdown c(10000, 500);
  c.start(0, 10);
  c.tick(0);
  c.cancel();
  TEST_ASSERT_FALSE(c.active());
  TEST_ASSERT_EQUAL((int)CountdownPhase::Idle, (int)c.tick(100).phase);
}

void test_restart_during_alarm() {
  Countdown c(10000, 500);
  c.start(0, 1);
  c.tick(0);
  c.tick(1000);  // alarm on
  CountdownFrame f = c.tick(1500);
  TEST_ASSERT_EQUAL((int)CountdownPhase::AlarmOff, (int)f.phase);
  c.start(2000, 60);
  expectFrame(c, 2000, CountdownPhase::Running, 60, true);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_format);
  RUN_TEST(test_clamp);
  RUN_TEST(test_countdown_sequence);
  RUN_TEST(test_millis_overflow);
  RUN_TEST(test_cancel);
  RUN_TEST(test_restart_during_alarm);
  return UNITY_END();
}
