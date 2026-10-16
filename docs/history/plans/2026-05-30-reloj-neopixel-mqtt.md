# Reloj NeoPixel MQTT — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Firmware ESP8266 que conecta un reloj NeoPixel de 56 LEDs a Home Assistant via MQTT, con WiFiManager para configuración y NTP para la hora.

**Architecture:** DisplayEngine renderiza los 56 LEDs de forma independiente. MQTTManager maneja la conectividad y Discovery. CommandHandler parsea JSON de MQTT y delega al DisplayEngine. El sketch principal une todo en un loop no bloqueante.

**Tech Stack:** Arduino/ESP8266 (NodeMCU ESP-12E), Adafruit NeoPixel, WiFiManager (tzapu), PubSubClient, ArduinoJson 7.x, NTPClient, LittleFS

---

## Librerías a instalar (Arduino IDE → Gestor de Librerías)

Antes de empezar, instalar:
- `Adafruit NeoPixel` (Adafruit) — ya instalada
- `WiFiManager` by tzapu — v2.0.17+
- `PubSubClient` by Nick O'Leary — v2.8+
- `ArduinoJson` by Benoit Blanchon — v7.x
- `NTPClient` by Fabrice Weinberg — v3.2+

---

## Estructura de archivos

```
reloj_neopixel/
├── reloj_neopixel.ino       ← Setup, loop, instancias globales
├── config.h                  ← Constantes: pines, topics, intervalos
├── display_engine.h          ← Declaración clase DisplayEngine
├── display_engine.cpp        ← Renderizado NeoPixel: dígitos, texto, scroll
├── command_handler.h         ← Declaración clase CommandHandler
├── command_handler.cpp       ← Parseo JSON → llama DisplayEngine
├── mqtt_client.h             ← Declaración clase MQTTManager
└── mqtt_client.cpp           ← Conexión MQTT, Discovery, pub/sub
```

---

## Task 1: Scaffold + config.h

**Files:**
- Create: `reloj_neopixel/reloj_neopixel.ino`
- Create: `reloj_neopixel/config.h`

- [ ] **Step 1.1: Crear config.h con todas las constantes**

```cpp
// config.h
#pragma once

// ── NeoPixel ──────────────────────────────────────────
#define NEO_PIN        D2
#define NEO_COUNT      56
#define NEO_BRIGHTNESS 80       // 0-255, arranca al 30%

// ── NTP ───────────────────────────────────────────────
#define NTP_SERVER     "pool.ntp.org"
#define NTP_OFFSET     3600     // UTC+1 (España invierno). Verano: 7200
#define NTP_INTERVAL   3600000  // Resync cada hora (ms)

// ── MQTT Topics ───────────────────────────────────────
#define MQTT_TOPIC_LIGHT_SET    "reloj/light/set"
#define MQTT_TOPIC_LIGHT_STATE  "reloj/light/state"
#define MQTT_TOPIC_CMD          "reloj/cmd"
#define MQTT_TOPIC_STATE        "reloj/state"
#define MQTT_TOPIC_AVAILABLE    "reloj/available"
#define MQTT_DISCOVERY_TOPIC    "homeassistant/light/reloj_neopixel/config"
#define MQTT_CLIENT_ID          "reloj_neopixel"

// ── WiFiManager ───────────────────────────────────────
#define WIFI_AP_NAME   "Reloj-Config"
#define WIFI_AP_PASS   ""       // Portal abierto (sin contraseña)

// ── Timing ────────────────────────────────────────────
#define HEARTBEAT_INTERVAL  30000   // Estado cada 30s (ms)
#define MQTT_RECONNECT_MS   5000    // Reintento MQTT cada 5s (ms)
#define SCROLL_DEFAULT_MS   150     // Velocidad scroll por defecto (ms/paso)
#define CLOCK_UPDATE_MS     60000   // Actualizar hora en modo reloj cada minuto
```

- [ ] **Step 1.2: Crear reloj_neopixel.ino mínimo que compile**

```cpp
// reloj_neopixel.ino
#include "config.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n[RELOJ] Arrancando..."));
}

void loop() {
}
```

- [ ] **Step 1.3: Compilar y verificar que no hay errores**

En Arduino IDE: Sketch → Verify/Compile (Ctrl+R).
Resultado esperado: "Done compiling." sin errores.

- [ ] **Step 1.4: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: project scaffold + config.h"
```

---

## Task 2: DisplayEngine — renderizado de dígitos

**Files:**
- Create: `reloj_neopixel/display_engine.h`
- Create: `reloj_neopixel/display_engine.cpp`
- Modify: `reloj_neopixel/reloj_neopixel.ino`

**Nota sobre el mapa de segmentos (derivado del sketch original — VERIFICADO):**
Cada dígito usa 14 LEDs (posición relativa 0-13) organizados como display de 7 segmentos con 2 LEDs por segmento:
```
  AA         LEDs  4, 5 = segmento A (arriba)
 F  B        LEDs  2, 3 = segmento F (arriba-izquierda)
 F  B        LEDs  6, 7 = segmento B (arriba-derecha)
  GG         LEDs  0, 1 = segmento G (medio)
 E  C        LEDs 12,13 = segmento E (abajo-izquierda)
 E  C        LEDs  8, 9 = segmento C (abajo-derecha)
  DD         LEDs 10,11 = segmento D (abajo)
```
Dígito en posición `pos` (0-3) → offset = `pos * 14`. LED absoluto = `offset + led_relativo`.

- [ ] **Step 2.1: Crear display_engine.h**

```cpp
// display_engine.h
#pragma once
#include <Adafruit_NeoPixel.h>
#include "config.h"

class DisplayEngine {
public:
  DisplayEngine();
  void begin();
  void clear();
  void setBrightness(uint8_t brightness);

