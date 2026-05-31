// reloj_neopixel.ino — Reloj NeoPixel + Home Assistant (MQTT)
// WiFiManager (portal) + NTP + MQTT (Discovery, comandos, modo reloj autonomo).
#include "config.h"
#include "display_engine.h"
#include "mqtt_client.h"
#include "command_handler.h"

#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

DisplayEngine  display;
MQTTManager    mqtt;
CommandHandler cmdHandler(&display, &mqtt);

// ── NTP ───────────────────────────────────────────────────────────────────
WiFiUDP    ntpUDP;
NTPClient  timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_INTERVAL);

// ── Config MQTT (persistida en LittleFS, introducida en el portal) ─────────
char mqttServer[40] = "192.168.1.1";
char mqttPort[6]    = "1883";

// ── Temporizadores no bloqueantes ──────────────────────────────────────────
unsigned long lastClockUpdate = 0;
unsigned long lastHeartbeat   = 0;
bool          wifiWasConnected = false;

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

// Callback de MQTT → delega en el manejador de comandos
void onMQTTMessage(const char* topic, const char* payload) {
  Serial.printf("[MQTT] '%s': %s\n", topic, payload);
  cmdHandler.handle(topic, payload);
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
  wifiWasConnected = true;

  // ── NTP ──────────────────────────────────────────────────────────────────
  timeClient.begin();
  timeClient.update();
  Serial.printf("[NTP] Hora: %02d:%02d:%02d\n",
                timeClient.getHours(),
                timeClient.getMinutes(),
                timeClient.getSeconds());

  // ── MQTT ──────────────────────────────────────────────────────────────────
  mqtt.begin(mqttServer, atoi(mqttPort), onMQTTMessage);

  // Estado inicial: apagado, a la espera de comandos de HA (spec 4.2)
  cmdHandler.publishState();
  cmdHandler.publishLightState();
}

void loop() {
  // ── Pérdida de WiFi → apagar LEDs ────────────────────────────────────────
  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  if (wifiWasConnected && !wifiNow) {
    Serial.println(F("[WiFi] Conexion perdida — apagando LEDs"));
    display.clear();
  } else if (!wifiWasConnected && wifiNow) {
    Serial.println(F("[WiFi] Reconectado"));
    lastClockUpdate = 0;  // si estaba en modo reloj, reanuda al instante
  }
  wifiWasConnected = wifiNow;

  if (wifiNow) {
    mqtt.loop();
    timeClient.update();
  }
  display.tickScroll();

  // ── Modo reloj: refresca al entrar y luego cada minuto (solo con WiFi) ────
  if (cmdHandler.isClockMode()) {
    if (wifiNow) {
      unsigned long now = millis();
      if (lastClockUpdate == 0 || now - lastClockUpdate >= CLOCK_UPDATE_MS) {
        lastClockUpdate = now;
        cmdHandler.updateClock(timeClient.getHours(), timeClient.getMinutes());
      }
    }
  } else {
    lastClockUpdate = 0;  // reset: la proxima vez en modo reloj refresca al instante
  }

  // ── Heartbeat: estado cada 30s ───────────────────────────────────────────
  if (mqtt.isConnected()) {
    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      lastHeartbeat = now;
      cmdHandler.publishState();
    }
  }
}
