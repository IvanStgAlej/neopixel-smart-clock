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
  _client.setBufferSize(512);   // Discovery payload no cabe en los 256 por defecto
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

bool MQTTManager::isConnected() {
  return _client.connected();
}

void MQTTManager::publish(const char* topic, const char* payload, bool retain) {
  if (_client.connected()) {
    _client.publish(topic, payload, retain);
  }
}

bool MQTTManager::_connect() {
  Serial.print(F("[MQTT] Conectando..."));

  // Last Will: si el ESP se cae, el broker publica "offline" en reloj/available
  bool ok = _client.connect(
    MQTT_CLIENT_ID,
    MQTT_USER, MQTT_PASS,        // login dedicado del broker
    MQTT_TOPIC_AVAILABLE,        // topic del Last Will
    0, true,                     // QoS=0, retain=true
    "offline"                    // payload del Last Will
  );

  if (ok) {
    Serial.println(F(" OK"));
    _client.subscribe(MQTT_TOPIC_LIGHT_SET);
    _client.subscribe(MQTT_TOPIC_CMD);
    _client.subscribe(MQTT_TOPIC_DST_SET);
    publish(MQTT_TOPIC_AVAILABLE, "online", true);
    publishDiscovery();
    Serial.println(F("[MQTT] Suscrito a topics"));
  } else {
    Serial.printf(" FALLO (rc=%d)\n", _client.state());
  }
  return ok;
}

// Helper local: anade el bloque "device" comun a todas las entidades, para que
// HA las agrupe bajo el mismo dispositivo (identifiers "reloj_neopixel").
static void addDevice(JsonDocument& doc) {
  JsonObject device = doc["device"].to<JsonObject>();
  device["name"]         = "Reloj NeoPixel";
  device["model"]        = "ESP8266 NodeMCU";
  device["manufacturer"] = "DIY";
  device["sw_version"]   = FW_VERSION;
  device["identifiers"].to<JsonArray>().add("reloj_neopixel");
}

void MQTTManager::_publishJson(const char* topic, JsonDocument& doc) {
  char buf[512];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  _client.publish(topic, (const uint8_t*)buf, n, true);  // retain
}

void MQTTManager::publishDiscovery() {
  // ── Light (JSON schema): on/off, brillo, color ───────────────────────────
  {
    JsonDocument doc;
    doc["name"]               = "Reloj NeoPixel";
    doc["unique_id"]          = "reloj_neopixel_001";
    doc["schema"]             = "json";
    doc["state_topic"]        = MQTT_TOPIC_LIGHT_STATE;
    doc["command_topic"]      = MQTT_TOPIC_LIGHT_SET;
    doc["brightness"]         = true;
    doc["color_mode"]         = true;
    doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;
    doc["supported_color_modes"].to<JsonArray>().add("rgb");
    addDevice(doc);
    _publishJson(MQTT_DISCOVERY_TOPIC, doc);
  }

  // ── Switch "Horario de verano" (DST): ON=verano UTC+2 / OFF=invierno UTC+1 ─
  {
    JsonDocument doc;
    doc["name"]               = "Horario de verano";
    doc["unique_id"]          = "reloj_neopixel_dst";
    doc["command_topic"]      = MQTT_TOPIC_DST_SET;
    doc["state_topic"]        = MQTT_TOPIC_DST_STATE;
    doc["payload_on"]         = "ON";
    doc["payload_off"]        = "OFF";
    doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;
    doc["icon"]               = "mdi:weather-sunny";
    addDevice(doc);
    _publishJson(MQTT_DST_DISCOVERY_TOPIC, doc);
  }

  // ── Boton "Modo reloj": publica {"tipo":"hora"} en reloj/cmd ──────────────
  {
    JsonDocument doc;
    doc["name"]               = "Modo reloj";
    doc["unique_id"]          = "reloj_neopixel_hora";
    doc["command_topic"]      = MQTT_TOPIC_CMD;
    doc["payload_press"]      = "{\"tipo\":\"hora\"}";
    doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;
    doc["icon"]               = "mdi:clock-outline";
    addDevice(doc);
    _publishJson(MQTT_DISC_BTN_HORA, doc);
  }

  // ── Texto "Mensaje" (fijo): {"tipo":"texto","valor":"..."} ────────────────
  {
    JsonDocument doc;
    doc["name"]               = "Mensaje";
    doc["unique_id"]          = "reloj_neopixel_texto";
    doc["command_topic"]      = MQTT_TOPIC_CMD;
    doc["command_template"]   = "{\"tipo\":\"texto\",\"valor\":\"{{ value }}\"}";
    doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;
    doc["icon"]               = "mdi:message-text";
    doc["max"]                = 32;
    addDevice(doc);
    _publishJson(MQTT_DISC_TXT_FIJO, doc);
  }

  // ── Texto "Mensaje desplazante": {"tipo":"scroll","valor":"..."} ──────────
  {
    JsonDocument doc;
    doc["name"]               = "Mensaje desplazante";
    doc["unique_id"]          = "reloj_neopixel_scroll";
    doc["command_topic"]      = MQTT_TOPIC_CMD;
    doc["command_template"]   = "{\"tipo\":\"scroll\",\"valor\":\"{{ value }}\"}";
    doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;
    doc["icon"]               = "mdi:message-arrow-right";
    doc["max"]                = 64;
    addDevice(doc);
    _publishJson(MQTT_DISC_TXT_SCROLL, doc);
  }

  // ── Numero: {"tipo":"numero","valor":"..."} ───────────────────────────────
  {
    JsonDocument doc;
    doc["name"]               = "Numero";
    doc["unique_id"]          = "reloj_neopixel_numero";
    doc["command_topic"]      = MQTT_TOPIC_CMD;
    doc["command_template"]   = "{\"tipo\":\"numero\",\"valor\":\"{{ value | int }}\"}";
    doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;
    doc["icon"]               = "mdi:numeric";
    doc["min"]                = 0;
    doc["max"]                = 9999;
    doc["step"]               = 1;
    doc["mode"]               = "box";
    addDevice(doc);
    _publishJson(MQTT_DISC_NUM, doc);
  }

  Serial.println(F("[MQTT] Discovery publicado (light, switch, boton, 2x texto, numero)"));
}

void MQTTManager::_mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!_instance || !_instance->_userCallback) return;
  char buf[256] = {0};
  unsigned int n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  _instance->_userCallback(topic, buf);
}
