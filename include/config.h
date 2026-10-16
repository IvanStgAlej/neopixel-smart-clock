// config.h
#pragma once

// ── NeoPixel ──────────────────────────────────────────
#define NEO_PIN        D2
#define NEO_COUNT      56
// Max bytes of display text after normalization (255 chars = Home Assistant text max, + NUL)
#define MAX_TEXT_BYTES 256
#define NEO_BRIGHTNESS 80       // 0-255, arranca al ~30%

// Orientación física del panel completo.
// FLIP_DIGITS: gira cada dígito 180° (segmentos boca abajo). 1 = girar.
// REVERSE_DIGITS: invierte el orden de los 4 dígitos (0↔3, 1↔2).
// Reloj rotado 180° (cable de esquina inf-izq a sup-der): FLIP=0 + REVERSE=1.
//   (la rotación física cancela el giro por dígito que antes hacía FLIP=1)
#define FLIP_DIGITS    0
#define REVERSE_DIGITS 1

// ── NTP ───────────────────────────────────────────────
#define NTP_SERVER     "pool.ntp.org"
#define NTP_OFFSET     3600     // Offset inicial (invierno); se ajusta en runtime via switch DST
#define NTP_INTERVAL   3600000  // Resync cada hora (ms)

// Offsets horarios (España): invierno UTC+1, verano UTC+2
#define NTP_OFFSET_WINTER  3600
#define NTP_OFFSET_SUMMER  7200

// ── MQTT Topics ───────────────────────────────────────
#define MQTT_TOPIC_LIGHT_SET    "reloj/light/set"
#define MQTT_TOPIC_LIGHT_STATE  "reloj/light/state"
#define MQTT_TOPIC_CMD          "reloj/cmd"
#define MQTT_TOPIC_STATE        "reloj/state"
#define MQTT_TOPIC_AVAILABLE    "reloj/available"
#define MQTT_TOPIC_DST_SET      "reloj/dst/set"
#define MQTT_TOPIC_DST_STATE    "reloj/dst/state"
#define MQTT_DISCOVERY_TOPIC    "homeassistant/light/reloj_neopixel/config"
#define MQTT_DST_DISCOVERY_TOPIC "homeassistant/switch/reloj_neopixel_dst/config"
#define MQTT_DISC_BTN_HORA      "homeassistant/button/reloj_neopixel_hora/config"
#define MQTT_DISC_TXT_FIJO      "homeassistant/text/reloj_neopixel_texto/config"
#define MQTT_DISC_TXT_SCROLL    "homeassistant/text/reloj_neopixel_scroll/config"
#define MQTT_DISC_NUM           "homeassistant/number/reloj_neopixel_numero/config"
#define MQTT_CLIENT_ID          "reloj_neopixel"

// Credenciales del broker (login dedicado creado en el add-on Mosquitto)
// Credentials live in secrets.h (git-ignored); see secrets.example.h
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found - building with placeholder credentials (secrets.example.h)"
#include "secrets.example.h"
#endif

// ── WiFiManager ───────────────────────────────────────
#define WIFI_AP_NAME   "Reloj-Config"
#define WIFI_AP_PASS   ""       // Portal abierto (sin contraseña)

// ── OTA (actualizacion inalambrica) ───────────────────
#define OTA_HOSTNAME   "reloj-neopixel"   // aparece como puerto de red en el Arduino IDE

// ── Timing ────────────────────────────────────────────
#define HEARTBEAT_INTERVAL  30000   // Estado cada 30s (ms)
#define MQTT_RECONNECT_MS   5000    // Reintento MQTT cada 5s (ms)
#define SCROLL_DEFAULT_MS   300     // Velocidad scroll por defecto (ms/paso; mas alto = mas lento)
#define CLOCK_UPDATE_MS     60000   // Actualizar hora en modo reloj cada minuto

// ── Temporizador ──────────────────────────────────────
#define TIMER_MAX_SECONDS   359940  // Tope: 99h59m (formato HH:MM a partir de 100 min)
#define TIMER_ALARM_MS      10000   // Duracion del parpadeo al terminar (ms)
#define TIMER_BLINK_MS      500     // Periodo de cada fase del parpadeo (ms)
#define TIMER_ALARM_COLOR   "#FF0000"
