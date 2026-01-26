#include "SSD1306Display.h"

bool SSD1306Display::i2c_probe(TwoWire& wire, uint8_t addr) {
  wire.beginTransmission(addr);
  uint8_t error = wire.endTransmission();
  return (error == 0);
}

bool SSD1306Display::begin() {
  Serial.println("SSD1306Display::begin() starting...");
  #if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Serial.printf("Wire.begin(SDA=%d, SCL=%d)\n", PIN_BOARD_SDA, PIN_BOARD_SCL);
  Wire.begin(PIN_BOARD_SDA, PIN_BOARD_SCL);
  #else
  Serial.println("Wire.begin() with default pins");
  Wire.begin();
  #endif

  bool probe_ok = i2c_probe(Wire, DISPLAY_ADDRESS);
  Serial.printf("I2C probe at 0x%02X: %s\n", DISPLAY_ADDRESS, probe_ok ? "FOUND" : "NOT FOUND");

  #ifdef DISPLAY_ROTATION
  display.setRotation(DISPLAY_ROTATION);
  #endif

  bool display_ok = display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS, true, false);
  Serial.printf("display.begin(): %s\n", display_ok ? "SUCCESS" : "FAILED");

  bool result = display_ok && probe_ok;
  Serial.printf("SSD1306Display::begin() returning: %s\n", result ? "true" : "false");
  return result;
}

void SSD1306Display::turnOn() {
  display.ssd1306_command(SSD1306_DISPLAYON);
  _isOn = true;
}

void SSD1306Display::turnOff() {
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  _isOn = false;
}

void SSD1306Display::clear() {
  display.clearDisplay();
  display.display();
}

void SSD1306Display::startFrame(Color bkg) {
  display.clearDisplay();  // TODO: apply 'bkg'
  _color = SSD1306_WHITE;
  display.setTextColor(_color);
  display.setTextSize(1);
  display.cp437(true);         // Use full 256 char 'Code Page 437' font
}

void SSD1306Display::setTextSize(int sz) {
  display.setTextSize(sz);
}

void SSD1306Display::setColor(Color c) {
  _color = (c != 0) ? SSD1306_WHITE : SSD1306_BLACK;
  display.setTextColor(_color);
}

void SSD1306Display::setCursor(int x, int y) {
  display.setCursor(x, y);
}

void SSD1306Display::print(const char* str) {
  display.print(str);
}

void SSD1306Display::fillRect(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, _color);
}

void SSD1306Display::drawRect(int x, int y, int w, int h) {
  display.drawRect(x, y, w, h, _color);
}

void SSD1306Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  display.drawBitmap(x, y, bits, w, h, SSD1306_WHITE);
}

uint16_t SSD1306Display::getTextWidth(const char* str) {
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(str, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void SSD1306Display::endFrame() {
  display.display();
}
