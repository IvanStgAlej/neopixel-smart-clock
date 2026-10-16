// Unit tests for glyphs (7-segment masks) and text normalization.
#include <unity.h>
#include <string.h>
#include "glyphs.h"

// ── Reference data: the ORIGINAL tables from display_engine.cpp, kept verbatim
// so the refactor is proven to change nothing.
static const uint8_t REF_DIGIT_MAP[10][15] = {
  {12, 2,3, 4,5, 6,7, 8,9, 10,11, 12,13, 0,0},  // 0
  { 4, 6,7, 8,9, 0,0, 0,0,  0,0,   0,0,  0,0},  // 1
  {10, 0,1, 4,5, 6,7, 10,11, 12,13, 0,0, 0,0},  // 2
  {10, 0,1, 4,5, 6,7, 8,9,  10,11,  0,0, 0,0},  // 3
  { 8, 0,1, 2,3, 6,7, 8,9,   0,0,   0,0, 0,0},  // 4
  {10, 0,1, 2,3, 4,5, 8,9,  10,11,  0,0, 0,0},  // 5
  {12, 0,1, 2,3, 4,5, 8,9,  10,11, 12,13, 0,0}, // 6
  { 6, 4,5, 6,7, 8,9, 0,0,   0,0,   0,0, 0,0},  // 7
  {14, 0,1, 2,3, 4,5, 6,7,   8,9,  10,11,12,13},// 8
  {12, 0,1, 2,3, 4,5, 6,7,   8,9,  10,11, 0,0}, // 9
};
static const char REF_CHAR_KEYS[] = " -ABCDEFGHIJLNOPRSTUYZKMQVWX";
static const uint8_t REF_CHAR_MAP[][15] = {
  { 0, 0,0, 0,0, 0,0, 0,0,  0,0,  0,0,  0,0},   // ' ' (espacio)
  { 2, 0,1, 0,0, 0,0, 0,0,  0,0,  0,0,  0,0},   // '-'  G
  {12, 0,1, 2,3, 4,5, 6,7,  8,9, 12,13, 0,0},   // 'A'
  {10, 0,1, 2,3, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'B'
  { 8, 2,3, 4,5, 10,11, 12,13, 0,0, 0,0, 0,0},  // 'C'
  {10, 0,1, 6,7, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'D'
  {10, 0,1, 2,3, 4,5, 10,11, 12,13, 0,0, 0,0},  // 'E'
  { 8, 0,1, 2,3, 4,5, 12,13, 0,0,  0,0, 0,0},   // 'F'
  {10, 2,3, 4,5, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'G'
  {10, 0,1, 2,3, 6,7, 8,9,  12,13, 0,0, 0,0},   // 'H'
  { 4, 6,7, 8,9, 0,0, 0,0,   0,0,  0,0, 0,0},   // 'I'
  { 6, 6,7, 8,9, 10,11, 0,0, 0,0,  0,0, 0,0},   // 'J'
  { 6, 2,3, 10,11, 12,13, 0,0, 0,0, 0,0, 0,0},  // 'L'
  { 6, 0,1, 8,9, 12,13, 0,0, 0,0,  0,0, 0,0},   // 'N'
  {12, 2,3, 4,5, 6,7, 8,9, 10,11, 12,13, 0,0},  // 'O'
  {10, 0,1, 2,3, 4,5, 6,7, 12,13,  0,0, 0,0},   // 'P'
  { 4, 0,1, 12,13, 0,0, 0,0, 0,0,  0,0, 0,0},   // 'R'
  {10, 0,1, 2,3, 4,5, 8,9, 10,11,  0,0, 0,0},   // 'S'
  { 8, 0,1, 2,3, 10,11, 12,13, 0,0, 0,0, 0,0},  // 'T'
  {10, 2,3, 6,7, 8,9, 10,11, 12,13, 0,0, 0,0},  // 'U'
  {10, 0,1, 2,3, 6,7, 8,9, 10,11,  0,0, 0,0},   // 'Y'
  {10, 0,1, 4,5, 6,7, 10,11, 12,13, 0,0, 0,0},  // 'Z'
  // ── Letras anadidas: aproximaciones 7-seg (completan el alfabeto) ──────────
  { 8, 2,3, 0,1, 6,7, 12,13, 0,0, 0,0, 0,0},    // 'K'  F,G,B,E
  {10, 4,5, 6,7, 8,9, 12,13, 2,3, 0,0, 0,0},    // 'M'  A,B,C,E,F (~Pi)
  {10, 4,5, 6,7, 8,9, 2,3, 0,1, 0,0, 0,0},      // 'Q'  A,B,C,F,G (~9)
  { 6, 8,9, 10,11, 12,13, 0,0, 0,0, 0,0, 0,0},  // 'V'  C,D,E (cubeta baja)
  {12, 6,7, 8,9, 10,11, 12,13, 2,3, 0,1, 0,0},  // 'W'  B,C,D,E,F,G
  { 8, 6,7, 8,9, 12,13, 2,3, 0,0, 0,0, 0,0},    // 'X'  B,C,E,F
};

static uint16_t maskFromRow(const uint8_t* row) {
  uint16_t m = 0;
  for (uint8_t i = 1; i <= row[0]; i++) m |= (uint16_t)(1u << row[i]);
  return m;
}

void setUp() {}
void tearDown() {}

void test_known_masks() {
  TEST_ASSERT_EQUAL_HEX16(0x3FFF, glyphMask('8'));  // all 14 LEDs
  TEST_ASSERT_EQUAL_HEX16(0x03C0, glyphMask('1'));  // B,C = LEDs 6-9
  TEST_ASSERT_EQUAL_HEX16(0x3FFC, glyphMask('0'));  // everything but G
  TEST_ASSERT_EQUAL_HEX16(0x0003, glyphMask('-'));  // G
  TEST_ASSERT_EQUAL_HEX16(glyphMask('A'), glyphMask('a'));
  TEST_ASSERT_EQUAL_HEX16(0, glyphMask(' '));
  TEST_ASSERT_EQUAL_HEX16(0, glyphMask('?'));
}

void test_equivalence_digits() {
  for (int d = 0; d < 10; d++) {
    TEST_ASSERT_EQUAL_HEX16(maskFromRow(REF_DIGIT_MAP[d]), glyphMask((char)('0' + d)));
  }
}

void test_equivalence_chars() {
  for (int i = 0; REF_CHAR_KEYS[i] != '\0'; i++) {
    char c = REF_CHAR_KEYS[i];
    TEST_ASSERT_EQUAL_HEX16(maskFromRow(REF_CHAR_MAP[i]), glyphMask(c));
    if (c >= 'A' && c <= 'Z') {
      TEST_ASSERT_EQUAL_HEX16(maskFromRow(REF_CHAR_MAP[i]), glyphMask((char)(c + 32)));
    }
  }
}

void test_normalize_accents() {
  char out[64];
  size_t n = normalizeText("Canci\xC3\xB3n \xC3\x91U", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("CanciOn NU", out);
  TEST_ASSERT_EQUAL(10, n);
}

void test_normalize_literal_utf8() {
  char out[64];
  normalizeText("Canción ÑU", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("CanciOn NU", out);
}

void test_normalize_truncated_utf8() {
  char out[64];
  normalizeText("ab\xC3", out, sizeof out);
  TEST_ASSERT_EQUAL_STRING("ab", out);
}

void test_normalize_truncated_output() {
  char out[4];
  size_t n = normalizeText("abcdef", out, 4);
  TEST_ASSERT_EQUAL_STRING("abc", out);
  TEST_ASSERT_EQUAL(3, n);
}

void test_normalize_skips_other_multibyte_and_zero_size() {
  char out[16];
  normalizeText("a\xC3\x80z", out, sizeof out);  // U+00C0 not mapped: skipped
  TEST_ASSERT_EQUAL_STRING("az", out);
  TEST_ASSERT_EQUAL(0, normalizeText("abc", out, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_known_masks);
  RUN_TEST(test_equivalence_digits);
  RUN_TEST(test_equivalence_chars);
  RUN_TEST(test_normalize_accents);
  RUN_TEST(test_normalize_literal_utf8);
  RUN_TEST(test_normalize_truncated_utf8);
  RUN_TEST(test_normalize_truncated_output);
  RUN_TEST(test_normalize_skips_other_multibyte_and_zero_size);
  return UNITY_END();
}
