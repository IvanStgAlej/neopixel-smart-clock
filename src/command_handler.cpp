// command_handler.cpp
#include "command_handler.h"
#include <ArduinoJson.h>
#include "command.h"
#include "weather.h"

static_assert(kCommandValueBytes == MAX_TEXT_BYTES,
              "Command::value size must match the display text buffer");

CommandHandler::CommandHandler(DisplayEngine* display, MQTTManager* mqtt)
  : _display(display), _mqtt(mqtt), _countdown(TIMER_ALARM_MS, TIMER_BLINK_MS) {}

void CommandHandler::handle(const char* topic, const char* payload) {
  if (strcmp(topic, MQTT_TOPIC_LIGHT_SET) == 0) {
    _handleLightSet(payload);
  } else if (strcmp(topic, MQTT_TOPIC_CMD) == 0) {
    _handleCmd(payload);
  }
}

void CommandHandler::_handleLightSet(const char* payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) return;
  _overlayActive = false;  // the light command repaints (or clears) the display itself

  // ON / OFF
  if (doc["state"].is<const char*>()) {
    String state = doc["state"].as<String>();
    if (state == "OFF") {
      if (_countdown.active()) _endTimer(false);  // on power-on, return to what was shown before the timer
      _powered = false;
      _display->clear();
      // _mode/_value/_color are NOT touched: remembered to restore on power-on.
      publishLightState();
      publishState();
      return;
    }
    _powered = true;
  }

  // Brightness (applied live)
  if (doc["brightness"].is<int>()) {
    _brightness = doc["brightness"].as<uint8_t>();
    _display->setBrightness(_brightness);
  }

  // RGB color → store as hex and set it as the base color
  if (doc["color"].is<JsonObject>()) {
    uint8_t r = doc["color"]["r"] | 255;
    uint8_t g = doc["color"]["g"] | 255;
    uint8_t b = doc["color"]["b"] | 255;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    _color = hex;
    _display->setBaseColor(Adafruit_NeoPixel::Color(r, g, b));
  }

  // Powered on → re-render what was shown before (or the new color/brightness)
  if (_powered) {
    _display->setBrightness(_brightness);
    _restoreDisplay();
  }

  publishLightState();
  publishState();
}

// Re-renders the content for the current mode. Used on power-on (to recover
// what was shown before switching off) and when color/brightness change.
void CommandHandler::_restoreDisplay() {
  uint32_t color = DisplayEngine::hexToColor(_color.c_str());
  if (_mode == "texto") {
    _display->showText(_value.c_str(), color);
  } else if (_mode == "numero") {
    _display->showNumber(_value.toInt(), color);
  } else if (_mode == "scroll") {
    _display->startScroll(_value.c_str(), color, SCROLL_DEFAULT_MS);
  } else if (_mode == "hora") {
    // Repaint immediately with the last cached time (showTime uses the base color,
    // already updated by setBaseColor). The loop keeps refreshing every minute.
    if (_lastHours >= 0) _display->showTime(_lastHours, _lastMinutes);
  } else if (_mode == "temporizador") {
    // Force tickTimer to repaint on the next loop iteration
    _timerRepaint = true;
  } else {
    // No previous content (boot/off) → default to clock mode.
    _mode = "hora";
  }
}

