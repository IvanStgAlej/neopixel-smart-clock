// glyphs.h - 7-segment glyph masks and text normalization (hardware-free).
#pragma once
#include <stdint.h>
#include <stddef.h>

// Bit i (0-13) set means relative LED i of the digit is lit.
// Segment layout per digit: G=0,1 F=2,3 A=4,5 B=6,7 C=8,9 D=10,11 E=12,13.
// Lowercase letters map to uppercase. Unknown characters return 0.
uint16_t glyphMask(char c);

// Converts UTF-8 accents/u-diaeresis/n-tilde (lead byte 0xC3) to the plain
// uppercase letter; other multibyte sequences are skipped. Always
// NUL-terminates (when outSize > 0) and returns the length written.
size_t normalizeText(const char* in, char* out, size_t outSize);
