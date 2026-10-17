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
  return CommandType::Unknown;
}

// Whole seconds from a JSON number; strings, missing and negatives give 0.
static int32_t secondsFrom(JsonVariantConst v) {
  if (!v.is<double>()) return 0;
  double d = v.as<double>();
  if (!(d >= 1.0)) return 0;                        // also rejects NaN
  if (d >= 2000000000.0) return 2000000000;         // caller clamps to the timer range
  return (int32_t)d;
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
    default:
      break;
  }
  return cmd;
}
