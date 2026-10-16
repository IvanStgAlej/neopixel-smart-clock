// main.cpp — NeoPixel clock + Home Assistant (MQTT)
// WiFiManager (portal) + NTP + MQTT (Discovery, commands, autonomous clock mode).
#include <Arduino.h>
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
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

DisplayEngine  display;
MQTTManager    mqtt;
CommandHandler cmdHandler(&display, &mqtt);

// ── NTP ───────────────────────────────────────────────────────────────────
WiFiUDP    ntpUDP;
NTPClient  timeClient(ntpUDP, NTP_SERVER, NTP_OFFSET, NTP_INTERVAL);

// ── OTA (over-the-air updates) ──────────────────────────────────────────────
ESP8266WebServer        otaServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// ── MQTT config (persisted in LittleFS, entered in the portal) ─────────────
char mqttServer[40] = "192.168.1.1";
char mqttPort[6]    = "1883";

// ── Non-blocking timers ────────────────────────────────────────────────────
unsigned long lastClockUpdate = 0;
unsigned long lastHeartbeat   = 0;
bool          wifiWasConnected = false;
bool          mqttWasConnected = false;

// ── DST (daylight saving time) ─────────────────────────────────────────────
// false = winter (UTC+1), true = summer (UTC+2). Persisted in LittleFS.
bool          dstSummer        = false;

void saveMQTTConfig() {
  File f = LittleFS.open("/mqtt.json", "w");
  if (!f) { Serial.println(F("[FS] Error saving MQTT config")); return; }
  JsonDocument doc;
  doc["server"] = mqttServer;
  doc["port"]   = mqttPort;
  serializeJson(doc, f);
  f.close();
  Serial.printf("[FS] MQTT config saved: %s:%s\n", mqttServer, mqttPort);
}

void loadMQTTConfig() {
  if (!LittleFS.exists("/mqtt.json")) {
    Serial.println(F("[FS] No saved MQTT config (will be requested in the portal)"));
    return;
  }
  File f = LittleFS.open("/mqtt.json", "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    strlcpy(mqttServer, doc["server"] | "192.168.1.1", sizeof(mqttServer));
    strlcpy(mqttPort,   doc["port"]   | "1883",        sizeof(mqttPort));
    Serial.printf("[FS] MQTT config loaded: %s:%s\n", mqttServer, mqttPort);
  }
  f.close();
}

// ── DST: persistence and NTP offset handling ───────────────────────────────
void saveDSTConfig() {
  File f = LittleFS.open("/dst.json", "w");
  if (!f) { Serial.println(F("[FS] Error saving DST config")); return; }
  JsonDocument doc;
  doc["summer"] = dstSummer;
  serializeJson(doc, f);
  f.close();
  Serial.printf("[FS] DST config saved: %s\n", dstSummer ? "summer" : "winter");
}

void loadDSTConfig() {
  if (!LittleFS.exists("/dst.json")) {
    Serial.println(F("[FS] No saved DST config (default: winter)"));
    return;
  }
  File f = LittleFS.open("/dst.json", "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    dstSummer = doc["summer"] | false;
    Serial.printf("[FS] DST config loaded: %s\n", dstSummer ? "summer" : "winter");
  }
  f.close();
}

// Applies the NTP offset according to dstSummer (takes effect immediately on the displayed time)
void applyDSTOffset() {
  timeClient.setTimeOffset(dstSummer ? NTP_OFFSET_SUMMER : NTP_OFFSET_WINTER);
}

// Publishes the DST switch state to HA ("ON" = summer / "OFF" = winter)
void publishDSTState() {
  mqtt.publish(MQTT_TOPIC_DST_STATE, dstSummer ? "ON" : "OFF", true);
}

// Handles the DST switch command: updates the offset, persists, refreshes and publishes
void handleDSTCommand(const char* payload) {
  bool wantSummer = (strcmp(payload, "ON") == 0);
  if (wantSummer != dstSummer) {
    dstSummer = wantSummer;
    applyDSTOffset();
    saveDSTConfig();
    lastClockUpdate = 0;  // force an immediate refresh if in clock mode
    Serial.printf("[DST] Switched to %s\n", dstSummer ? "summer (UTC+2)" : "winter (UTC+1)");
  }
  publishDSTState();
}

// MQTT callback → delegates to the command handler
void onMQTTMessage(const char* topic, const char* payload) {
  Serial.printf("[MQTT] '%s': %s\n", topic, payload);
  if (strcmp(topic, MQTT_TOPIC_DST_SET) == 0) {
    handleDSTCommand(payload);   // the NTP offset lives in this file, not in CommandHandler
    return;
  }
  cmdHandler.handle(topic, payload);
}

