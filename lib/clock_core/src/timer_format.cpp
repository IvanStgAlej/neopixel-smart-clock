// timer_format.cpp
#include "timer_format.h"
#include <stdio.h>

static const long TIMER_MAX = 359940;  // 99h59m

void formatTimer(long seconds, char out[5]) {
  if (seconds < 0) seconds = 0;
  if (seconds > TIMER_MAX) seconds = TIMER_MAX;
  int a, b;
  if (seconds >= 6000) {
    a = (int)(seconds / 3600);
    b = (int)((seconds % 3600) / 60);
  } else {
    a = (int)(seconds / 60);
    b = (int)(seconds % 60);
  }
  snprintf(out, 5, "%02d%02d", a, b);
}

long clampTimerSeconds(long seconds) {
  if (seconds < 0) return 0;
  if (seconds > TIMER_MAX) return TIMER_MAX;
  return seconds;
}
