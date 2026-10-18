// Unit tests for MQTT command parsing.
#include <unity.h>
#include <string.h>
#include <string>
#include "command.h"

void setUp() {}
void tearDown() {}

void test_text_with_color() {
  Command c = parseCommand("{\"tipo\":\"texto\",\"valor\":\"HOLA\",\"color\":\"#00FF00\"}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Text, (int)c.type);
  TEST_ASSERT_EQUAL_STRING("HOLA", c.value);
  TEST_ASSERT_TRUE(c.hasColor);
  TEST_ASSERT_EQUAL_STRING("#00FF00", c.color);
}

void test_text_without_color() {
  Command c = parseCommand("{\"tipo\":\"texto\",\"valor\":\"HI\"}", 300);
  TEST_ASSERT_FALSE(c.hasColor);
}

void test_number_default() {
  Command c = parseCommand("{\"tipo\":\"numero\"}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Number, (int)c.type);
  TEST_ASSERT_EQUAL_STRING("0", c.value);
}

void test_scroll_default_speed() {
  Command c = parseCommand("{\"tipo\":\"scroll\",\"valor\":\"hi\"}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Scroll, (int)c.type);
  TEST_ASSERT_EQUAL(300, c.speedMs);
  c = parseCommand("{\"tipo\":\"scroll\",\"valor\":\"hi\",\"velocidad\":150}", 300);
  TEST_ASSERT_EQUAL(150, c.speedMs);
}

static int32_t secs(const char* json) {
  Command c = parseCommand(json, 300);
  TEST_ASSERT_EQUAL((int)CommandType::Timer, (int)c.type);
  return c.seconds;
}

void test_timer_seconds() {
  TEST_ASSERT_EQUAL(300, secs("{\"tipo\":\"temporizador\",\"segundos\":300}"));
  TEST_ASSERT_EQUAL(90, secs("{\"tipo\":\"temporizador\",\"segundos\":90.7}"));
  TEST_ASSERT_EQUAL(0, secs("{\"tipo\":\"temporizador\",\"segundos\":\"300\"}"));
  TEST_ASSERT_EQUAL(0, secs("{\"tipo\":\"temporizador\",\"segundos\":-5}"));
  TEST_ASSERT_EQUAL(0, secs("{\"tipo\":\"temporizador\"}"));
  TEST_ASSERT_EQUAL(0, secs("{\"tipo\":\"temporizador\",\"segundos\":-0.5}"));
  // Huge values must not overflow; the caller clamps to the timer range.
  TEST_ASSERT_TRUE(secs("{\"tipo\":\"temporizador\",\"segundos\":1e30}") >= 359940);
  TEST_ASSERT_TRUE(secs("{\"tipo\":\"temporizador\",\"segundos\":99999999999}") >= 359940);
}

void test_unknown_and_invalid() {
  TEST_ASSERT_EQUAL((int)CommandType::Unknown, (int)parseCommand("{\"tipo\":\"bailar\"}", 300).type);
  TEST_ASSERT_EQUAL((int)CommandType::Unknown, (int)parseCommand("{}", 300).type);
  TEST_ASSERT_EQUAL((int)CommandType::Unknown, (int)parseCommand("{\"tipo\":5}", 300).type);
  TEST_ASSERT_EQUAL((int)CommandType::Invalid, (int)parseCommand("no json", 300).type);
  TEST_ASSERT_EQUAL((int)CommandType::Invalid, (int)parseCommand("", 300).type);
  TEST_ASSERT_EQUAL((int)CommandType::Invalid, (int)parseCommand(nullptr, 300).type);
}

void test_clock_and_off() {
  TEST_ASSERT_EQUAL((int)CommandType::Clock, (int)parseCommand("{\"tipo\":\"hora\"}", 300).type);
  TEST_ASSERT_EQUAL((int)CommandType::Off, (int)parseCommand("{\"tipo\":\"apagar\"}", 300).type);
}

void test_long_value_truncated() {
  std::string json = "{\"tipo\":\"texto\",\"valor\":\"" + std::string(300, 'A') + "\"}";
  Command c = parseCommand(json.c_str(), 300);
  TEST_ASSERT_EQUAL((int)CommandType::Text, (int)c.type);
  TEST_ASSERT_EQUAL(255, (int)strlen(c.value));
  TEST_ASSERT_EQUAL('\0', c.value[255]);
}

void test_long_color_truncated() {
  Command c = parseCommand("{\"tipo\":\"texto\",\"valor\":\"x\",\"color\":\"#00FF00AABBCC\"}", 300);
  TEST_ASSERT_TRUE(c.hasColor);
  TEST_ASSERT_EQUAL_STRING("#00FF00", c.color);
}

void test_weather() {
  Command c = parseCommand("{\"tipo\":\"tiempo\",\"temperatura\":19.6,\"lluvia\":40,\"segundos\":8}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Weather, (int)c.type);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 19.6f, c.temperature);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, c.rain);
  TEST_ASSERT_EQUAL(8, c.seconds);
}

void test_weather_defaults() {
  // Missing fields: 0 °C, 0 % and the default 5 s duration
  Command c = parseCommand("{\"tipo\":\"tiempo\"}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Weather, (int)c.type);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, c.temperature);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, c.rain);
  TEST_ASSERT_EQUAL(5, c.seconds);
  // Strings (e.g. an "unavailable" sensor) are not numbers
  c = parseCommand("{\"tipo\":\"tiempo\",\"temperatura\":\"unknown\",\"segundos\":\"x\"}", 300);
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, c.temperature);
  TEST_ASSERT_EQUAL(5, c.seconds);
  // Duration is capped at 60 s
  c = parseCommand("{\"tipo\":\"tiempo\",\"segundos\":600}", 300);
  TEST_ASSERT_EQUAL(60, c.seconds);
}

void test_notice() {
  Command c = parseCommand("{\"tipo\":\"aviso\",\"valor\":\"CARG\",\"color\":\"#FF0000\",\"segundos\":8}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Notice, (int)c.type);
  TEST_ASSERT_EQUAL_STRING("CARG", c.value);
  TEST_ASSERT_TRUE(c.hasColor);
  TEST_ASSERT_EQUAL_STRING("#FF0000", c.color);
  TEST_ASSERT_EQUAL(8, c.seconds);
}

void test_notice_defaults() {
  Command c = parseCommand("{\"tipo\":\"aviso\"}", 300);
  TEST_ASSERT_EQUAL((int)CommandType::Notice, (int)c.type);
  TEST_ASSERT_EQUAL_STRING("", c.value);
  TEST_ASSERT_FALSE(c.hasColor);
  TEST_ASSERT_EQUAL(5, c.seconds);
  c = parseCommand("{\"tipo\":\"aviso\",\"valor\":\"x\",\"segundos\":600}", 300);
  TEST_ASSERT_EQUAL(60, c.seconds);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_notice);
  RUN_TEST(test_notice_defaults);
  RUN_TEST(test_weather);
  RUN_TEST(test_weather_defaults);
  RUN_TEST(test_text_with_color);
  RUN_TEST(test_text_without_color);
  RUN_TEST(test_number_default);
  RUN_TEST(test_scroll_default_speed);
  RUN_TEST(test_timer_seconds);
  RUN_TEST(test_unknown_and_invalid);
  RUN_TEST(test_clock_and_off);
  RUN_TEST(test_long_value_truncated);
  RUN_TEST(test_long_color_truncated);
  return UNITY_END();
}
