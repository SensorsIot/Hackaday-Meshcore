#include "NV3007Display.h"
#include <SPI.h>
// Arduino_GFX_Library.h must come after NV3007Display.h so MeshCore's
// Color enum is parsed before Arduino_GFX defines RED/GREEN/BLUE as
// preprocessor macros.
#include <Arduino_GFX_Library.h>
// Proportional UI font (Adafruit GFXfont, MIT). Used for read-only
// info/list screens via setTextSize(3); see setTextSize() below.
#include "FreeSans9pt7b.h"

// Baseline offset for FreeSans9pt7b: cap glyphs reach ~13 px above the
// baseline, so adding this to the top-left y lands the text correctly.
#define NV3007_PROP_BASELINE 13

// NV3007 driver — MeshCore DisplayDriver wrapper over Arduino_GFX.
//
// We use Arduino_GFX (MIT) for the actual panel I/O and rendering
// primitives, with a 1-bit Canvas_Mono framebuffer that flushes to the
// panel in 16-bit BE RGB565. The TFT_MESH green color is applied at
// flush time by subclassing Canvas_Mono's flush() — the canvas itself
// stays monochrome (one bit per pixel ≈ 7.5 KB).
//
// Architectural choice (FSD §4, revised 2026-05-19): the from-scratch
// SPI + glyph driver got iters 1 and 2 working but stalled at text
// rendering. Switched to Arduino_GFX underneath the same DisplayDriver
// API rather than re-implement font handling, rotation, and partial
// redraw — that work is generic display-library territory, not
// badge-specific.

#ifndef PIN_TFT_BL
  #define PIN_TFT_BL 2
#endif
#ifndef PIN_TFT_DC
  #define PIN_TFT_DC 39
#endif
#ifndef PIN_TFT_CS
  #define PIN_TFT_CS 41
#endif
#ifndef PIN_TFT_RST
  #define PIN_TFT_RST 40
#endif
#ifndef PIN_TFT_SCK
  #define PIN_TFT_SCK 38
#endif
#ifndef PIN_TFT_MOSI
  #define PIN_TFT_MOSI 21
#endif

// TFT_MESH foreground color: RGB888 (0x67, 0xEA, 0x94) → RGB565 0x6752.
#define TFT_MESH_565 0x6752
#define TFT_BLK_565  0x0000

// Panel native orientation is 142 cols × 428 rows; user view is 428×142
// (landscape). Each row is padded to ceil(142/8)=18 bytes because
// Arduino_GFX::drawBitmap reads bitmaps with byteWidth-aligned rows
// (reading past a packed buffer would corrupt the image). Total =
// 428 rows × 18 bytes = 7704 bytes.
#define NV3007_PANEL_W       142
#define NV3007_PANEL_H       428
#define NV3007_FB_BYTES_ROW  18
#define NV3007_FB_BYTES      (NV3007_PANEL_H * NV3007_FB_BYTES_ROW)

// Custom 1-bit canvas. Inherits Arduino_GFX directly (not Canvas_Mono)
// because Canvas_Mono::setRotation only rotates clip bounds, not pixel
// writes — the bits would land at the wrong buffer positions. Here we
// keep the buffer in panel-native orientation and apply the 90° user→
// panel rotation in writePixelPreclipped, so every primitive that
// Arduino_GFX builds on writePixel (fillRect, drawChar, print, ...)
// automatically lands at the right user-facing position.
class BadgeCanvas : public Arduino_GFX {
public:
  BadgeCanvas(Arduino_GFX *output)
    : Arduino_GFX(NV3007_WIDTH, NV3007_HEIGHT), _output(output) {
    memset(_fb, 0, sizeof(_fb));
  }

  bool begin(int32_t speed = GFX_NOT_DEFINED) override {
    return _output ? _output->begin(speed) : true;
  }

