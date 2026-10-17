// countdown.cpp
#include "countdown.h"

Countdown::Countdown(uint32_t alarmMs, uint32_t blinkMs)
  : _alarmMs(alarmMs), _blinkMs(blinkMs) {}

void Countdown::start(uint32_t nowMs, long seconds) {
  _state = State::Running;
  _endMs = nowMs + (uint32_t)seconds * 1000u;
  _lastSecs = -1;
  _lastBlink = -1;
}

void Countdown::cancel() { _state = State::Idle; }

bool Countdown::active() const { return _state != State::Idle; }

CountdownFrame Countdown::tick(uint32_t nowMs) {
  if (_state == State::Idle) return {CountdownPhase::Idle, 0, false};

  if (_state == State::Running) {
    long remainingMs = (int32_t)(_endMs - nowMs);  // signed: survives millis() wrap
    if (remainingMs > 0) {
      long secs = (remainingMs + 999) / 1000;   // round up: shows 1 until the end
      bool changed = secs != _lastSecs;
      _lastSecs = secs;
      return {CountdownPhase::Running, secs, changed};
    }
    _state = State::Alarm;
    _alarmStartMs = nowMs;
    _lastBlink = -1;
  }

  uint32_t elapsed = nowMs - _alarmStartMs;
  if (elapsed >= _alarmMs) {
    _state = State::Idle;
    return {CountdownPhase::Finished, 0, true};
  }
  int on = ((elapsed / _blinkMs) % 2 == 0) ? 1 : 0;
  bool changed = on != _lastBlink;
  _lastBlink = on;
  return {on ? CountdownPhase::AlarmOn : CountdownPhase::AlarmOff, 0, changed};
}