// Sets up both over-the-air update paths (ArduinoOTA + web).
void setupOTA() {
  // 1) ArduinoOTA: the clock shows up as a "network port" in the Arduino IDE.
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    display.clear();   // turn the LEDs off during the update
    Serial.println(F("[OTA] Update started"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\n[OTA] Complete, rebooting..."));
  });
  ArduinoOTA.onProgress([](unsigned int prog, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", (prog * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[OTA] Error %u\n", err);
  });
  ArduinoOTA.begin();

  // 2) Core's built-in web updater: http://<ip>/update (upload a .bin)
  httpUpdater.setup(&otaServer, "/update", OTA_WEB_USER, OTA_WEB_PASS);
  otaServer.begin();

  Serial.printf("[OTA] Ready. IDE: '%s'  |  Web: http://%s/update\n",
                OTA_HOSTNAME, WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n[CLOCK] Starting..."));

  if (!LittleFS.begin()) {
    Serial.println(F("[FS] LittleFS not mounted, formatting..."));
    LittleFS.format();
    LittleFS.begin();
  }
  loadMQTTConfig();
  loadDSTConfig();

  display.begin();

  // ── WiFiManager: configuration portal ───────────────────────────────────
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

  Serial.println(F("[WiFi] Connecting (or opening portal 'Reloj-Config')..."));
  if (!wm.autoConnect(WIFI_AP_NAME, WIFI_AP_PASS)) {
    Serial.println(F("[WiFi] Connection failed, restarting..."));
    delay(1000);
    ESP.restart();
  }

  Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
  wifiWasConnected = true;

  // ── NTP ──────────────────────────────────────────────────────────────────
  timeClient.begin();
  applyDSTOffset();   // apply the summer/winter setting loaded from LittleFS before the first fetch
  timeClient.update();
  Serial.printf("[NTP] Time: %02d:%02d:%02d\n",
                timeClient.getHours(),
                timeClient.getMinutes(),
                timeClient.getSeconds());

  // ── OTA ────────────────────────────────────────────────────────────────────
  setupOTA();

  // ── MQTT ──────────────────────────────────────────────────────────────────
  // The actual connection happens in loop(); the initial state is published there
  // when the (re)connection is detected (starts off, waiting for HA).
  mqtt.begin(mqttServer, atoi(mqttPort), onMQTTMessage);
}

void loop() {
  // ── WiFi lost → turn LEDs off ────────────────────────────────────────────
  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  if (wifiWasConnected && !wifiNow) {
    Serial.println(F("[WiFi] Connection lost — turning LEDs off"));
    display.clear();
  } else if (!wifiWasConnected && wifiNow) {
    Serial.println(F("[WiFi] Reconnected"));
    lastClockUpdate = 0;  // if in clock mode, resume immediately
  }
  wifiWasConnected = wifiNow;

  if (wifiNow) {
    ArduinoOTA.handle();        // serves uploads from the IDE
    otaServer.handleClient();   // serves the /update web page
    mqtt.loop();
    timeClient.update();
  }

  // On MQTT (re)connect, publish state so HA doesn't show it as "unknown"
  bool mqttNow = mqtt.isConnected();
  if (mqttNow && !mqttWasConnected) {
    cmdHandler.publishLightState();
    cmdHandler.publishState();
    publishDSTState();   // sync the "Horario de verano" switch in HA
  }
  mqttWasConnected = mqttNow;

  display.tickScroll();
  cmdHandler.tickTimer();   // countdown and final blink (independent of WiFi)

  // ── Clock mode: refresh on entry, then every minute (WiFi only) ───────────
  if (cmdHandler.isClockMode()) {
    if (wifiNow) {
      unsigned long now = millis();
      if (lastClockUpdate == 0 || now - lastClockUpdate >= CLOCK_UPDATE_MS) {
        lastClockUpdate = now;
        cmdHandler.updateClock(timeClient.getHours(), timeClient.getMinutes());
      }
    }
  } else {
    lastClockUpdate = 0;  // reset: next time in clock mode, refresh immediately
  }

  // ── Heartbeat: state every 30 s ──────────────────────────────────────────
  if (mqtt.isConnected()) {
    unsigned long now = millis();
    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
      lastHeartbeat = now;
      cmdHandler.publishState();
    }
  }
}