  void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
    // (x, y) in user space (0..427, 0..141) → panel-native (cx, cy).
    // The badge's panel is mounted rotated 180° relative to the chip's
    // native orientation, so both axes flip: cx = y, cy = panel_h-1-x.
    int16_t cx = y;
    int16_t cy = NV3007_PANEL_H - 1 - x;
    uint8_t mask = 0x80 >> (cx & 7);
    uint8_t *byte_ptr = &_fb[cy * NV3007_FB_BYTES_ROW + (cx >> 3)];
    if (color & 0b1000010000010000) *byte_ptr |= mask;
    else                            *byte_ptr &= ~mask;
  }

  void flush(bool force = false) override {
    if (!_output) return;
    // Push row-by-row using draw16bitBeRGBBitmap — the same path
    // Meshtastic uses on this hardware. drawBitmap's per-pixel CASET/
    // RASET cycling didn't render reliably on the NV3007; this
    // explicit row push avoids that.
    uint16_t line[NV3007_PANEL_W];
    for (int row = 0; row < NV3007_PANEL_H; row++) {
      const uint8_t* src = &_fb[row * NV3007_FB_BYTES_ROW];
      for (int col = 0; col < NV3007_PANEL_W; col++) {
        bool on = (src[col >> 3] >> (7 - (col & 7))) & 1;
        uint16_t c = on ? TFT_MESH_565 : TFT_BLK_565;
        // Pre-swap to big-endian so draw16bitBeRGBBitmap pushes the
        // bytes in the order NV3007 expects on the wire.
        line[col] = (uint16_t)((c >> 8) | ((c & 0xFF) << 8));
      }
      _output->draw16bitBeRGBBitmap(0, row, line, NV3007_PANEL_W, 1);
    }
  }

private:
  uint8_t _fb[NV3007_FB_BYTES];
  Arduino_GFX *_output;
};

static inline uint16_t color_565(DisplayDriver::Color c) {
  return (c == DisplayDriver::DARK) ? TFT_BLK_565 : TFT_MESH_565;
}

bool NV3007Display::begin() {
  _bus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS,
                              PIN_TFT_SCK, PIN_TFT_MOSI,
                              GFX_NOT_DEFINED, HSPI);
  // rotation=0 + panel-native dims 142×428 + offsets (12,0) — exact
  // working values from the Meshtastic badge variant. The user→panel
  // 90° rotation lives in BadgeCanvas::writePixelPreclipped, not in
  // the panel chip, so the chip's MADCTL stays at its default.
  _panel = new Arduino_NV3007(_bus, PIN_TFT_RST, 0, false,
                              142, 428, 12, 0, 14, 0,
                              NV3007_279_init_operations,
                              sizeof(NV3007_279_init_operations));
  _canvas = new BadgeCanvas(_panel);

  if (!_canvas->begin()) return false;

  pinMode(PIN_TFT_BL, OUTPUT);
  digitalWrite(PIN_TFT_BL, HIGH);
  _on = true;
  return true;
}

void NV3007Display::turnOn()  { digitalWrite(PIN_TFT_BL, HIGH); _on = true; }
void NV3007Display::turnOff() { digitalWrite(PIN_TFT_BL, LOW);  _on = false; }

void NV3007Display::clear() {
  _canvas->fillScreen(TFT_BLK_565);
}

void NV3007Display::startFrame(Color bkg) {
  _canvas->fillScreen(color_565(bkg));
}

void NV3007Display::setTextSize(int sz) {
  // Text modes on the badge:
  //   1, 2 -> Arduino_GFX's built-in 5×7 bitmap font at N× scale
  //           (monospace; keeps the chat wrap math and the chrome text).
  //   3+   -> FreeSans9pt7b proportional GFXfont (~22 px line height),
  //           used by the read-only info/list screens for legibility.
  if (sz >= 3) {
    _canvas->setFont(&FreeSans9pt7b);
    _canvas->setTextSize(1);
    _baseline = NV3007_PROP_BASELINE;
  } else {
    _canvas->setFont(NULL);
    _canvas->setTextSize(sz);
    _baseline = 0;
  }
}

void NV3007Display::setColor(Color c) {
  _color = c;
  _canvas->setTextColor(color_565(c));
}

void NV3007Display::setCursor(int x, int y) {
  // _baseline is 0 for the bitmap font and the GFXfont ascent otherwise,
  // so callers always pass the top-left y of the text.
  _canvas->setCursor(x, y + _baseline);
}

void NV3007Display::print(const char* str) {
  _canvas->print(str);
}

void NV3007Display::fillRect(int x, int y, int w, int h) {
  _canvas->fillRect(x, y, w, h, color_565(_color));
}

void NV3007Display::drawRect(int x, int y, int w, int h) {
  _canvas->drawRect(x, y, w, h, color_565(_color));
}

void NV3007Display::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  _canvas->drawXBitmap(x, y, bits, w, h, color_565(_color));
}

uint16_t NV3007Display::getTextWidth(const char* str) {
  int16_t  bx, by;
  uint16_t bw, bh;
  _canvas->getTextBounds(str, 0, 0, &bx, &by, &bw, &bh);
  return bw;
}

void NV3007Display::endFrame() {
  _canvas->flush();
}
