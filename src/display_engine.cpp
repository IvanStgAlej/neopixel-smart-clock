// display_engine.cpp
#include "display_engine.h"
#include <Arduino.h>
#include "glyphs.h"

// ── Per-digit 180° rotation ───────────────────────────────────────────────
// If the strip's data input (DIN) is fed from the opposite end, each digit
// comes out upside down. A 180° turn swaps the opposite segments:
//   F(2,3)↔C(8,9)   A(4,5)↔D(10,11)   B(6,7)↔E(12,13)   G(0,1) stays the same.
// ROT180[r] returns the already-rotated relative index (0-13).
static const uint8_t ROT180[14] = { 0,1, 8,9, 10,11, 12,13, 2,3, 4,5, 6,7 };

// Applies the position offset and, if FLIP_DIGITS is enabled, the rotation.
static inline uint16_t mapLed(int offset, uint8_t rel) {
#if FLIP_DIGITS
  return offset + ROT180[rel];
#else
  return offset + rel;
#endif
}

// Offset of the 14-LED block for a logical position (0 = left … 3 = right).
// With REVERSE_DIGITS the digit order is reversed (clock rotated 180°).
static inline int slotOffset(int position) {
#if REVERSE_DIGITS
  return (3 - position) * 14;
#else
  return position * 14;
#endif
}

DisplayEngine::DisplayEngine()
  : _pixels(NEO_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800) {}

void DisplayEngine::begin() {
  _pixels.begin();
  _pixels.setBrightness(NEO_BRIGHTNESS);
  _pixels.show();
  Serial.println(F("[DISPLAY] Initialized"));
}

void DisplayEngine::clear() {
  _scrolling = false;
  _pixels.clear();
  _pixels.show();
}

void DisplayEngine::setBrightness(uint8_t brightness) {
  _pixels.setBrightness(brightness);
  _pixels.show();
}

void DisplayEngine::setBaseColor(uint32_t color) {
  _baseColor = color;
}

// ── Digit rendering ───────────────────────────────────────────────────────

void DisplayEngine::_renderDigit(int position, int digit, uint32_t color) {
  if (position < 0 || position > 3) return;
  if (digit < 0 || digit > 9) return;
  _renderChar(position, (char)('0' + digit), color);
}

void DisplayEngine::showTime(int hours, int minutes) {
  _scrolling = false;
  _pixels.clear();
  uint32_t color = (_baseColor != 0) ? _baseColor : _pixels.Color(255, 255, 255);
  _renderDigit(0, hours / 10,   color);
  _renderDigit(1, hours % 10,   color);
  _renderDigit(2, minutes / 10, color);
  _renderDigit(3, minutes % 10, color);
  _pixels.show();
}

uint32_t DisplayEngine::hexToColor(const char* hex) {
  // Accepts "#RRGGBB" or "RRGGBB"
  if (!hex) return 0xFFFFFF;
  const char* p = (hex[0] == '#') ? hex + 1 : hex;
  long v = strtol(p, nullptr, 16);
  uint8_t r = (v >> 16) & 0xFF;
  uint8_t g = (v >>  8) & 0xFF;
  uint8_t b =  v        & 0xFF;
  return Adafruit_NeoPixel::Color(r, g, b);
}

// ── Character rendering ───────────────────────────────────────────────────

void DisplayEngine::_renderChar(int position, char c, uint32_t color) {
  if (position < 0 || position > 3) return;
  uint16_t mask = glyphMask(c);  // unknown characters give 0 -> blank position
  int offset = slotOffset(position);
  for (uint8_t bit = 0; bit < 14; bit++) {
    if (mask & (1u << bit)) {
      _pixels.setPixelColor(mapLed(offset, bit), color);
    }
  }
}

void DisplayEngine::showText(const char* text, uint32_t color) {
  _scrolling = false;
  _pixels.clear();
  uint32_t c = (color != 0) ? color : _baseColor;
  char t[MAX_TEXT_BYTES];  // stack, transient (freed on return)
  int n = (int)normalizeText(text, t, sizeof t);
  for (int i = 0; i < 4 && i < n; i++) {
    _renderChar(i, t[i], c);
  }
  _pixels.show();
}

void DisplayEngine::showNumber(int number, uint32_t color) {
  _scrolling = false;
  _pixels.clear();
  uint32_t c = (color != 0) ? color : _baseColor;
  number = abs(number) % 10000;

  int digits[4];
  digits[3] = number % 10;
  digits[2] = (number / 10) % 10;
  digits[1] = (number / 100) % 10;
  digits[0] = (number / 1000) % 10;

  bool leading = true;
  for (int i = 0; i < 4; i++) {
    if (leading && digits[i] == 0 && i < 3) continue; // skip leading zeros
    leading = false;
    _renderDigit(i, digits[i], c);
  }
  _pixels.show();
}

// ── Scroll ────────────────────────────────────────────────────────────────

void DisplayEngine::startScroll(const char* text, uint32_t color, int speedMs) {
  _scrollText = "    ";
  char t[MAX_TEXT_BYTES];  // stack, transient (freed on return)
  normalizeText(text, t, sizeof t);
  _scrollText += t;
  _scrollText += "    ";
  _scrollColor   = (color != 0) ? color : _baseColor;
  _scrollSpeedMs = (speedMs > 0) ? speedMs : SCROLL_DEFAULT_MS;
  _scrollPos     = 0;
  _scrolling     = true;
  _lastScrollTick = millis();
  Serial.printf("[DISPLAY] Scroll: '%s' (%dms)\n", text, _scrollSpeedMs);
}

void DisplayEngine::tickScroll() {
  if (!_scrolling) return;
  if (millis() - _lastScrollTick < (unsigned long)_scrollSpeedMs) return;
  _lastScrollTick = millis();

  _pixels.clear();
  for (int i = 0; i < 4; i++) {
    int idx = _scrollPos + i;
    if (idx < (int)_scrollText.length()) {
      _renderChar(i, _scrollText[idx], _scrollColor);
    }
  }
  _pixels.show();

  _scrollPos++;

  // Continuous loop: on reaching the end (text + padding) it starts over.
  // The 4-space padding before and after creates a clean pause between loops.
  if (_scrollPos >= (int)_scrollText.length()) {
    _scrollPos = 0;
  }
}

bool DisplayEngine::isScrolling() const {
  return _scrolling;
}
