// command_handler.cpp
#include "command_handler.h"
#include <ArduinoJson.h>

CommandHandler::CommandHandler(DisplayEngine* display, MQTTManager* mqtt)
  : _display(display), _mqtt(mqtt) {}

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

  // ON / OFF
  if (doc["state"].is<const char*>()) {
    String state = doc["state"].as<String>();
    if (state == "OFF") {
      if (_timerActive) _endTimer(false);  // al encender se vuelve a lo de antes del temporizador
      _powered = false;
      _display->clear();
      // NO se toca _mode/_value/_color: se recuerda para restaurarlo al encender.
      publishLightState();
      publishState();
      return;
    }
    _powered = true;
  }

  // Brillo (se aplica en vivo)
  if (doc["brightness"].is<int>()) {
    _brightness = doc["brightness"].as<uint8_t>();
    _display->setBrightness(_brightness);
  }

  // Color RGB → guardamos hex y lo fijamos como color base
  if (doc["color"].is<JsonObject>()) {
    uint8_t r = doc["color"]["r"] | 255;
    uint8_t g = doc["color"]["g"] | 255;
    uint8_t b = doc["color"]["b"] | 255;
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);
    _color = hex;
    _display->setBaseColor(Adafruit_NeoPixel::Color(r, g, b));
  }

  // Encendido → re-renderiza lo que estaba mostrando antes (o el nuevo color/brillo)
  if (_powered) {
    _display->setBrightness(_brightness);
    _restoreDisplay();
  }

  publishLightState();
  publishState();
}

// Re-renderiza el contenido segun el modo actual. Se usa al encender la luz
// (para recuperar lo que se mostraba antes de apagar) y al cambiar color/brillo.
void CommandHandler::_restoreDisplay() {
  uint32_t color = DisplayEngine::hexToColor(_color.c_str());
  if (_mode == "texto") {
    _display->showText(_value.c_str(), color);
  } else if (_mode == "numero") {
    _display->showNumber(_value.toInt(), color);
  } else if (_mode == "scroll") {
    _display->startScroll(_value.c_str(), color, SCROLL_DEFAULT_MS);
  } else if (_mode == "hora") {
    // Repinta al instante con la ultima hora cacheada (showTime usa el color base,
    // ya actualizado por setBaseColor). El loop seguira refrescando cada minuto.
    if (_lastHours >= 0) _display->showTime(_lastHours, _lastMinutes);
  } else if (_mode == "temporizador") {
    // Fuerza a tickTimer a repintar en la siguiente vuelta del loop
    _lastTimerShown = -1;
    _lastBlinkOn    = -1;
  } else {
    // Sin contenido previo (arranque/apagado) → por defecto, modo reloj.
    _mode = "hora";
  }
}

void CommandHandler::_handleCmd(const char* payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println(F("[CMD] JSON invalido"));
    return;
  }

  String tipo = doc["tipo"] | "";
  Serial.printf("[CMD] Tipo: %s\n", tipo.c_str());

  bool known = tipo == "apagar" || tipo == "hora" || tipo == "texto" ||
               tipo == "numero" || tipo == "scroll" || tipo == "temporizador";
  if (!known) {
    Serial.printf("[CMD] Tipo desconocido: %s\n", tipo.c_str());
    return;
  }

  // Cualquier otro comando sustituye al temporizador en marcha
  if (_timerActive && tipo != "temporizador") _endTimer(false);

  // Color del comando: sobreescribe temporalmente el color de la entidad Light
  // (el temporizador no lo toca, para que al acabar se vuelva al color de antes)
  uint32_t color = 0;
  if (tipo != "temporizador" && doc["color"].is<const char*>()) {
    color  = DisplayEngine::hexToColor(doc["color"].as<const char*>());
    _color = doc["color"].as<String>();
  }

  if (tipo == "apagar") {
    _powered = false;
    _mode = "apagado";
    _value = "";
    _display->clear();

  } else if (tipo == "hora") {
    _powered = true;
    _mode = "hora";
    _value = "";
    // El render lo hace el loop principal (updateClock) que tiene la hora NTP.

  } else if (tipo == "texto") {
    String val = doc["valor"] | "";
    _powered = true;
    _mode = "texto";
    _value = val;
    _display->showText(val.c_str(), color);

  } else if (tipo == "numero") {
    String val = doc["valor"] | "0";
    _powered = true;
    _mode = "numero";
    _value = val;
    _display->showNumber(val.toInt(), color);

  } else if (tipo == "scroll") {
    String val = doc["valor"] | "";
    int vel = doc["velocidad"] | SCROLL_DEFAULT_MS;
    _powered = true;
    _mode = "scroll";
    _value = val;
    _display->startScroll(val.c_str(), color, vel);

  } else if (tipo == "temporizador") {
    // {"tipo":"temporizador","segundos":300[,"color":"#RRGGBB"]} · segundos=0 cancela
    long secs = doc["segundos"] | 0L;
    if (secs <= 0) {
      if (!_timerActive) return;  // nada que cancelar
      _endTimer(true);
    } else {
      uint32_t tcolor = doc["color"].is<const char*>()
                          ? DisplayEngine::hexToColor(doc["color"].as<const char*>())
                          : DisplayEngine::hexToColor(_color.c_str());
      _startTimer(min(secs, (long)TIMER_MAX_SECONDS), tcolor);
    }
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

  char buf[220];
  serializeJson(doc, buf, sizeof(buf));
  _mqtt->publish(MQTT_TOPIC_STATE, buf);
}

