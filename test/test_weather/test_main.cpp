// Unit tests for the weather overlay: text layout and temperature color.
#include <unity.h>
#include "weather.h"

void setUp() {}
void tearDown() {}

static void assertText(const char* expected, float temp, float rain) {
  char buf[5];
  formatWeather(temp, rain, buf);
  TEST_ASSERT_EQUAL_STRING(expected, buf);
}

void test_format_basic() {
  assertText("2040", 19.6f, 40);   // temperature rounds to nearest
  assertText("1940", 19.4f, 40);
  assertText("2305", 23.2f, 5);    // rain always two digits
  assertText(" 700", 7.0f, 0);     // single-digit temperature right-aligned
  assertText(" 000", 0.2f, 0);
}

void test_format_rain_clamped() {
  assertText("2099", 20, 100);     // 100 % shows as 99
  assertText("2099", 20, 250);
  assertText("2000", 20, -3);
  assertText("2050", 20, 49.6f);   // rounds
}

void test_format_temperature_range() {
  assertText("-512", -5.0f, 12);
  assertText("-912", -20.0f, 12);  // clamped to -9
  assertText("9900", 120.0f, 0);   // clamped to 99
  assertText(" 000", -0.4f, 0);    // rounds to 0, no "-0"
}

static void assertColor(uint32_t expected, float temp) {
  TEST_ASSERT_EQUAL_HEX32(expected, weatherTempColor(temp));
}

void test_color_cold_is_blue() {
  assertColor(0x0000FF, -10);
  assertColor(0x0000FF, 0);
  assertColor(0x0000FF, 10);
}

void test_color_comfort_is_white() {
  assertColor(0xFFFFFF, 20);
  assertColor(0xFFFFFF, 22.5f);
  assertColor(0xFFFFFF, 25);
}

void test_color_hot() {
  assertColor(0xFF6400, 30);       // orange
  assertColor(0xFF0000, 35);       // red
  assertColor(0xFF0000, 45);
}

void test_color_transitions() {
  assertColor(0x8080FF, 15);       // halfway blue -> white
  assertColor(0xFFB280, 27.5f);    // halfway white -> orange
  assertColor(0xFF3200, 32.5f);    // halfway orange -> red
}

void test_rain_color_is_blue() {
  TEST_ASSERT_EQUAL_HEX32(0x0000FF, kWeatherRainColor);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_format_basic);
  RUN_TEST(test_format_rain_clamped);
  RUN_TEST(test_format_temperature_range);
  RUN_TEST(test_color_cold_is_blue);
  RUN_TEST(test_color_comfort_is_white);
  RUN_TEST(test_color_hot);
  RUN_TEST(test_color_transitions);
  RUN_TEST(test_rain_color_is_blue);
  return UNITY_END();
}
