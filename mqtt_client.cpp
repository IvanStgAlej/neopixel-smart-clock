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

  // Last Will: si el ESP se cae, el broker publica "offline" en reloj/available
  bool ok = _client.connect(
    MQTT_CLIENT_ID,
    nullptr, nullptr,            // sin usuario/contrasena
    MQTT_TOPIC_AVAILABLE,        // topic del Last Will
    0, true,                     // QoS=0, retain=true
    "offline"                    // payload del Last Will
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
  // Payload de MQTT Discovery: registra una entidad Light (JSON schema) en HA
  JsonDocument doc;
  doc["name"]               = "Reloj NeoPixel";
  doc["unique_id"]          = "reloj_neopixel_001";
  doc["schema"]             = "json";
  doc["state_topic"]        = MQTT_TOPIC_LIGHT_STATE;
  doc["command_topic"]      = MQTT_TOPIC_LIGHT_SET;
  doc["brightness"]         = true;
  doc["color_mode"]         = true;
  doc["availability_topic"] = MQTT_TOPIC_AVAILABLE;

  JsonArray modes = doc["supported_color_modes"].to<JsonArray>();
  modes.add("rgb");

  JsonObject device = doc["device"].to<JsonObject>();
  device["name"]         = "Reloj NeoPixel";
  device["model"]        = "ESP8266 NodeMCU";
  device["manufacturer"] = "DIY";
  JsonArray ids = device["identifiers"].to<JsonArray>();
  ids.add("reloj_neopixel");

  char buf[512];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  _client.publish(MQTT_DISCOVERY_TOPIC, (const uint8_t*)buf, n, true); // retain
  Serial.println(F("[MQTT] Discovery publicado"));
}

void MQTTManager::_mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (!_instance || !_instance->_userCallback) return;
  char buf[256] = {0};
  unsigned int n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';
  _instance->_userCallback(topic, buf);
}
