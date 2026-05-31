// reloj_neopixel.ino — Reloj NeoPixel + Home Assistant (MQTT)
// Etapa actual: WiFiManager (portal de configuracion) + NTP (hora por WiFi).
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

// ── Config MQTT (persistida en LittleFS, introducida en el portal) ─────────
char mqttServer[40] = "192.168.1.1";
char mqttPort[6]    = "1883";

void saveMQTTConfig() {
  File f = LittleFS.open("/mqtt.json", "w");
  if (!f) { Serial.println(F("[FS] Error guardando config MQTT")); return; }
  JsonDocument doc;
  doc["server"] = mqttServer;
  doc["port"]   = mqttPort;
  serializeJson(doc, f);
  f.close();
  Serial.printf("[FS] Config MQTT guardada: %s:%s\n", mqttServer, mqttPort);
}

void loadMQTTConfig() {
  if (!LittleFS.exists("/mqtt.json")) {
    Serial.println(F("[FS] Sin config MQTT previa (se pedira en el portal)"));
    return;
  }
  File f = LittleFS.open("/mqtt.json", "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    strlcpy(mqttServer, doc["server"] | "192.168.1.1", sizeof(mqttServer));
    strlcpy(mqttPort,   doc["port"]   | "1883",        sizeof(mqttPort));
    Serial.printf("[FS] Config MQTT cargada: %s:%s\n", mqttServer, mqttPort);
  }
  f.close();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n[RELOJ] Arrancando..."));

  if (!LittleFS.begin()) {
    Serial.println(F("[FS] LittleFS no montado, formateando..."));
    LittleFS.format();
    LittleFS.begin();
  }
  loadMQTTConfig();

  display.begin();

  // ── WiFiManager: portal de configuracion ────────────────────────────────
  WiFiManager wm;

  WiFiManagerParameter p_server("server", "MQTT Server IP", mqttServer, 40);
  WiFiManagerParameter p_port  ("port",   "MQTT Port",      mqttPort,   6);
  wm.addParameter(&p_server);
  wm.addParameter(&p_port);

  wm.setSaveParamsCallback([&]() {
    strlcpy(mqttServer, p_server.getValue(), sizeof(mqttServer));
    strlcpy(mqttPort,   p_port.getValue(),   sizeof(mqttPort));
    saveMQTTConfig();
  });

  Serial.println(F("[WiFi] Conectando (o abriendo portal 'Reloj-Config')..."));
  if (!wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASS)) {
    Serial.println(F("[WiFi] Fallo al conectar, reiniciando..."));
    delay(1000);
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

  // Verificacion visual de esta etapa: mostrar la hora actual en azul.
  display.setBaseColor(DisplayEngine::hexToColor("#0000FF"));
  display.showTime(timeClient.getHours(), timeClient.getMinutes());
}

void loop() {
  timeClient.update();
  display.tickScroll();
}
