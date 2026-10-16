// countdown.h — Countdown state machine driven by an external millis() clock
#pragma once
#include <stdint.h>

enum class CountdownPhase { Idle, Running, AlarmOn, AlarmOff, Finished };

struct CountdownFrame {
  CountdownPhase phase;
  long secondsLeft;
  bool changed;  // displayed second or blink phase changed since the last tick
};

class Countdown {
public:
  Countdown(uint32_t alarmMs, uint32_t blinkMs);
  void start(uint32_t nowMs, long seconds);
  void cancel();
  bool active() const;  // Running, AlarmOn or AlarmOff
  CountdownFrame tick(uint32_t nowMs);

private:
  enum class State { Idle, Running, Alarm };
  uint32_t _alarmMs;
  uint32_t _blinkMs;
  State _state = State::Idle;
  uint32_t _endMs = 0;
  uint32_t _alarmStartMs = 0;
  long _lastSecs = -1;
  int _lastBlink = -1;
};
