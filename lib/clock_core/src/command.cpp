// command.cpp — MQTT command JSON parsing
#include "command.h"
#include <ArduinoJson.h>
#include <string.h>

// Copies src into dst (cap bytes incl. NUL), truncating safely.
static void copyTruncated(char* dst, size_t cap, const char* src) {
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
}

static CommandType typeFromName(const char* tipo) {
  if (strcmp(tipo, "apagar") == 0) return CommandType::Off;
  if (strcmp(tipo, "hora") == 0) return CommandType::Clock;
  if (strcmp(tipo, "texto") == 0) return CommandType::Text;
  if (strcmp(tipo, "numero") == 0) return CommandType::Number;
  if (strcmp(tipo, "scroll") == 0) return CommandType::Scroll;
  if (strcmp(tipo, "temporizador") == 0) return CommandType::Timer;
  if (strcmp(tipo, "tiempo") == 0) return CommandType::Weather;
  if (strcmp(tipo, "aviso") == 0) return CommandType::Notice;
  return CommandType::Unknown;
}

// Number from a JSON value; strings and missing give 0.
static float floatFrom(JsonVariantConst v) {
  return v.is<double>() ? (float)v.as<double>() : 0.0f;
}

// Whole seconds from a JSON number; strings, missing and negatives give 0.
static int32_t secondsFrom(JsonVariantConst v) {
  if (!v.is<double>()) return 0;
  double d = v.as<double>();
  if (!(d >= 1.0)) return 0;                        // also rejects NaN
  if (d >= 2000000000.0) return 2000000000;         // caller clamps to the timer range
  return (int32_t)d;
}

// Overlay duration: missing/invalid → default, capped at the maximum.
static int32_t overlaySecondsFrom(JsonVariantConst v) {
  int32_t s = secondsFrom(v);
  if (s <= 0) return kOverlayDefaultSeconds;
  return s > kOverlayMaxSeconds ? kOverlayMaxSeconds : s;
}

Command parseCommand(const char* json, int defaultSpeedMs) {
  Command cmd;
  memset(&cmd, 0, sizeof(cmd));
  cmd.type = CommandType::Invalid;
  cmd.speedMs = defaultSpeedMs;

  if (json == nullptr) return cmd;
  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return cmd;

  const char* tipo = doc["tipo"].is<const char*>() ? doc["tipo"].as<const char*>() : "";
  cmd.type = typeFromName(tipo);
  if (cmd.type == CommandType::Unknown) return cmd;

  if (doc["color"].is<const char*>()) {
    cmd.hasColor = true;
    copyTruncated(cmd.color, sizeof(cmd.color), doc["color"].as<const char*>());
  }

  const char* valor = doc["valor"].is<const char*>() ? doc["valor"].as<const char*>() : nullptr;
  switch (cmd.type) {
    case CommandType::Text:
    case CommandType::Scroll:
      copyTruncated(cmd.value, sizeof(cmd.value), valor ? valor : "");
      if (cmd.type == CommandType::Scroll && doc["velocidad"].is<int>())
        cmd.speedMs = doc["velocidad"].as<int>();
      break;
    case CommandType::Number:
      copyTruncated(cmd.value, sizeof(cmd.value), valor ? valor : "0");
      break;
    case CommandType::Timer:
      cmd.seconds = secondsFrom(doc["segundos"]);
      break;
    case CommandType::Weather:
      cmd.temperature = floatFrom(doc["temperatura"]);
      cmd.rain        = floatFrom(doc["lluvia"]);
      cmd.seconds     = overlaySecondsFrom(doc["segundos"]);
      break;
    case CommandType::Notice:
      copyTruncated(cmd.value, sizeof(cmd.value), valor ? valor : "");
      cmd.seconds = overlaySecondsFrom(doc["segundos"]);
      break;
    default:
      break;
  }
  return cmd;
}
