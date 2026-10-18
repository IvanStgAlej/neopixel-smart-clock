// command_handler.h — Parses MQTT JSON and delegates to the DisplayEngine
#pragma once
#include <Arduino.h>
#include "display_engine.h"
#include "mqtt_client.h"
#include "countdown.h"
#include "timer_format.h"

class CommandHandler {
public:
  CommandHandler(DisplayEngine* display, MQTTManager* mqtt);

  // Call from the MQTT callback
  void handle(const char* topic, const char* payload);

  // State feedback
  void publishState();
  void publishLightState();

  // Clock mode (driven by the main loop with the NTP time)
  bool isClockMode() const;
  void updateClock(int hours, int minutes);

  // Timer: call on every loop iteration (repaints once per second)
  void tickTimer();

  // Temporary overlays: call on every loop iteration (ends them and restores the display)
  void tickOverlay();

private:
  DisplayEngine* _display;
  MQTTManager*   _mqtt;

  bool    _powered    = false;
  String  _mode       = "apagado";
  String  _value      = "";
  String  _color      = "#FFFFFF";
  uint8_t _brightness = NEO_BRIGHTNESS;
  int     _lastHours   = -1;   // last NTP time drawn (to repaint when the color changes)
  int     _lastMinutes = -1;

  // Timer state (countdown state machine lives in clock_core)
  Countdown     _countdown;
  uint32_t      _timerColor     = 0;
  bool          _timerRepaint   = false;  // force a repaint on the next tick (e.g. after restore)
  // What was shown before the timer, to return to when it ends or is cancelled
  bool          _prevPowered    = false;
  String        _prevMode       = "hora";
  String        _prevValue      = "";

  // Temporary overlay (weather, notice): painted on top of the current mode without
  // touching _powered/_mode/_value/_color, so ending it just repaints what was there.
  bool          _overlayActive  = false;
  unsigned long _overlayStart   = 0;
  unsigned long _overlayMs      = 0;

  void _beginOverlay(int32_t seconds);
  void _showWeather(float temperature, float rain, int32_t seconds);
  void _showNotice(const char* text, const char* hexColor, int32_t seconds);
  void _repaintCurrent();

  void _startTimer(long seconds, uint32_t color);
  void _endTimer(bool restore);  // restore=true → go back to the previous state and repaint it

  void _handleLightSet(const char* payload);
  void _handleCmd(const char* payload);
  void _restoreDisplay();   // re-render the current mode/content (on power-on or color change)
};
