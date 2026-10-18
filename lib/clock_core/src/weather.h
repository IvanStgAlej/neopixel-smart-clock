// weather.h — weather overlay layout and colors (no hardware dependencies)
#pragma once
#include <stdint.h>

// Color of the rain digits (0xRRGGBB): plain blue.
static const uint32_t kWeatherRainColor = 0x0000FF;

// Writes the 4 display characters + NUL into out:
//   positions 0-1: temperature rounded, right-aligned, clamped to -9..99
//   positions 2-3: rain probability rounded, two digits, clamped to 00..99
void formatWeather(float temperatureC, float rainPercent, char out[5]);

// Temperature color (0xRRGGBB): blue up to 10 °C, fades to white at 20 °C,
// white (comfort) until 25 °C, orange at 30 °C and red from 35 °C.
uint32_t weatherTempColor(float temperatureC);