  // Renderizado
  void showTime(int hours, int minutes);
  void showText(const char* text, uint32_t color);
  void showNumber(int number, uint32_t color);

  // Scroll
  void startScroll(const char* text, uint32_t color, int speedMs);
  void tickScroll();
  bool isScrolling() const;

  // Color base (usado por showTime cuando no se especifica color en el comando)
  void setBaseColor(uint32_t color);

  // Utilidades
  static uint32_t hexToColor(const char* hex);

private:
  Adafruit_NeoPixel _pixels;
  uint32_t _baseColor = 0xFFFFFF; // blanco por defecto
  void _renderDigit(int position, int digit, uint32_t color);
  void _renderChar(int position, char c, uint32_t color);

  // Estado scroll
  bool     _scrolling      = false;
  String   _scrollText;
  uint32_t _scrollColor    = 0;
  int      _scrollSpeedMs  = SCROLL_DEFAULT_MS;
  int      _scrollPos      = 0;
  unsigned long _lastScrollTick = 0;
};
```

- [ ] **Step 2.2: Crear display_engine.cpp — parte 1: datos y constructor**

```cpp
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
// Recalculadas con mapeo verificado: G=0,1 F=2,3 A=4,5 B=6,7 C=8,9 D=10,11 E=12,13
// Caracteres soportados (mayúsculas; minúsculas se convierten)
// NOTA: letras = extrapolación 7-segmentos; verificar en hardware (Task 3).
static const char CHAR_KEYS[] = " -ABCDEFGHIJLNOPRSTUYZ";
static const uint8_t CHAR_MAP[][15] = {
  { 0, 0,0, 0,0, 0,0, 0,0,  0,0,  0,0,  0,0},   // ' ' (espacio)
  { 2, 0,1, 0,0, 0,0, 0,0,  0,0,  0,0,  0,0},   // '-'  G
  {12, 0,1, 2,3, 4,5, 6,7,  8,9, 12,13, 0,0},   // 'A'  A+B+C+E+F+G
  {10, 0,1, 2,3, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'B'  C+D+E+F+G (b minúscula)
  { 8, 2,3, 4,5, 10,11, 12,13, 0,0, 0,0, 0,0},  // 'C'  A+D+E+F
  {10, 0,1, 6,7, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'D'  B+C+D+E+G (d minúscula)
  {10, 0,1, 2,3, 4,5, 10,11, 12,13, 0,0, 0,0},  // 'E'  A+D+E+F+G
  { 8, 0,1, 2,3, 4,5, 12,13, 0,0,  0,0, 0,0},   // 'F'  A+E+F+G
  {10, 2,3, 4,5, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'G'  A+C+D+E+F
  {10, 0,1, 2,3, 6,7, 8,9,  12,13, 0,0, 0,0},   // 'H'  B+C+E+F+G
  { 4, 6,7, 8,9, 0,0, 0,0,   0,0,  0,0, 0,0},   // 'I'  B+C (= 1)
  { 6, 6,7, 8,9, 10,11, 0,0, 0,0,  0,0, 0,0},   // 'J'  B+C+D
  { 6, 2,3, 10,11, 12,13, 0,0, 0,0, 0,0, 0,0},  // 'L'  D+E+F
  { 6, 0,1, 8,9, 12,13, 0,0, 0,0,  0,0, 0,0},   // 'N'  C+E+G (n minúscula)
  {12, 2,3, 4,5, 6,7, 8,9, 10,11, 12,13, 0,0},  // 'O'  = 0
  {10, 0,1, 2,3, 4,5, 6,7, 12,13,  0,0, 0,0},   // 'P'  A+B+E+F+G
  { 4, 0,1, 12,13, 0,0, 0,0, 0,0,  0,0, 0,0},   // 'R'  E+G (r minúscula)
  {10, 0,1, 2,3, 4,5, 8,9, 10,11,  0,0, 0,0},   // 'S'  = 5
  { 8, 0,1, 2,3, 10,11, 12,13, 0,0, 0,0, 0,0},  // 'T'  D+E+F+G (t minúscula)
  {10, 2,3, 6,7, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'U'  B+C+D+E+F
  {10, 0,1, 2,3, 6,7, 8,9, 10,11,  0,0, 0,0},   // 'Y'  B+C+D+F+G
  {10, 0,1, 4,5, 6,7, 10,11, 12,13, 0,0, 0,0},  // 'Z'  = 2
};

// ── Constructor ───────────────────────────────────────────────────────────
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
```

- [ ] **Step 2.3: Añadir _renderDigit y showTime al mismo archivo**

```cpp
// ── Renderizado interno ───────────────────────────────────────────────────
void DisplayEngine::_renderDigit(int position, int digit, uint32_t color) {
  if (position < 0 || position > 3) return;
  if (digit < 0 || digit > 9) return;
  int offset = position * 14;
  uint8_t count = DIGIT_MAP[digit][0];
  for (uint8_t i = 1; i <= count; i++) {
    _pixels.setPixelColor(offset + DIGIT_MAP[digit][i], color);
  }
}

void DisplayEngine::showTime(int hours, int minutes) {
  _scrolling = false;
  _pixels.clear();
  uint32_t color = _pixels.getPixelColor(0); // color actual
  // Si todos apagados, usar blanco
  if (color == 0) color = _pixels.Color(255, 255, 255);

  _renderDigit(0, hours / 10,   color);
  _renderDigit(1, hours % 10,   color);
  _renderDigit(2, minutes / 10, color);
  _renderDigit(3, minutes % 10, color);
  _pixels.show();
}
```

**Nota:** `showTime` usa el color actual del primer pixel como color de los dígitos. El color lo gestiona `CommandHandler` (via `hexToColor` + `setPixelColor`). En la integración, `CommandHandler` llamará a `_pixels.fill(color)` antes de `showTime`.

Dado que `_pixels` es privado, añadir método público para establecer el color base:

```cpp
// Añadir en display_engine.h, sección public:
void setBaseColor(uint32_t color);
```

```cpp
// Añadir en display_engine.cpp:
void DisplayEngine::setBaseColor(uint32_t color) {
  // Guarda el color base sin mostrar nada;
  // showTime/showText lo usarán al renderizar
  _baseColor = color;
}
```

Y añadir `uint32_t _baseColor = 0xFFFFFF;` en `private` de `display_engine.h`.

Actualizar `showTime` para usar `_baseColor`:

```cpp
void DisplayEngine::showTime(int hours, int minutes) {
  _scrolling = false;
  _pixels.clear();
  uint32_t color = (_baseColor != 0) ? _baseColor : _pixels.Color(255,255,255);
  _renderDigit(0, hours / 10,   color);
  _renderDigit(1, hours % 10,   color);
  _renderDigit(2, minutes / 10, color);
  _renderDigit(3, minutes % 10, color);
  _pixels.show();
}
```

- [ ] **Step 2.4: Añadir hexToColor al mismo archivo**

```cpp
uint32_t DisplayEngine::hexToColor(const char* hex) {
  // Acepta "#RRGGBB" o "RRGGBB"
  if (!hex) return 0xFFFFFF;
  const char* p = (hex[0] == '#') ? hex + 1 : hex;
  long v = strtol(p, nullptr, 16);
  uint8_t r = (v >> 16) & 0xFF;
  uint8_t g = (v >>  8) & 0xFF;
  uint8_t b =  v        & 0xFF;
  // Usar instancia estática temporal para Color()
  return Adafruit_NeoPixel::Color(r, g, b);
}
```

- [ ] **Step 2.5: Modificar reloj_neopixel.ino para probar showTime**

```cpp
// reloj_neopixel.ino
#include "config.h"
#include "display_engine.h"

DisplayEngine display;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("[RELOJ] Arrancando..."));
  display.begin();
  display.setBaseColor(DisplayEngine::hexToColor("#0000FF")); // azul
  display.showTime(12, 34);  // debe mostrar 12:34
  Serial.println(F("[TEST] Mostrando 12:34 en azul"));
}

void loop() {}
```

- [ ] **Step 2.6: Subir al ESP8266 y verificar**

Upload → abrir Serial Monitor (115200 baud).
Salida esperada:
```
[RELOJ] Arrancando...
[DISPLAY] Inicializado
[TEST] Mostrando 12:34 en azul
```
Los LEDs deben mostrar `1234` en azul.

- [ ] **Step 2.7: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: display engine - digit rendering + showTime"
```

---

## Task 3: DisplayEngine — texto, números y caracteres

**Files:**
- Modify: `reloj_neopixel/display_engine.cpp`
- Modify: `reloj_neopixel/reloj_neopixel.ino`

- [ ] **Step 3.1: Añadir _renderChar, showText y showNumber**

Añadir al final de `display_engine.cpp`:

```cpp
// ── Renderizado de caracteres ─────────────────────────────────────────────
void DisplayEngine::_renderChar(int position, char c, uint32_t color) {
  if (position < 0 || position > 3) return;

  // Convertir a mayúscula
  if (c >= 'a' && c <= 'z') c = c - 32;

  // Buscar en dígitos primero (0-9)
  if (c >= '0' && c <= '9') {
    _renderDigit(position, c - '0', color);
    return;
  }

  // Buscar en tabla de letras
  int offset = position * 14;
  for (int i = 0; CHAR_KEYS[i] != '\0'; i++) {
    if (CHAR_KEYS[i] == c) {
      uint8_t count = CHAR_MAP[i][0];
      for (uint8_t j = 1; j <= count; j++) {
        _pixels.setPixelColor(offset + CHAR_MAP[i][j], color);
      }
      return;
    }
  }
  // Carácter no encontrado → posición en blanco (nada)
}

void DisplayEngine::showText(const char* text, uint32_t color) {
  _scrolling = false;
  _pixels.clear();
  uint32_t c = (color != 0) ? color : _baseColor;
  // Mostrar hasta 4 caracteres, alineado a la izquierda
  for (int i = 0; i < 4; i++) {
    if (text[i] == '\0') break;
    _renderChar(i, text[i], c);
  }
  _pixels.show();
}

void DisplayEngine::showNumber(int number, uint32_t color) {
  _scrolling = false;
  _pixels.clear();
  uint32_t c = (color != 0) ? color : _baseColor;

  // Limitar a 4 dígitos (0-9999)
  number = abs(number) % 10000;

  // Extraer dígitos (alineación derecha)
  int digits[4];
  digits[3] = number % 10;
  digits[2] = (number / 10) % 10;
  digits[1] = (number / 100) % 10;
  digits[0] = (number / 1000) % 10;

  // Solo mostrar dígitos significativos (sin ceros a la izquierda)
  bool leading = true;
  for (int i = 0; i < 4; i++) {
    if (leading && digits[i] == 0 && i < 3) continue; // saltar ceros iniciales
    leading = false;
    _renderDigit(i, digits[i], c);
  }
  _pixels.show();
}
```

- [ ] **Step 3.2: Actualizar reloj_neopixel.ino para probar texto y número**

```cpp
// reloj_neopixel.ino
#include "config.h"
#include "display_engine.h"

DisplayEngine display;

void setup() {
  Serial.begin(115200);
  delay(500);
  display.begin();

  // Test 1: texto
  uint32_t rojo = DisplayEngine::hexToColor("#FF0000");
  display.showText("HOLA", rojo);
  Serial.println(F("[TEST] HOLA en rojo"));
  delay(3000);

  // Test 2: número
  uint32_t verde = DisplayEngine::hexToColor("#00FF00");
  display.showNumber(42, verde);
  Serial.println(F("[TEST] 42 en verde"));
  delay(3000);

  // Test 3: hora
  uint32_t azul = DisplayEngine::hexToColor("#0000FF");
  display.setBaseColor(azul);
  display.showTime(9, 5);
  Serial.println(F("[TEST] 09:05 en azul"));
}

void loop() {}
```

- [ ] **Step 3.3: Subir y verificar**

Salida esperada en Serial:
```
[DISPLAY] Inicializado
[TEST] HOLA en rojo
[TEST] 42 en verde
[TEST] 09:05 en azul
```
LEDs deben mostrar: HOLA rojo (3s) → 42 verde (3s) → 0905 azul.

- [ ] **Step 3.4: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: display engine - text and number rendering"
```

---

## Task 4: DisplayEngine — scroll

**Files:**
- Modify: `reloj_neopixel/display_engine.cpp`
- Modify: `reloj_neopixel/reloj_neopixel.ino`

El scroll funciona a nivel de caracter: el texto se desplaza un caracter por tick, mostrando siempre 4 posiciones de una ventana deslizante. Se añaden espacios al inicio y al final para que el texto entre y salga limpiamente.

- [ ] **Step 4.1: Añadir startScroll, tickScroll, isScrolling**

Añadir al final de `display_engine.cpp`:

```cpp
// ── Scroll ────────────────────────────────────────────────────────────────
void DisplayEngine::startScroll(const char* text, uint32_t color, int speedMs) {
  // Formato: "    TEXTO    " (4 espacios al inicio y al final)
  _scrollText = "    ";
  _scrollText += text;
  _scrollText += "    ";
  _scrollColor  = (color != 0) ? color : _baseColor;
  _scrollSpeedMs = (speedMs > 0) ? speedMs : SCROLL_DEFAULT_MS;
  _scrollPos    = 0;
  _scrolling    = true;
  _lastScrollTick = millis();
  Serial.printf("[DISPLAY] Scroll: '%s' (%dms)\n", text, _scrollSpeedMs);
}

void DisplayEngine::tickScroll() {
  if (!_scrolling) return;
  if (millis() - _lastScrollTick < (unsigned long)_scrollSpeedMs) return;
  _lastScrollTick = millis();

  // Mostrar 4 caracteres desde _scrollPos
  _pixels.clear();
  for (int i = 0; i < 4; i++) {
    int idx = _scrollPos + i;
    if (idx < (int)_scrollText.length()) {
      _renderChar(i, _scrollText[idx], _scrollColor);
    }
  }
  _pixels.show();

  _scrollPos++;

  // Scroll terminado cuando no quedan más caracteres
  if (_scrollPos >= (int)_scrollText.length()) {
    _scrolling = false;
    _pixels.clear();
    _pixels.show();
    Serial.println(F("[DISPLAY] Scroll terminado"));
  }
}

bool DisplayEngine::isScrolling() const {
  return _scrolling;
}
```

- [ ] **Step 4.2: Actualizar reloj_neopixel.ino para probar scroll**

```cpp
// reloj_neopixel.ino
#include "config.h"
#include "display_engine.h"

DisplayEngine display;

void setup() {
  Serial.begin(115200);
  delay(500);
  display.begin();

  uint32_t amarillo = DisplayEngine::hexToColor("#FFFF00");
  display.startScroll("HOLA MUNDO", amarillo, 200);
  Serial.println(F("[TEST] Scroll 'HOLA MUNDO' amarillo a 200ms"));
}

void loop() {
  display.tickScroll();

  if (!display.isScrolling()) {
    Serial.println(F("[TEST] Scroll terminado, apagando"));
    while (true) {}  // parar
  }
}
```

- [ ] **Step 4.3: Subir y verificar**

Salida esperada:
```
[DISPLAY] Inicializado
[TEST] Scroll 'HOLA MUNDO' amarillo a 200ms
[DISPLAY] Scroll: 'HOLA MUNDO' (200ms)
[DISPLAY] Scroll terminado
[TEST] Scroll terminado, apagando
```
Los LEDs deben desplazar el texto "HOLA MUNDO" de izquierda a derecha.

- [ ] **Step 4.4: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: display engine - scroll animation"
```

---

## Task 5: WiFiManager + NTP

**Files:**
- Modify: `reloj_neopixel/reloj_neopixel.ino`

- [ ] **Step 5.1: Actualizar reloj_neopixel.ino con WiFiManager y NTP**

```cpp
// reloj_neopixel.ino
#include "config.h"
#include "display_engine.h"

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

DisplayEngine display;

// ── NTP ───────────────────────────────────────────────────────────────────
WiFiUDP    ntpUDP;
NTPClient  timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_INTERVAL);

// ── MQTT config (guardado en LittleFS) ────────────────────────────────────
char mqttServer[40] = "192.168.1.1";
char mqttPort[6]    = "1883";

void saveMQTTConfig() {
  File f = LittleFS.open("/mqtt.json", "w");
  if (!f) { Serial.println(F("[WiFi] Error guardando config MQTT")); return; }
  JsonDocument doc;
  doc["server"] = mqttServer;
  doc["port"]   = mqttPort;
  serializeJson(doc, f);
  f.close();
  Serial.println(F("[WiFi] Config MQTT guardada"));
}

void loadMQTTConfig() {
  if (!LittleFS.begin()) { Serial.println(F("[WiFi] LittleFS error")); return; }
  if (!LittleFS.exists("/mqtt.json")) return;
  File f = LittleFS.open("/mqtt.json", "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    strlcpy(mqttServer, doc["server"] | "192.168.1.1", sizeof(mqttServer));
    strlcpy(mqttPort,   doc["port"]   | "1883",        sizeof(mqttPort));
    Serial.printf("[WiFi] Config MQTT cargada: %s:%s\n", mqttServer, mqttPort);
  }
  f.close();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n[RELOJ] Arrancando..."));

  LittleFS.begin();
  loadMQTTConfig();
  display.begin();

  // ── WiFiManager ──────────────────────────────────────────────────────────
  WiFiManager wm;

  // Parámetros extra: IP y puerto del broker MQTT
  WiFiManagerParameter p_server("server", "MQTT Server IP", mqttServer, 40);
  WiFiManagerParameter p_port  ("port",   "MQTT Port",      mqttPort,   6);
  wm.addParameter(&p_server);
  wm.addParameter(&p_port);

  // Callback: guardar parámetros cuando se configura por primera vez
  wm.setSaveParamsCallback([&]() {
    strlcpy(mqttServer, p_server.getValue(), sizeof(mqttServer));
    strlcpy(mqttPort,   p_port.getValue(),   sizeof(mqttPort));
    saveMQTTConfig();
  });

  Serial.println(F("[WiFi] Conectando..."));
  if (!wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASS)) {
    Serial.println(F("[WiFi] Fallo al conectar, reiniciando..."));
    ESP.restart();
  }

  Serial.printf("[WiFi] Conectado. IP: %s\n", WiFi.localIP().toString().c_str());

  // ── NTP ──────────────────────────────────────────────────────────────────
  timeClient.begin();
  timeClient.update();
  Serial.printf("[NTP] Hora: %02d:%02d:%02d\n",
    timeClient.getHours(),
    timeClient.getMinutes(),
    timeClient.getSeconds());
}

void loop() {
  timeClient.update();
  display.tickScroll();
}
```

- [ ] **Step 5.2: Subir y probar**

Primera ejecución (sin WiFi guardado):
1. El ESP crea un AP llamado `"Reloj-Config"`
2. Conectar el móvil a ese AP
3. Ir a `192.168.4.1` en el navegador del móvil
4. Introducir SSID, contraseña WiFi, IP del broker MQTT y puerto (1883)
5. Guardar → el ESP reinicia y se conecta

Salida esperada en Serial:
```
[RELOJ] Arrancando...
[DISPLAY] Inicializado
[WiFi] Conectando...
[WiFi] Conectado. IP: 192.168.x.x
[NTP] Hora: HH:MM:SS
```

- [ ] **Step 5.3: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: WiFiManager + NTP time sync"
```

---

## Task 6: MQTT client + Discovery

**Files:**
- Create: `reloj_neopixel/mqtt_client.h`
- Create: `reloj_neopixel/mqtt_client.cpp`
- Modify: `reloj_neopixel/reloj_neopixel.ino`

- [ ] **Step 6.1: Crear mqtt_client.h**

```cpp
// mqtt_client.h
#pragma once
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include "config.h"

// Tipo para el callback de mensajes recibidos
typedef void (*MQTTMessageCallback)(const char* topic, const char* payload);

class MQTTManager {
public:
  MQTTManager();
  void begin(const char* server, int port, MQTTMessageCallback callback);
  void loop();
  bool isConnected() const;
  void publish(const char* topic, const char* payload, bool retain = false);
  void publishDiscovery();

private:
  WiFiClient    _wifiClient;
  PubSubClient  _client;
  const char*   _server;
  int           _port;
  MQTTMessageCallback _userCallback;
  unsigned long _lastReconnectAttempt = 0;

  bool _connect();
  static void _mqttCallback(char* topic, byte* payload, unsigned int length);
  static MQTTManager* _instance; // singleton para el callback estático
};
```

- [ ] **Step 6.2: Crear mqtt_client.cpp**

```cpp
// mqtt_client.cpp
#include "mqtt_client.h"
#include <ArduinoJson.h>

MQTTManager* MQTTManager::_instance = nullptr;

MQTTManager::MQTTManager() : _client(_wifiClient) {
  _instance = this;
}

void MQTTManager::begin(const char* server, int port, MQTTMessageCallback callback) {
  _server = server;
  _port = port;
  _userCallback = callback;
  _client.setServer(server, port);
  _client.setCallback(_mqttCallback);
  _client.setBufferSize(512);
  Serial.printf("[MQTT] Configurado: %s:%d\n", server, port);
}

void MQTTManager::loop() {
  if (!_client.connected()) {
    unsigned long now = millis();
    if (now - _lastReconnectAttempt > MQTT_RECONNECT_MS) {
      _lastReconnectAttempt = now;
      _connect();
    }
  } else {
    _client.loop();
  }
}

bool MQTTManager::isConnected() const {
  return _client.connected();
}

void MQTTManager::publish(const char* topic, const char* payload, bool retain) {
  if (_client.connected()) {
    _client.publish(topic, payload, retain);
  }
}

bool MQTTManager::_connect() {
  Serial.print(F("[MQTT] Conectando..."));

  // Last Will: publica "offline" en reloj/available si se cae
  bool ok = _client.connect(
    MQTT_CLIENT_ID,
    nullptr, nullptr,         // sin usuario/contraseña
    MQTT_TOPIC_AVAILABLE,     // LWT topic
    0, true,                  // LWT QoS=0, retain=true
    "offline"                 // LWT payload
  );

  if (ok) {
    Serial.println(F(" OK"));
    _client.subscribe(MQTT_TOPIC_LIGHT_SET);
    _client.subscribe(MQTT_TOPIC_CMD);
    publish(MQTT_TOPIC_AVAILABLE, "online", true);
    publishDiscovery();
    Serial.println(F("[MQTT] Suscrito a topics"));
  } else {
    Serial.printf(" FALLO (rc=%d)\n", _client.state());
  }
  return ok;
}

void MQTTManager::publishDiscovery() {
  // Payload MQTT Discovery para entidad Light en HA
  StaticJsonDocument<600> doc;
  doc["name"]           = "Reloj NeoPixel";
  doc["unique_id"]      = "reloj_neopixel_001";
  doc["schema"]         = "json";
  doc["state_topic"]    = MQTT_TOPIC_LIGHT_STATE;
  doc["command_topic"]  = MQTT_TOPIC_LIGHT_SET;
  doc["brightness"]     = true;
  doc["color_mode"]     = true;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;

  JsonArray modes = doc.createNestedArray("supported_color_modes");
  modes.add("rgb");

  JsonObject device = doc.createNestedObject("device");
  device["name"]         = "Reloj NeoPixel";
  device["model"]        = "ESP8266 NodeMCU";
  device["manufacturer"] = "DIY";
  JsonArray ids = device.createNestedArray("identifiers");
  ids.add("reloj_neopixel");

  char buf[600];
  serializeJson(doc, buf, sizeof(buf));
  _client.publish(MQTT_DISCOVERY_TOPIC, buf, true); // retain=true
  Serial.println(F("[MQTT] Discovery publicado"));
}

void MQTTManager::_mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!_instance || !_instance->_userCallback) return;
  char buf[256] = {0};
  if (length < sizeof(buf)) {
    memcpy(buf, payload, length);
  }
  _instance->_userCallback(topic, buf);
}
```

- [ ] **Step 6.3: Actualizar reloj_neopixel.ino para incluir MQTT**

Añadir al inicio:
```cpp
#include "mqtt_client.h"
```

Añadir instancia global:
```cpp
MQTTManager mqtt;
```

Añadir callback de prueba y llamar a `mqtt.begin()` en `setup()`, después de la conexión WiFi:

```cpp
void onMQTTMessage(const char* topic, const char* payload) {
  Serial.printf("[MQTT] Recibido en '%s': %s\n", topic, payload);
}
```

En `setup()`, después de `timeClient.update()`:
```cpp
  int port = atoi(mqttPort);
  mqtt.begin(mqttServer, port, onMQTTMessage);
