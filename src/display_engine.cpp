// display_engine.cpp
#include "display_engine.h"
#include <Arduino.h>
#include "glyphs.h"

// ── Rotación 180° por dígito ──────────────────────────────────────────────
// Si la tira alimenta el dato (DIN) por el extremo opuesto, cada dígito sale
// boca abajo. Un giro de 180° intercambia los segmentos opuestos:
//   F(2,3)↔C(8,9)   A(4,5)↔D(10,11)   B(6,7)↔E(12,13)   G(0,1) se queda igual.
// ROT180[r] devuelve el índice relativo (0-13) ya rotado.
static const uint8_t ROT180[14] = { 0,1, 8,9, 10,11, 12,13, 2,3, 4,5, 6,7 };

// Aplica el offset de posición y, si FLIP_DIGITS está activo, la rotación.
static inline uint16_t mapLed(int offset, uint8_t rel) {
#if FLIP_DIGITS
  return offset + ROT180[rel];
#else
  return offset + rel;
#endif
}

// Offset del bloque de 14 LEDs para una posición lógica (0=izq … 3=der).
// Con REVERSE_DIGITS, el orden de los dígitos se invierte (reloj rotado 180°).
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
  Serial.println(F("[DISPLAY] Inicializado"));
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

// ── Renderizado de dígitos ────────────────────────────────────────────────

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
  // Acepta "#RRGGBB" o "RRGGBB"
  if (!hex) return 0xFFFFFF;
  const char* p = (hex[0] == '#') ? hex + 1 : hex;
  long v = strtol(p, nullptr, 16);
  uint8_t r = (v >> 16) & 0xFF;
  uint8_t g = (v >>  8) & 0xFF;
  uint8_t b =  v        & 0xFF;
  return Adafruit_NeoPixel::Color(r, g, b);
}

// ── Renderizado de caracteres ─────────────────────────────────────────────

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
    if (leading && digits[i] == 0 && i < 3) continue; // saltar ceros iniciales
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

  // Bucle continuo: al llegar al final (texto + relleno) vuelve a empezar.
  // El relleno de 4 espacios delante y detras crea una pausa limpia entre vueltas.
  if (_scrollPos >= (int)_scrollText.length()) {
    _scrollPos = 0;
  }
}

bool DisplayEngine::isScrolling() const {
  return _scrolling;
}
