// timer_format.h — Pure timer display formatting (no hardware dependencies)
#pragma once

// Writes 4 digits + NUL into out: MMSS below 6000 s (100 min), HHMM otherwise.
void formatTimer(long seconds, char out[5]);

// Clamps to the valid timer range [0, 359940].
long clampTimerSeconds(long seconds);