```

En `loop()`:
```cpp
void loop() {
  mqtt.loop();
  timeClient.update();
  display.tickScroll();
}
```

- [ ] **Step 6.4: Subir y verificar en HA**

Salida esperada en Serial al arrancar:
```
[WiFi] Conectado. IP: 192.168.x.x
[NTP] Hora: HH:MM:SS
[MQTT] Configurado: 192.168.x.x:1883
[MQTT] Conectando... OK
[MQTT] Suscrito a topics
[MQTT] Discovery publicado
```

En Home Assistant → Ajustes → Dispositivos e integraciones → MQTT → Dispositivos:
Debe aparecer **"Reloj NeoPixel"** con una entidad Light.

- [ ] **Step 6.5: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: MQTT client + HA Discovery auto-register"
```

---

## Task 7: CommandHandler — parseo JSON + control de luz

**Files:**
- Create: `reloj_neopixel/command_handler.h`
- Create: `reloj_neopixel/command_handler.cpp`
- Modify: `reloj_neopixel/reloj_neopixel.ino`

- [ ] **Step 7.1: Crear command_handler.h**

```cpp
// command_handler.h
#pragma once
#include <Arduino.h>
#include "display_engine.h"
#include "mqtt_client.h"

class CommandHandler {
public:
  CommandHandler(DisplayEngine* display, MQTTManager* mqtt);

  // Llamar desde el callback MQTT
  void handle(const char* topic, const char* payload);

  // Estado actual (para heartbeat y feedback)
  void publishState();
  void publishLightState();

private:
  DisplayEngine* _display;
  MQTTManager*   _mqtt;

  // Estado
  bool    _powered    = false;
  String  _mode       = "apagado";
  String  _value      = "";
  String  _color      = "#FFFFFF";
  uint8_t _brightness = NEO_BRIGHTNESS;

  void _handleLightSet(const char* payload);
  void _handleCmd(const char* payload);
};
```