bool CommandHandler::isClockMode() const {
  return _powered && (_mode == "hora");
}

void CommandHandler::updateClock(int hours, int minutes) {
  if (!isClockMode()) return;
  _lastHours   = hours;     // cache para repintar al instante si cambia color/brillo
  _lastMinutes = minutes;
  char val[6];
  snprintf(val, sizeof(val), "%02d%02d", hours, minutes);
  _value = val;
  _display->showTime(hours, minutes);
}

// ── Temporizador ────────────────────────────────────────────────────────────

void CommandHandler::_startTimer(long seconds, uint32_t color) {
  // Si ya habia uno en marcha se reinicia, pero se conserva lo que habia antes de el
  if (!_timerActive) {
    _prevPowered = _powered;
    _prevMode    = _mode;
    _prevValue   = _value;
  }
  _timerActive    = true;
  _timerAlarm     = false;
  _timerEndMs     = millis() + (unsigned long)seconds * 1000UL;
  _timerColor     = color;
  _powered        = true;
  _mode           = "temporizador";
  _lastTimerShown = seconds;
  _lastBlinkOn    = -1;
  _display->setBrightness(_brightness);
  _renderTimer(seconds);
  Serial.printf("[TIMER] Inicio: %ld s\n", seconds);
}

void CommandHandler::_endTimer(bool restore) {
  _timerActive = false;
  _timerAlarm  = false;
  _mode        = _prevMode;
  _value       = _prevValue;
  if (!restore) return;

  _powered = _prevPowered;
  if (_powered) _restoreDisplay();
  else          _display->clear();
  Serial.println(F("[TIMER] Fin, vuelta al modo anterior"));
}

// MM:SS por debajo de 100 minutos; HH:MM a partir de ahi
void CommandHandler::_renderTimer(long seconds) {
  char buf[12];
  if (seconds >= 100L * 60) {
    snprintf(buf, sizeof(buf), "%02ld%02ld", seconds / 3600, (seconds % 3600) / 60);
  } else {
    snprintf(buf, sizeof(buf), "%02ld%02ld", seconds / 60, seconds % 60);
  }
  _value = String(seconds);
  _display->showText(buf, _timerColor);
}

void CommandHandler::tickTimer() {
  if (!_timerActive || !_powered) return;
  unsigned long now = millis();

  if (!_timerAlarm) {
    long remainingMs = (long)(_timerEndMs - now);  // resta con signo: aguanta el desborde de millis()
    if (remainingMs > 0) {
      long secs = (remainingMs + 999) / 1000;      // redondeo hacia arriba: muestra 00:01 hasta el final
      if (secs != _lastTimerShown) {
        _lastTimerShown = secs;
        _renderTimer(secs);
      }
      return;
    }
    _timerAlarm   = true;
    _alarmStartMs = now;
    _lastBlinkOn  = -1;
    _value        = "0";
    Serial.println(F("[TIMER] Llego a 0"));
    publishState();
  }

  // Alarma: parpadeo de 0000 y vuelta a lo anterior
  unsigned long elapsed = now - _alarmStartMs;
  if (elapsed >= TIMER_ALARM_MS) {
    _endTimer(true);
    publishState();
    publishLightState();
    return;
  }
  int on = ((elapsed / TIMER_BLINK_MS) % 2 == 0) ? 1 : 0;
  if (on != _lastBlinkOn) {
    _lastBlinkOn = on;
    _display->showText(on ? "0000" : "", DisplayEngine::hexToColor(TIMER_ALARM_COLOR));
  }
}