void CommandHandler::_handleCmd(const char* payload) {
  Command cmd = parseCommand(payload, SCROLL_DEFAULT_MS);  // ~290 B on the stack

  if (cmd.type == CommandType::Invalid) {
    Serial.println(F("[CMD] Invalid JSON"));
    return;
  }
  if (cmd.type == CommandType::Unknown) {
    Serial.println(F("[CMD] Unknown type"));
    return;
  }

  // Weather and notices are temporary overlays: they leave the mode, timer and color alone
  if (cmd.type == CommandType::Weather) {
    _showWeather(cmd.temperature, cmd.rain, cmd.seconds);
    return;
  }
  if (cmd.type == CommandType::Notice) {
    _showNotice(cmd.value, cmd.hasColor ? cmd.color : _color.c_str(), cmd.seconds);
    return;
  }
  bool hadOverlay = _overlayActive;
  _overlayActive = false;  // any other command replaces the overlay

  // Any other command replaces the running timer
  if (_countdown.active() && cmd.type != CommandType::Timer) _endTimer(false);

  // Command color: temporarily overrides the Light entity color
  // (the timer leaves it alone, so the previous color returns when it ends)
  uint32_t color = 0;
  if (cmd.type != CommandType::Timer && cmd.hasColor) {
    color  = DisplayEngine::hexToColor(cmd.color);
    _color = cmd.color;
  }

  switch (cmd.type) {
    case CommandType::Off:
      _powered = false;
      _mode = "apagado";
      _value = "";
      _display->clear();
      break;

    case CommandType::Clock:
      _powered = true;
      _mode = "hora";
      _value = "";
      // Rendering is done by the main loop (updateClock), which has the NTP time.
      break;

    case CommandType::Text:
      _powered = true;
      _mode = "texto";
      _value = cmd.value;
      _display->showText(cmd.value, color);
      break;

    case CommandType::Number:
      _powered = true;
      _mode = "numero";
      _value = cmd.value;
      _display->showNumber(_value.toInt(), color);
      break;

    case CommandType::Scroll:
      _powered = true;
      _mode = "scroll";
      _value = cmd.value;
      _display->startScroll(cmd.value, color, cmd.speedMs);
      break;

    case CommandType::Timer:
      // {"tipo":"temporizador","segundos":300[,"color":"#RRGGBB"]} · segundos=0 cancels
      if (cmd.seconds <= 0) {
        if (!_countdown.active()) {        // nothing to cancel
          if (hadOverlay) _repaintCurrent();
          return;
        }
        _endTimer(true);
      } else {
        uint32_t tcolor = DisplayEngine::hexToColor(cmd.hasColor ? cmd.color : _color.c_str());
        _startTimer(clampTimerSeconds(cmd.seconds), tcolor);
      }
      break;

    default:
      return;
  }

  publishState();
  publishLightState();
}

void CommandHandler::publishLightState() {
  JsonDocument doc;
  doc["state"] = _powered ? "ON" : "OFF";

  if (_powered) {
    doc["brightness"] = _brightness;
    long v = strtol(_color.c_str() + 1, nullptr, 16);  // _color = "#RRGGBB"
    JsonObject c = doc["color"].to<JsonObject>();
    c["r"] = (uint8_t)((v >> 16) & 0xFF);
    c["g"] = (uint8_t)((v >>  8) & 0xFF);
    c["b"] = (uint8_t)( v        & 0xFF);
    doc["color_mode"] = "rgb";
  }

  char buf[160];
  serializeJson(doc, buf, sizeof(buf));
  _mqtt->publish(MQTT_TOPIC_LIGHT_STATE, buf);
}

void CommandHandler::publishState() {
  JsonDocument doc;
  doc["encendido"] = _powered;
  doc["modo"]      = _mode;
  doc["valor"]     = _value;
  doc["color"]     = _color;
  doc["brillo"]    = _brightness;
  doc["ip"]        = WiFi.localIP().toString();
  doc["rssi"]      = WiFi.RSSI();

  doc["version"] = FW_VERSION;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  _mqtt->publish(MQTT_TOPIC_STATE, buf);
}

bool CommandHandler::isClockMode() const {
  // false during the overlay so the main loop doesn't paint over it; when it
  // ends the loop sees clock mode again and repaints the time immediately
  return _powered && (_mode == "hora") && !_overlayActive;
}

// ── Temporary overlays (weather, notice) ────────────────────────────────────

void CommandHandler::_beginOverlay(int32_t seconds) {
  _overlayActive = true;
  _overlayStart  = millis();
  _overlayMs     = (unsigned long)seconds * 1000UL;
  _display->setBrightness(_brightness);  // visible even if the clock was off
}