- [ ] **Step 7.2: Crear command_handler.cpp**

```cpp
// command_handler.cpp
#include "command_handler.h"
#include <ArduinoJson.h>

CommandHandler::CommandHandler(DisplayEngine* display, MQTTManager* mqtt)
  : _display(display), _mqtt(mqtt) {}

void CommandHandler::handle(const char* topic, const char* payload) {
  if (strcmp(topic, MQTT_TOPIC_LIGHT_SET) == 0) {
    _handleLightSet(payload);
  } else if (strcmp(topic, MQTT_TOPIC_CMD) == 0) {
    _handleCmd(payload);
  }
}

void CommandHandler::_handleLightSet(const char* payload) {
  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) return;

  // ON / OFF
  if (doc.containsKey("state")) {
    String state = doc["state"].as<String>();
    if (state == "OFF") {
      _powered = false;
      _mode = "apagado";
      _display->clear();
      publishLightState();
      publishState();
      return;
    }
    _powered = true;
  }

  // Brillo
  if (doc.containsKey("brightness")) {
    _brightness = doc["brightness"].as<uint8_t>();
    _display->setBrightness(_brightness);
  }

  // Color RGB → hex
  if (doc.containsKey("color")) {
    uint8_t r = doc["color"]["r"] | 255;
    uint8_t g = doc["color"]["g"] | 255;
    uint8_t b = doc["color"]["b"] | 255;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    _color = hex;
    _display->setBaseColor(Adafruit_NeoPixel::Color(r, g, b));
  }

  publishLightState();
  publishState();
}

void CommandHandler::_handleCmd(const char* payload) {
  StaticJsonDocument<300> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println(F("[CMD] JSON inválido"));
    return;
  }

  String tipo = doc["tipo"] | "";
  Serial.printf("[CMD] Tipo: %s\n", tipo.c_str());

  // Color del comando (sobreescribe color de la entidad Light temporalmente)
  uint32_t color = 0;
  if (doc.containsKey("color")) {
    color = DisplayEngine::hexToColor(doc["color"].as<const char*>());
    _color = doc["color"].as<String>();
  }

  if (tipo == "apagar") {
    _powered = false;
    _mode = "apagado";
    _value = "";
    _display->clear();

  } else if (tipo == "hora") {
    _powered = true;
    _mode = "hora";
    _value = "";
    // Actualizar inmediatamente (no esperar al tick de 60s en loop)
    // Necesita acceso a NTP — lo resolvemos con un puntero a NTPClient
    // Ver Task 8: updateClock() se llamará desde setup() y desde loop()

  } else if (tipo == "texto") {
    String val = doc["valor"] | "";
    _powered = true;
    _mode = "texto";
    _value = val;
    _display->showText(val.c_str(), color);

  } else if (tipo == "numero") {
    String val = doc["valor"] | "0";
    _powered = true;
    _mode = "numero";
    _value = val;
    _display->showNumber(val.toInt(), color);

  } else if (tipo == "scroll") {
    String val = doc["valor"] | "";
    int vel = doc["velocidad"] | SCROLL_DEFAULT_MS;
    _powered = true;
    _mode = "scroll";
    _value = val;
    _display->startScroll(val.c_str(), color, vel);

  } else {
    Serial.printf("[CMD] Tipo desconocido: %s\n", tipo.c_str());
    return;
  }

  publishState();
  publishLightState();
}

void CommandHandler::publishLightState() {
  StaticJsonDocument<150> doc;
  doc["state"] = _powered ? "ON" : "OFF";

  if (_powered) {
    doc["brightness"] = _brightness;
    // Convertir _color #RRGGBB → {r,g,b}
    long v = strtol(_color.c_str() + 1, nullptr, 16);
    JsonObject c = doc.createNestedObject("color");
    c["r"] = (v >> 16) & 0xFF;
    c["g"] = (v >>  8) & 0xFF;
    c["b"] =  v        & 0xFF;
    doc["color_mode"] = "rgb";
  }

  char buf[150];
  serializeJson(doc, buf, sizeof(buf));
  _mqtt->publish(MQTT_TOPIC_LIGHT_STATE, buf);
}

void CommandHandler::publishState() {
  StaticJsonDocument<200> doc;
  doc["encendido"] = _powered;
  doc["modo"]      = _mode;
  doc["valor"]     = _value;
  doc["color"]     = _color;
  doc["brillo"]    = _brightness;
  doc["ip"]        = WiFi.localIP().toString();
  doc["rssi"]      = WiFi.RSSI();

  char buf[200];
  serializeJson(doc, buf, sizeof(buf));
  _mqtt->publish(MQTT_TOPIC_STATE, buf);
}
```

