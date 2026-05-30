// reloj_neopixel.ino — Test harness para DisplayEngine (Tasks 1-4)
// Sube este sketch para verificar visualmente todos los glifos y funciones.
#include "display_engine.h"

DisplayEngine display;

// Helper: bloquea hasta que el scroll termina, cediendo ciclos al WDT
static void runScroll(const char* text, uint32_t color, int speedMs) {
  display.startScroll(text, color, speedMs);
  while (display.isScrolling()) {
    display.tickScroll();
    yield();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n===================================="));
  Serial.println(F("  reloj_neopixel — DisplayEngine test"));
  Serial.println(F("  Tasks 1-4: digits, text, number, scroll"));
  Serial.println(F("====================================\n"));
  display.begin();
}

void loop() {
  // ── 1. Hora 12:34 en azul ───────────────────────────────────────────────
  Serial.println(F("[TEST] Mostrando hora 12:34 (azul #0000FF)"));
  display.setBaseColor(DisplayEngine::hexToColor("#0000FF"));
  display.showTime(12, 34);
  delay(2500);

  // ── 2. Texto HOLA en rojo ───────────────────────────────────────────────
  Serial.println(F("[TEST] Mostrando texto HOLA (rojo #FF0000)"));
  display.showText("HOLA", DisplayEngine::hexToColor("#FF0000"));
  delay(2500);

  // ── 3. Número 42 en verde ───────────────────────────────────────────────
  Serial.println(F("[TEST] Mostrando numero 42 (verde #00FF00)"));
  display.showNumber(42, DisplayEngine::hexToColor("#00FF00"));
  delay(2500);

  // ── 4. Scroll "ABCDEFGHIJ" en blanco ───────────────────────────────────
  Serial.println(F("[TEST] Scroll 'ABCDEFGHIJ' (blanco #FFFFFF, 200ms)"));
  runScroll("ABCDEFGHIJ", DisplayEngine::hexToColor("#FFFFFF"), 200);

  // ── 5. Pausa breve ──────────────────────────────────────────────────────
  display.clear();
  delay(500);

  // ── 6. Scroll completo de todos los glifos en cian ─────────────────────
  Serial.println(F("[TEST] Verifica cada glifo"));
  Serial.println(F("[TEST] Scroll '0123456789 ABCDEFGHIJLNOPRSTUYZ' (cian #00FFFF, 250ms)"));
  runScroll("0123456789 ABCDEFGHIJLNOPRSTUYZ",
            DisplayEngine::hexToColor("#00FFFF"), 250);

  // ── Pausa al final del ciclo ────────────────────────────────────────────
  display.clear();
  Serial.println(F("[TEST] Fin de ciclo. Repitiendo en 1s...\n"));
  delay(1000);
}
