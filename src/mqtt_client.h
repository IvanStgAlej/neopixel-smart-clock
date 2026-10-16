// mqtt_client.h — Conexion MQTT, Discovery y pub/sub para Home Assistant
#pragma once
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "config.h"

// Callback que recibe los mensajes ya copiados a string terminada en '\0'
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
  void _publishJson(const char* topic, JsonDocument& doc);  // serializa + publica (retain)
  static void _mqttCallback(char* topic, byte* payload, unsigned int length);
  static MQTTManager* _instance;   // singleton para el callback estatico
};