- [ ] **Step 7.3: Actualizar reloj_neopixel.ino**

Añadir al inicio:
```cpp
#include "command_handler.h"
```

Añadir instancias globales:
```cpp
MQTTManager    mqtt;
CommandHandler cmdHandler(&display, &mqtt);
```

Actualizar el callback MQTT:
```cpp
void onMQTTMessage(const char* topic, const char* payload) {
  Serial.printf("[MQTT] '%s': %s\n", topic, payload);
  cmdHandler.handle(topic, payload);
}
```

En `setup()`, después de `mqtt.begin(...)`:
```cpp
  // Publicar estado inicial (apagado)
  cmdHandler.publishState();
  cmdHandler.publishLightState();
```

- [ ] **Step 7.4: Subir y probar desde HA**

En HA → Developer Tools → MQTT, publicar en `reloj/cmd`:
```json
{"tipo": "texto", "valor": "HOLA", "color": "#FF0000"}
```
Los LEDs deben mostrar `HOLA` en rojo.

Publicar en `reloj/light/set`:
```json
{"state": "OFF"}
```
Los LEDs deben apagarse.

Publicar en `reloj/light/set`:
```json
{"state": "ON", "brightness": 150, "color": {"r": 0, "g": 0, "b": 255}}
```
Los LEDs deben encenderse con el último contenido en azul.

