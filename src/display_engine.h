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

  // Rendering
  void showTime(int hours, int minutes);
  void showText(const char* text, uint32_t color);
  void showNumber(int number, uint32_t color);
  // 4 characters: positions 0-1 in leftColor, 2-3 in rightColor
  void showSplit(const char* text, uint32_t leftColor, uint32_t rightColor);

  // Scroll
  void startScroll(const char* text, uint32_t color, int speedMs);
  void tickScroll();
  bool isScrolling() const;

  // Base color (used by showTime when the command specifies no color)
  void setBaseColor(uint32_t color);

  // Utilities
  static uint32_t hexToColor(const char* hex);

private:
  Adafruit_NeoPixel _pixels;
  uint32_t _baseColor = 0xFFFFFF; // white by default
  void _renderDigit(int position, int digit, uint32_t color);
  void _renderChar(int position, char c, uint32_t color);

  // Scroll state
  bool     _scrolling      = false;
  String   _scrollText;
  uint32_t _scrollColor    = 0;
  int      _scrollSpeedMs  = SCROLL_DEFAULT_MS;
  int      _scrollPos      = 0;
  unsigned long _lastScrollTick = 0;
};