void CommandHandler::_showWeather(float temperature, float rain, int32_t seconds) {
  char text[5];
  formatWeather(temperature, rain, text);
  _beginOverlay(seconds);
  _display->showSplit(text, weatherTempColor(temperature), kWeatherRainColor);
  Serial.printf("[OVERLAY] Weather '%s' for %ld s\n", text, (long)seconds);
}

void CommandHandler::_showNotice(const char* text, const char* hexColor, int32_t seconds) {
  _beginOverlay(seconds);
  _display->showText(text, DisplayEngine::hexToColor(hexColor));
  Serial.printf("[OVERLAY] Notice '%s' for %ld s\n", text, (long)seconds);
}

void CommandHandler::tickOverlay() {
  if (!_overlayActive) return;
  if (millis() - _overlayStart < _overlayMs) return;
  _overlayActive = false;
  _repaintCurrent();
  Serial.println(F("[OVERLAY] Done, back to previous mode"));
}

// Repaints whatever the current state says should be on screen.
void CommandHandler::_repaintCurrent() {
  if (_countdown.active()) _timerRepaint = true;  // tickTimer repaints (or ends) it
  else if (_powered)       _restoreDisplay();     // clock mode is repainted by the main loop
  else                     _display->clear();
}

void CommandHandler::updateClock(int hours, int minutes) {
  if (!isClockMode()) return;
  _lastHours   = hours;     // cache to repaint immediately if color/brightness change
  _lastMinutes = minutes;
  char val[6];
  snprintf(val, sizeof(val), "%02d%02d", hours, minutes);
  _value = val;
  _display->showTime(hours, minutes);
}

// ── Timer ───────────────────────────────────────────────────────────────────

void CommandHandler::_startTimer(long seconds, uint32_t color) {
  // If one is already running it restarts, but what was shown before it is preserved
  if (!_countdown.active()) {
    _prevPowered = _powered;
    _prevMode    = _mode;
    _prevValue   = _value;
  }
  _countdown.start(millis(), seconds);
  _timerColor   = color;
  _powered      = true;
  _mode         = "temporizador";
  _value        = String(seconds);
  _timerRepaint = true;  // first tick paints immediately
  _display->setBrightness(_brightness);
  Serial.printf("[TIMER] Started: %ld s\n", seconds);
}

void CommandHandler::_endTimer(bool restore) {
  _countdown.cancel();
  _timerRepaint = false;
  _mode         = _prevMode;
  _value        = _prevValue;
  if (!restore) return;

  _powered = _prevPowered;
  if (_powered) _restoreDisplay();
  else          _display->clear();
  Serial.println(F("[TIMER] Done, back to previous mode"));
}

void CommandHandler::tickTimer() {
  if (!_countdown.active() || !_powered) return;
  if (_overlayActive) return;  // countdown keeps time via millis(); resumes after the overlay

  CountdownFrame f = _countdown.tick(millis());
  bool paint = f.changed || _timerRepaint;
  _timerRepaint = false;

  switch (f.phase) {
    case CountdownPhase::Running:
      if (paint) {
        char buf[5];
        formatTimer(f.secondsLeft, buf);
        _value = String(f.secondsLeft);
        _display->showText(buf, _timerColor);
      }
      break;
    case CountdownPhase::AlarmOn:
    case CountdownPhase::AlarmOff:
      if (_value != "0") {  // first alarm frame: reached 0
        _value = "0";
        Serial.println(F("[TIMER] Reached 0"));
        publishState();
      }
      if (paint) {
        _display->showText(f.phase == CountdownPhase::AlarmOn ? "0000" : "",
                           DisplayEngine::hexToColor(TIMER_ALARM_COLOR));
      }
      break;
    case CountdownPhase::Finished:
      _endTimer(true);
      publishState();
      publishLightState();
      break;
    case CountdownPhase::Idle:
      break;
  }
}
