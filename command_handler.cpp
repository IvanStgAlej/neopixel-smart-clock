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
      _powered = false;
      _mode = "apagado";
      _display->clear();
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

  publishLightState();
  publishState();
}

void CommandHandler::_handleCmd(const char* payload) {
  JsonDocument doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    Serial.println(F("[CMD] JSON invalido"));
    return;
  }

  String tipo = doc["tipo"] | "";
  Serial.printf("[CMD] Tipo: %s\n", tipo.c_str());

  // Color del comando: sobreescribe temporalmente el color de la entidad Light
  uint32_t color = 0;
  if (doc["color"].is<const char*>()) {
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

  } else {
    Serial.printf("[CMD] Tipo desconocido: %s\n", tipo.c_str());
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

  char buf[220];
  serializeJson(doc, buf, sizeof(buf));
  _mqtt->publish(MQTT_TOPIC_STATE, buf);
}

bool CommandHandler::isClockMode() const {
  return _powered && (_mode == "hora");
}

void CommandHandler::updateClock(int hours, int minutes) {
  if (!isClockMode()) return;
  char val[6];
  snprintf(val, sizeof(val), "%02d%02d", hours, minutes);
  _value = val;
  _display->showTime(hours, minutes);
}
