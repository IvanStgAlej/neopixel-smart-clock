// weather.cpp — weather overlay layout and colors
#include "weather.h"
#include <math.h>
#include <stdio.h>

static int roundClamp(float v, int lo, int hi) {
  if (!(v == v)) return lo;  // NaN
  int r = (int)lroundf(v);
  if (r < lo) return lo;
  if (r > hi) return hi;
  return r;
}

void formatWeather(float temperatureC, float rainPercent, char out[5]) {
  int t = roundClamp(temperatureC, -9, 99);
  int r = roundClamp(rainPercent, 0, 99);
  // "%2d" right-aligns single digits with a space and prints -9..-1 as "-N"
  snprintf(out, 5, "%2d%02d", t, r);
}

// Temperature → color stops, linearly interpolated between neighbours.
struct ColorStop { float temp; uint8_t r, g, b; };
static const ColorStop kStops[] = {
  { 10.0f,   0,   0, 255 },  // blue
  { 20.0f, 255, 255, 255 },  // white (comfort starts)
  { 25.0f, 255, 255, 255 },  // white (comfort ends)
  { 30.0f, 255, 100,   0 },  // orange
  { 35.0f, 255,   0,   0 },  // red
};
static const int kStopCount = sizeof(kStops) / sizeof(kStops[0]);

static uint32_t pack(const ColorStop& s) {
  return ((uint32_t)s.r << 16) | ((uint32_t)s.g << 8) | s.b;
}

static uint8_t lerp(uint8_t a, uint8_t b, float f) {
  return (uint8_t)lroundf(a + (b - a) * f);
}

uint32_t weatherTempColor(float temperatureC) {
  if (!(temperatureC == temperatureC) || temperatureC <= kStops[0].temp) return pack(kStops[0]);
  for (int i = 1; i < kStopCount; i++) {
    if (temperatureC <= kStops[i].temp) {
      const ColorStop& a = kStops[i - 1];
      const ColorStop& b = kStops[i];
      float f = (temperatureC - a.temp) / (b.temp - a.temp);
      return ((uint32_t)lerp(a.r, b.r, f) << 16) | ((uint32_t)lerp(a.g, b.g, f) << 8) | lerp(a.b, b.b, f);
    }
  }
  return pack(kStops[kStopCount - 1]);
}
