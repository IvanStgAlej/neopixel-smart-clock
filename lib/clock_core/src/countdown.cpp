// countdown.cpp
#include "countdown.h"

Countdown::Countdown(unsigned long alarmMs, unsigned long blinkMs)
  : _alarmMs(alarmMs), _blinkMs(blinkMs) {}

void Countdown::start(unsigned long nowMs, long seconds) {
  _state = State::Running;
  _endMs = nowMs + (unsigned long)seconds * 1000UL;
  _lastSecs = -1;
  _lastBlink = -1;
}

void Countdown::cancel() { _state = State::Idle; }

bool Countdown::active() const { return _state != State::Idle; }

CountdownFrame Countdown::tick(unsigned long nowMs) {
  if (_state == State::Idle) return {CountdownPhase::Idle, 0, false};

  if (_state == State::Running) {
    long remainingMs = (long)(_endMs - nowMs);  // signed: survives millis() wrap
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

  unsigned long elapsed = nowMs - _alarmStartMs;
  if (elapsed >= _alarmMs) {
    _state = State::Idle;
    return {CountdownPhase::Finished, 0, true};
  }
  int on = ((elapsed / _blinkMs) % 2 == 0) ? 1 : 0;
  bool changed = on != _lastBlink;
  _lastBlink = on;
  return {on ? CountdownPhase::AlarmOn : CountdownPhase::AlarmOff, 0, changed};
}
