// reloj_neopixel.ino — Reloj NeoPixel + Home Assistant (MQTT)
// WiFiManager (portal) + NTP + MQTT (Discovery, comandos, modo reloj autonomo).
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

// ── OTA (actualizacion inalambrica) ─────────────────────────────────────────
ESP8266WebServer        otaServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// ── Config MQTT (persistida en LittleFS, introducida en el portal) ─────────
char mqttServer[40] = "192.168.1.1";
char mqttPort[6]    = "1883";

// ── Temporizadores no bloqueantes ──────────────────────────────────────────
unsigned long lastClockUpdate = 0;
unsigned long lastHeartbeat   = 0;
bool          wifiWasConnected = false;
bool          mqttWasConnected = false;

// ── DST (horario de verano) ────────────────────────────────────────────────
// false = invierno (UTC+1), true = verano (UTC+2). Persistido en LittleFS.
bool          dstSummer        = false;

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

// ── DST: persistencia y aplicacion del offset NTP ──────────────────────────
void saveDSTConfig() {
  File f = LittleFS.open("/dst.json", "w");
  if (!f) { Serial.println(F("[FS] Error guardando config DST")); return; }
  JsonDocument doc;
  doc["summer"] = dstSummer;
  serializeJson(doc, f);
  f.close();
  Serial.printf("[FS] Config DST guardada: %s\n", dstSummer ? "verano" : "invierno");
}

void loadDSTConfig() {
  if (!LittleFS.exists("/dst.json")) {
    Serial.println(F("[FS] Sin config DST previa (por defecto: invierno)"));
    return;
  }
  File f = LittleFS.open("/dst.json", "r");
  if (!f) return;
  JsonDocument doc;
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
    dstSummer = doc["summer"] | false;
    Serial.printf("[FS] Config DST cargada: %s\n", dstSummer ? "verano" : "invierno");
  }
  f.close();
}

// Aplica el offset NTP segun dstSummer (efecto inmediato sobre la hora mostrada)
void applyDSTOffset() {
  timeClient.setTimeOffset(dstSummer ? NTP_OFFSET_SUMMER : NTP_OFFSET_WINTER);
}

// Publica el estado del switch DST en HA ("ON" verano / "OFF" invierno)
void publishDSTState() {
  mqtt.publish(MQTT_TOPIC_DST_STATE, dstSummer ? "ON" : "OFF", true);
}

// Procesa el comando del switch DST: ajusta offset, persiste, refresca y publica
void handleDSTCommand(const char* payload) {
  bool nuevo = (strcmp(payload, "ON") == 0);
  if (nuevo != dstSummer) {
    dstSummer = nuevo;
    applyDSTOffset();
    saveDSTConfig();
    lastClockUpdate = 0;  // fuerza refresco inmediato si esta en modo reloj
    Serial.printf("[DST] Cambiado a %s\n", dstSummer ? "verano (UTC+2)" : "invierno (UTC+1)");
  }
  publishDSTState();
}

// Callback de MQTT → delega en el manejador de comandos
void onMQTTMessage(const char* topic, const char* payload) {
  Serial.printf("[MQTT] '%s': %s\n", topic, payload);
  if (strcmp(topic, MQTT_TOPIC_DST_SET) == 0) {
    handleDSTCommand(payload);   // el offset NTP vive en este .ino, no en CommandHandler
    return;
  }
  cmdHandler.handle(topic, payload);
}

// Configura las dos vias de actualizacion inalambrica (ArduinoOTA + web).
void setupOTA() {
  // 1) ArduinoOTA: el reloj aparece como "puerto de red" en el Arduino IDE.
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    display.clear();   // apaga los LEDs mientras se actualiza
    Serial.println(F("[OTA] Actualizacion iniciada"));
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("\n[OTA] Completada, reiniciando..."));
  });
  ArduinoOTA.onProgress([](unsigned int prog, unsigned int total) {
    Serial.printf("[OTA] %u%%\r", (prog * 100) / total);
  });
  ArduinoOTA.onError([](ota_error_t err) {
    Serial.printf("[OTA] Error %u\n", err);
  });
  ArduinoOTA.begin();

  // 2) Web updater integrado del core: http://<ip>/update (sube un .bin)
  httpUpdater.setup(&otaServer, "/update", OTA_WEB_USER, OTA_WEB_PASS);
  otaServer.begin();

  Serial.printf("[OTA] Listo. IDE: '%s'  |  Web: http://%s/update\n",
                OTA_HOSTNAME, WiFi.localIP().toString().c_str());
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
  loadDSTConfig();

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
  applyDSTOffset();   // aplica verano/invierno cargado de LittleFS antes del primer fetch
  timeClient.update();
  Serial.printf("[NTP] Hora: %02d:%02d:%02d\n",
                timeClient.getHours(),
                timeClient.getMinutes(),
                timeClient.getSeconds());

  // ── OTA ────────────────────────────────────────────────────────────────────
  setupOTA();

  // ── MQTT ──────────────────────────────────────────────────────────────────
  // La conexion real ocurre en loop(); el estado inicial se publica alli al
  // detectar la (re)conexion (spec 4.2: arranca apagado a la espera de HA).
  mqtt.begin(mqttServer, atoi(mqttPort), onMQTTMessage);
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
    ArduinoOTA.handle();        // atiende subidas desde el IDE
    otaServer.handleClient();   // atiende la pagina web /update
    mqtt.loop();
    timeClient.update();
  }

  // Al (re)conectar MQTT, publicar el estado para que HA no lo vea "desconocido"
  bool mqttNow = mqtt.isConnected();
  if (mqttNow && !mqttWasConnected) {
    cmdHandler.publishLightState();
    cmdHandler.publishState();
    publishDSTState();   // sincroniza el switch "Horario de verano" en HA
  }
  mqttWasConnected = mqttNow;

  display.tickScroll();
  cmdHandler.tickTimer();   // cuenta atras y parpadeo final (no depende del WiFi)

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
