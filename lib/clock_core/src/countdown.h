// countdown.h — Countdown state machine driven by an external millis() clock
#pragma once

enum class CountdownPhase { Idle, Running, AlarmOn, AlarmOff, Finished };

struct CountdownFrame {
  CountdownPhase phase;
  long secondsLeft;
  bool changed;  // displayed second or blink phase changed since the last tick
};

class Countdown {
public:
  Countdown(unsigned long alarmMs, unsigned long blinkMs);
  void start(unsigned long nowMs, long seconds);
  void cancel();
  bool active() const;  // Running, AlarmOn or AlarmOff
  CountdownFrame tick(unsigned long nowMs);

private:
  enum class State { Idle, Running, Alarm };
  unsigned long _alarmMs;
  unsigned long _blinkMs;
  State _state = State::Idle;
  unsigned long _endMs = 0;
  unsigned long _alarmStartMs = 0;
  long _lastSecs = -1;
  int _lastBlink = -1;
};
