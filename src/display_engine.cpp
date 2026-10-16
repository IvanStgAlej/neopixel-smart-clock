// display_engine.cpp
#include "display_engine.h"
#include <Arduino.h>

// ── Tabla de dígitos (0-9) — COPIADA EXACTA del sketch original (fuente de verdad)
// Formato: {count, led0, led1, ..., 0-relleno} (15 entradas total)
// Segmentos verificados: G=0,1  F=2,3  A=4,5  B=6,7  C=8,9  D=10,11  E=12,13
static const uint8_t DIGIT_MAP[10][15] = {
  {12, 2,3, 4,5, 6,7, 8,9, 10,11, 12,13, 0,0},  // 0
  { 4, 6,7, 8,9, 0,0, 0,0,  0,0,   0,0,  0,0},  // 1
  {10, 0,1, 4,5, 6,7, 10,11, 12,13, 0,0, 0,0},  // 2
  {10, 0,1, 4,5, 6,7, 8,9,  10,11,  0,0, 0,0},  // 3
  { 8, 0,1, 2,3, 6,7, 8,9,   0,0,   0,0, 0,0},  // 4
  {10, 0,1, 2,3, 4,5, 8,9,  10,11,  0,0, 0,0},  // 5
  {12, 0,1, 2,3, 4,5, 8,9,  10,11, 12,13, 0,0}, // 6
  { 6, 4,5, 6,7, 8,9, 0,0,   0,0,   0,0, 0,0},  // 7
  {14, 0,1, 2,3, 4,5, 6,7,   8,9,  10,11,12,13},// 8
  {12, 0,1, 2,3, 4,5, 6,7,   8,9,  10,11, 0,0}, // 9
};

