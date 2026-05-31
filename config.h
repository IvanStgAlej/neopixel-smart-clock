// config.h
#pragma once

// ── NeoPixel ──────────────────────────────────────────
#define NEO_PIN        D2
#define NEO_COUNT      56
#define NEO_BRIGHTNESS 80       // 0-255, arranca al ~30%

// Orientación física: si cada dígito se ve girado 180° (boca abajo),
// pon esto a 1 para invertir el mapeo de LEDs por dígito. 0 = sin rotar.
#define FLIP_DIGITS    1

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