- [ ] **Step 7.5: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: command handler - JSON parsing + light control"
```

---

## Task 8: Modo reloj — NTP autónomo

**Files:**
- Modify: `reloj_neopixel/reloj_neopixel.ino`
- Modify: `reloj_neopixel/command_handler.h`
- Modify: `reloj_neopixel/command_handler.cpp`

El modo reloj necesita que el loop principal actualice los dígitos cada minuto. CommandHandler expone `isClockMode()` y `updateClock(hours, minutes)` para que el sketch principal lo llame.

- [ ] **Step 8.1: Añadir isClockMode y updateClock a CommandHandler**

En `command_handler.h`, añadir en sección `public`:
```cpp
  bool isClockMode() const;
  void updateClock(int hours, int minutes);
```

En `command_handler.cpp`, añadir:
```cpp
bool CommandHandler::isClockMode() const {
  return _powered && (_mode == "hora");
}

void CommandHandler::updateClock(int hours, int minutes) {
  if (!isClockMode()) return;
  char val[6];
  snprintf(val, sizeof(val), "%02d%02d", hours, minutes);
  _value = val;
  _display->showTime(hours, minutes);
}
```

- [ ] **Step 8.2: Actualizar loop() en reloj_neopixel.ino para el modo reloj**

```cpp
// Variables globales (añadir junto a las instancias)
unsigned long lastClockUpdate  = 0;
unsigned long lastHeartbeat    = 0;

