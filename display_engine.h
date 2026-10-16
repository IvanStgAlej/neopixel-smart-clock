// display_engine.h
#pragma once
#include <Adafruit_NeoPixel.h>
#include "config.h"

class DisplayEngine {
public:
  DisplayEngine();
  void begin();
  void clear();
  void setBrightness(uint8_t brightness);

  // Renderizado
  void showTime(int hours, int minutes);
  void showText(const char* text, uint32_t color);
  void showNumber(int number, uint32_t color);

  // Scroll
  void startScroll(const char* text, uint32_t color, int speedMs);
  void tickScroll();
  bool isScrolling() const;

  // Color base (usado por showTime cuando no se especifica color en el comando)
  void setBaseColor(uint32_t color);

  // Utilidades
  static uint32_t hexToColor(const char* hex);

private:
  Adafruit_NeoPixel _pixels;
  uint32_t _baseColor = 0xFFFFFF; // blanco por defecto
  void _renderDigit(int position, int digit, uint32_t color);
  void _renderChar(int position, char c, uint32_t color);
  static String _normalize(const char* text);  // UTF-8 (acentos, ñ) → ASCII sin tilde

  // Estado scroll
  bool     _scrolling      = false;
  String   _scrollText;
  uint32_t _scrollColor    = 0;
  int      _scrollSpeedMs  = SCROLL_DEFAULT_MS;
  int      _scrollPos      = 0;
  unsigned long _lastScrollTick = 0;
};
