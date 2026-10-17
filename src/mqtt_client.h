// mqtt_client.h — MQTT connection, Discovery and pub/sub for Home Assistant
#pragma once
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "config.h"

// Callback receiving messages already copied into a NUL-terminated string
typedef void (*MQTTMessageCallback)(const char* topic, const char* payload);

class MQTTManager {
public:
  MQTTManager();
  void begin(const char* server, int port, MQTTMessageCallback callback);
  void loop();
  bool isConnected();
  void publish(const char* topic, const char* payload, bool retain = false);
  void publishDiscovery();

private:
  WiFiClient          _wifiClient;
  PubSubClient        _client;
  const char*         _server = nullptr;
  int                 _port = 1883;
  MQTTMessageCallback _userCallback = nullptr;
  unsigned long       _lastReconnectAttempt = 0;

  bool _connect();
  void _publishJson(const char* topic, JsonDocument& doc);  // serialize + publish (retained)
  static void _mqttCallback(char* topic, byte* payload, unsigned int length);
  static MQTTManager* _instance;   // singleton for the static callback
};