void loop() {
  mqtt.loop();
  timeClient.update();
  display.tickScroll();

  // ── Modo reloj: actualizar inmediatamente al entrar en modo, luego cada minuto
  if (cmdHandler.isClockMode()) {
    unsigned long now = millis();
    if (lastClockUpdate == 0 || now - lastClockUpdate >= CLOCK_UPDATE_MS) {
      lastClockUpdate = now;
      cmdHandler.updateClock(timeClient.getHours(), timeClient.getMinutes());
    }
  } else {
    lastClockUpdate = 0; // reset para que la próxima vez en modo reloj actualice al instante
  }

  // ── Heartbeat: publicar estado cada 30s ──────────────────────────────────
  if (mqtt.isConnected()) {
    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      lastHeartbeat = now;
      cmdHandler.publishState();
    }
  }
}
```

- [ ] **Step 8.3: Subir y probar modo reloj**

Publicar en `reloj/cmd`:
```json
{"tipo": "hora"}
```
Los LEDs deben mostrar la hora actual (HH:MM) y actualizarse cada minuto.

Verificar en Serial que cada minuto aparece:
```
[CMD] Tipo: hora
```
(Al recibir el comando inicial) y luego los dígitos se actualizan silenciosamente.

- [ ] **Step 8.4: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: autonomous clock mode with NTP"
```

