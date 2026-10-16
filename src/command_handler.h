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

  // Temporizador: llamar en cada vuelta del loop (repinta 1 vez por segundo)
  void tickTimer();

private:
  DisplayEngine* _display;
  MQTTManager*   _mqtt;

  bool    _powered    = false;
  String  _mode       = "apagado";
  String  _value      = "";
  String  _color      = "#FFFFFF";
  uint8_t _brightness = NEO_BRIGHTNESS;
  int     _lastHours   = -1;   // ultima hora NTP pintada (para repintar al cambiar color)
  int     _lastMinutes = -1;

  // Estado del temporizador
  bool          _timerActive    = false;
  bool          _timerAlarm     = false;  // ya llego a 0 y esta parpadeando
  unsigned long _timerEndMs     = 0;
  unsigned long _alarmStartMs   = 0;
  uint32_t      _timerColor     = 0;
  long          _lastTimerShown = -1;     // ultimo segundo pintado (evita repintar de mas)
  int           _lastBlinkOn    = -1;     // ultima fase del parpadeo pintada
  // Lo que se mostraba antes del temporizador, para volver al acabar o cancelar
  bool          _prevPowered    = false;
  String        _prevMode       = "hora";
  String        _prevValue      = "";

  void _startTimer(long seconds, uint32_t color);
  void _endTimer(bool restore);  // restore=true → vuelve a lo anterior y lo repinta
  void _renderTimer(long seconds);

  void _handleLightSet(const char* payload);
  void _handleCmd(const char* payload);
  void _restoreDisplay();   // re-renderiza el modo/contenido actual (al encender o cambiar color)
};
