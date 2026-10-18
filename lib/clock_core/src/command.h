// command.h — MQTT command JSON parsing (no hardware dependencies)
#pragma once
#include <stddef.h>
#include <stdint.h>

// Size of Command::value including the NUL. 255 chars is the Home Assistant
// text entity maximum; must equal MAX_TEXT_BYTES in include/config.h
// (asserted in src/command_handler.cpp).
static const size_t kCommandValueBytes = 256;

// Default and maximum time a temporary overlay (weather, notice) stays on screen.
static const int32_t kOverlayDefaultSeconds = 5;
static const int32_t kOverlayMaxSeconds     = 60;

// Weather and Notice are temporary overlays: the previous content returns after `seconds`.
enum class CommandType { Invalid, Unknown, Off, Clock, Text, Number, Scroll, Timer, Weather, Notice };

struct Command {
  CommandType type;
  char value[kCommandValueBytes];  // text/number/scroll/notice, truncated and NUL-terminated
  int32_t seconds;                 // timer (0 = cancel) · overlays: duration (1-60, default 5)
  float temperature;               // weather: °C
  float rain;                      // weather: precipitation probability %
  int speedMs;                     // scroll
  bool hasColor;
  char color[8];                   // "#RRGGBB" (truncated to 7 chars + NUL)
};

// Invalid: malformed JSON. Unknown: missing or unrecognised "tipo".
Command parseCommand(const char* json, int defaultSpeedMs);
