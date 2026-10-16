// config.h
#pragma once

// Firmware version: normally injected by scripts/version.py from git describe.
#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

// ── NeoPixel ──────────────────────────────────────────
#define NEO_PIN        D2
#define NEO_COUNT      56
// Max bytes of display text after normalization (255 chars = Home Assistant text max, + NUL)
#define MAX_TEXT_BYTES 256
#define NEO_BRIGHTNESS 80       // 0-255, starts at ~30%

// Physical orientation of the whole panel.
// FLIP_DIGITS: rotates each digit 180° (upside-down segments). 1 = rotate.
// REVERSE_DIGITS: reverses the order of the 4 digits (0↔3, 1↔2).
// Clock rotated 180° (cable from bottom-left corner to top-right): FLIP=0 + REVERSE=1.
//   (the physical rotation cancels the per-digit rotation FLIP=1 used to apply)
#define FLIP_DIGITS    0
#define REVERSE_DIGITS 1

// ── NTP ───────────────────────────────────────────────
#define NTP_SERVER     "pool.ntp.org"
#define NTP_OFFSET     3600     // Initial offset (winter); adjusted at runtime via the DST switch
#define NTP_INTERVAL   3600000  // Resync every hour (ms)

// Time zone offsets (Spain): winter UTC+1, summer UTC+2
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

// Broker credentials (dedicated login created in the Mosquitto add-on)
// Credentials live in secrets.h (git-ignored); see secrets.example.h
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h not found - building with placeholder credentials (secrets.example.h)"
#include "secrets.example.h"
#endif

// ── WiFiManager ───────────────────────────────────────
#define WIFI_AP_NAME   "Reloj-Config"
#define WIFI_AP_PASS   ""       // Open portal (no password)

// ── OTA (over-the-air updates) ────────────────────────
#define OTA_HOSTNAME   "reloj-neopixel"   // shows up as a network port in the Arduino IDE

// ── Timing ────────────────────────────────────────────
#define HEARTBEAT_INTERVAL  30000   // State every 30 s (ms)
#define MQTT_RECONNECT_MS   5000    // MQTT retry every 5 s (ms)
#define SCROLL_DEFAULT_MS   300     // Default scroll speed (ms/step; higher = slower)
#define CLOCK_UPDATE_MS     60000   // Update the time in clock mode every minute

// ── Timer ─────────────────────────────────────────────
#define TIMER_MAX_SECONDS   359940  // Cap: 99h59m (HH:MM format from 100 min)
#define TIMER_ALARM_MS      10000   // Blink duration when finished (ms)
#define TIMER_BLINK_MS      500     // Length of each blink phase (ms)
#define TIMER_ALARM_COLOR   "#FF0000"