---

## Task 9: Estado offline (WiFi perdido) + LEDs apagados

**Files:**
- Modify: `reloj_neopixel/reloj_neopixel.ino`

Cuando el ESP pierde WiFi, los LEDs deben apagarse. Ya gestionamos el caso MQTT (Last Will publica "offline"), pero los LEDs siguen encendidos si WiFi cae. Añadir chequeo de WiFi en el loop.

- [ ] **Step 9.1: Añadir chequeo de WiFi en loop()**

Añadir variable de estado:
```cpp
bool wifiWasConnected = false;
```

Añadir al inicio de `loop()`:
```cpp
  // ── Detectar pérdida de WiFi → apagar LEDs ───────────────────────────────
  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  if (wifiWasConnected && !wifiNow) {
    Serial.println(F("[WiFi] Conexión perdida — apagando LEDs"));
    display.clear();
  }
  if (!wifiWasConnected && wifiNow) {
    Serial.println(F("[WiFi] Reconectado"));
  }
  wifiWasConnected = wifiNow;
```

- [ ] **Step 9.2: Verificar comportamiento**

Desconectar el router WiFi por unos segundos:
- Los LEDs deben apagarse
- Serial debe mostrar `[WiFi] Conexión perdida — apagando LEDs`

Al reconectar:
- Serial muestra `[WiFi] Reconectado` y `[MQTT] Conectando... OK`
- HA recibe el estado "online" de nuevo
- Enviar `{"tipo": "hora"}` desde HA para retomar la visualización

- [ ] **Step 9.3: Commit**

```bash
git add reloj_neopixel/
git commit -m "feat: turn off LEDs on WiFi disconnect"
```

---

## Task 10: Test de integración final

**Files:** Solo verificación, sin cambios de código.

- [ ] **Step 10.1: Probar todos los comandos desde HA Developer Tools**

Ir a HA → Developer Tools → MQTT → Publish.

**Prueba 1 — Modo reloj:**
Topic: `reloj/cmd`, Payload: `{"tipo": "hora"}`
→ Debe mostrar hora actual y actualizarse cada minuto.

**Prueba 2 — Texto:**
Topic: `reloj/cmd`, Payload: `{"tipo": "texto", "valor": "HOLA", "color": "#FF0000"}`
→ Debe mostrar HOLA en rojo.

**Prueba 3 — Número:**
Topic: `reloj/cmd`, Payload: `{"tipo": "numero", "valor": "42", "color": "#00FF00"}`
→ Debe mostrar 42 en verde.

**Prueba 4 — Scroll:**
Topic: `reloj/cmd`, Payload: `{"tipo": "scroll", "valor": "Bienvenido a casa", "color": "#0000FF", "velocidad": 150}`
→ Debe desplazar el texto en azul. Al terminar, LEDs apagados.

**Prueba 5 — Apagar:**
Topic: `reloj/light/set`, Payload: `{"state": "OFF"}`
→ LEDs apagados. Entidad Light en HA muestra estado OFF.

**Prueba 6 — Encender con color:**
Topic: `reloj/light/set`, Payload: `{"state": "ON", "brightness": 200, "color": {"r": 255, "g": 128, "b": 0}}`
→ LEDs encendidos en naranja con el último contenido.

**Prueba 7 — Verificar estado en `reloj/state`:**
Ir a Developer Tools → MQTT → Listen → topic `reloj/state`.
Debe recibir el JSON de estado cada 30s y tras cada comando.

- [ ] **Step 10.2: Verificar entidad en HA UI**

Ir a HA → Ajustes → Dispositivos e integraciones → MQTT.
El dispositivo **"Reloj NeoPixel"** debe:
- Mostrar estado ON/OFF correcto
- Tener selector de color (color picker)
- Tener slider de brillo
- Aparecer como "disponible" (verde)

- [ ] **Step 10.3: Commit final**

```bash
git add reloj_neopixel/
git commit -m "feat: complete ESP8266 NeoPixel clock with MQTT + HA integration"
```

---

## Referencia rápida de comandos MQTT

| Topic | Payload | Resultado |
|---|---|---|
| `reloj/cmd` | `{"tipo":"hora"}` | Modo reloj NTP |
| `reloj/cmd` | `{"tipo":"texto","valor":"HOLA","color":"#FF0000"}` | Texto en rojo |
| `reloj/cmd` | `{"tipo":"numero","valor":"42","color":"#00FF00"}` | Número en verde |
| `reloj/cmd` | `{"tipo":"scroll","valor":"Mensaje","color":"#0000FF","velocidad":150}` | Scroll en azul |
| `reloj/cmd` | `{"tipo":"apagar"}` | LEDs off |
| `reloj/light/set` | `{"state":"OFF"}` | Apagar (entidad Light) |
| `reloj/light/set` | `{"state":"ON","brightness":200,"color":{"r":255,"g":255,"b":255}}` | Encender blanco |
