#pragma once

#include <Arduino.h>
#include <helpers/ui/DisplayDriver.h>

// Forward declarations only — Arduino_GFX headers define RED/GREEN/BLUE
// as preprocessor macros that collide with MeshCore's Color enum, so
// they must not be included in this public header.
class Arduino_DataBus;
class Arduino_GFX;
class BadgeCanvas;

#ifndef NV3007_WIDTH
  #define NV3007_WIDTH  428
#endif
#ifndef NV3007_HEIGHT
  #define NV3007_HEIGHT 142
#endif

// MeshCore DisplayDriver implementation backed by Arduino_GFX
// (Arduino_NV3007 panel + Arduino_Canvas_Mono framebuffer). The
// wrapper keeps upstream MeshCore code on a stable abstraction; the
// rendering library can be swapped without touching badge code.
class NV3007Display : public DisplayDriver {
public:
  NV3007Display() : DisplayDriver(NV3007_WIDTH, NV3007_HEIGHT) {}

  bool begin();

  bool isOn() override { return _on; }
  void turnOn() override;
  void turnOff() override;
  void clear() override;
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override;
  void setColor(Color c) override;
  void setCursor(int x, int y) override;
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override;
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;

private:
  bool _on = false;
  Color _color = LIGHT;
  Arduino_DataBus* _bus    = nullptr;
  Arduino_GFX*     _panel  = nullptr;
  BadgeCanvas*     _canvas = nullptr;
};
