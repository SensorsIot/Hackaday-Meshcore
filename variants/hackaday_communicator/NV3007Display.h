#pragma once

#include <Arduino.h>
#include <SPI.h>
#include <helpers/ui/DisplayDriver.h>

#ifndef NV3007_WIDTH
  #define NV3007_WIDTH  428
#endif
#ifndef NV3007_HEIGHT
  #define NV3007_HEIGHT 142
#endif

class NV3007Display : public DisplayDriver {
public:
  NV3007Display() : DisplayDriver(NV3007_WIDTH, NV3007_HEIGHT) {}

  bool begin();

  bool isOn() override { return _on; }
  void turnOn() override;
  void turnOff() override;
  void clear() override {}
  void startFrame(Color bkg = DARK) override {}
  void setTextSize(int sz) override { _text_size = sz; }
  void setColor(Color c) override { _color = c; }
  void setCursor(int x, int y) override { _cursor_x = x; _cursor_y = y; }
  void print(const char* str) override {}
  void fillRect(int x, int y, int w, int h) override {}
  void drawRect(int x, int y, int w, int h) override {}
  void drawXbm(int x, int y, const uint8_t* bits, int w, int h) override {}
  uint16_t getTextWidth(const char* str) override { return 0; }
  void endFrame() override {}

private:
  bool _on = false;
  int _cursor_x = 0, _cursor_y = 0;
  int _text_size = 1;
  Color _color = LIGHT;
};