// ── Tabla de letras y símbolos ────────────────────────────────────────────
// Mapeo verificado: G=0,1 F=2,3 A=4,5 B=6,7 C=8,9 D=10,11 E=12,13
static const char CHAR_KEYS[] = " -ABCDEFGHIJLNOPRSTUYZKMQVWX";
static const uint8_t CHAR_MAP[][15] = {
  { 0, 0,0, 0,0, 0,0, 0,0,  0,0,  0,0,  0,0},   // ' ' (espacio)
  { 2, 0,1, 0,0, 0,0, 0,0,  0,0,  0,0,  0,0},   // '-'  G
  {12, 0,1, 2,3, 4,5, 6,7,  8,9, 12,13, 0,0},   // 'A'
  {10, 0,1, 2,3, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'B'
  { 8, 2,3, 4,5, 10,11, 12,13, 0,0, 0,0, 0,0},  // 'C'
  {10, 0,1, 6,7, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'D'
  {10, 0,1, 2,3, 4,5, 10,11, 12,13, 0,0, 0,0},  // 'E'
  { 8, 0,1, 2,3, 4,5, 12,13, 0,0,  0,0, 0,0},   // 'F'
  {10, 2,3, 4,5, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'G'
  {10, 0,1, 2,3, 6,7, 8,9,  12,13, 0,0, 0,0},   // 'H'
  { 4, 6,7, 8,9, 0,0, 0,0,   0,0,  0,0, 0,0},   // 'I'
  { 6, 6,7, 8,9, 10,11, 0,0, 0,0,  0,0, 0,0},   // 'J'
  { 6, 2,3, 10,11, 12,13, 0,0, 0,0, 0,0, 0,0},  // 'L'
  { 6, 0,1, 8,9, 12,13, 0,0, 0,0,  0,0, 0,0},   // 'N'
  {12, 2,3, 4,5, 6,7, 8,9, 10,11, 12,13, 0,0},  // 'O'
  {10, 0,1, 2,3, 4,5, 6,7, 12,13,  0,0, 0,0},   // 'P'
  { 4, 0,1, 12,13, 0,0, 0,0, 0,0,  0,0, 0,0},   // 'R'
  {10, 0,1, 2,3, 4,5, 8,9, 10,11,  0,0, 0,0},   // 'S'
  { 8, 0,1, 2,3, 10,11, 12,13, 0,0, 0,0, 0,0},  // 'T'
  {10, 2,3, 6,7, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'U'
  {10, 0,1, 2,3, 6,7, 8,9, 10,11,  0,0, 0,0},   // 'Y'
  {10, 0,1, 4,5, 6,7, 10,11, 12,13, 0,0, 0,0},  // 'Z'
  // ── Letras anadidas: aproximaciones 7-seg (completan el alfabeto) ──────────
  { 8, 2,3, 0,1, 6,7, 12,13, 0,0, 0,0, 0,0},    // 'K'  F,G,B,E
  {10, 4,5, 6,7, 8,9, 12,13, 2,3, 0,0, 0,0},    // 'M'  A,B,C,E,F (~Pi)
  {10, 4,5, 6,7, 8,9, 2,3, 0,1, 0,0, 0,0},      // 'Q'  A,B,C,F,G (~9)
  { 6, 8,9, 10,11, 12,13, 0,0, 0,0, 0,0, 0,0},  // 'V'  C,D,E (cubeta baja)
  {12, 6,7, 8,9, 10,11, 12,13, 2,3, 0,1, 0,0},  // 'W'  B,C,D,E,F,G
  { 8, 6,7, 8,9, 12,13, 2,3, 0,0, 0,0, 0,0},    // 'X'  B,C,E,F
};

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
  int offset = slotOffset(position);
  uint8_t count = DIGIT_MAP[digit][0];
  for (uint8_t i = 1; i <= count; i++) {
    _pixels.setPixelColor(mapLed(offset, DIGIT_MAP[digit][i]), color);
  }
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
  if (c >= 'a' && c <= 'z') c = c - 32; // a mayúscula

  if (c >= '0' && c <= '9') {
    _renderDigit(position, c - '0', color);
    return;
  }

  int offset = slotOffset(position);
  for (int i = 0; CHAR_KEYS[i] != '\0'; i++) {
    if (CHAR_KEYS[i] == c) {
      uint8_t count = CHAR_MAP[i][0];
      for (uint8_t j = 1; j <= count; j++) {
        _pixels.setPixelColor(mapLed(offset, CHAR_MAP[i][j]), color);
      }
      return;
    }
  }
  // Carácter no encontrado → posición en blanco
}

// Convierte acentos/ñ (UTF-8, lead byte 0xC3) a su letra ASCII sin tilde.
// El display es de 7 seg. y solo tiene glifos ASCII; los multibyte se colapsan
// a un unico caracter para que no ocupen dos posiciones ni salgan como basura.
String DisplayEngine::_normalize(const char* text) {
  String out;
  for (const unsigned char* p = (const unsigned char*)text; *p; p++) {
    if (*p == 0xC3 && *(p + 1)) {
      unsigned char b = *(++p);            // consume el segundo byte
      switch (b) {
        case 0x81: case 0xA1: out += 'A'; break;  // Á á
        case 0x89: case 0xA9: out += 'E'; break;  // É é
        case 0x8D: case 0xAD: out += 'I'; break;  // Í í
        case 0x93: case 0xB3: out += 'O'; break;  // Ó ó
        case 0x9A: case 0xBA: out += 'U'; break;  // Ú ú
        case 0x9C: case 0xBC: out += 'U'; break;  // Ü ü
        case 0x91: case 0xB1: out += 'N'; break;  // Ñ ñ
        default: break;                            // otros: se omiten
      }
    } else {
      out += (char)*p;
    }
  }
  return out;
}

void DisplayEngine::showText(const char* text, uint32_t color) {
  _scrolling = false;
  _pixels.clear();
  uint32_t c = (color != 0) ? color : _baseColor;
  String t = _normalize(text);
  int n = t.length();
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
  _scrollText += _normalize(text);
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
