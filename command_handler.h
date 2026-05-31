// command_handler.h — Parsea JSON de MQTT y delega al DisplayEngine
#pragma once
#include <Arduino.h>
#include "display_engine.h"
#include "mqtt_client.h"

class CommandHandler {
public:
  CommandHandler(DisplayEngine* display, MQTTManager* mqtt);

  // Llamar desde el callback MQTT
  void handle(const char* topic, const char* payload);

  // Feedback de estado
  void publishState();
  void publishLightState();

  // Modo reloj (lo conduce el loop principal con la hora NTP)
  bool isClockMode() const;
  void updateClock(int hours, int minutes);

private:
  DisplayEngine* _display;
  MQTTManager*   _mqtt;

  bool    _powered    = false;
  String  _mode       = "apagado";
  String  _value      = "";
  String  _color      = "#FFFFFF";
  uint8_t _brightness = NEO_BRIGHTNESS;

  void _handleLightSet(const char* payload);
  void _handleCmd(const char* payload);
};
