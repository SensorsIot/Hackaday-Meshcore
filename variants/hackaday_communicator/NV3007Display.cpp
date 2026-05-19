#include "NV3007Display.h"

// NV3007 driver for the Hackaday 2025 Communicator badge.
//
// Panel: 142×428 native, displayed as 428×142 in the badge's physical
// orientation. SPI write-only, 16-bit RGB565 big-endian on the wire.
// Clean-room MIT implementation. The init byte sequence is transcribed
// from Arduino_GFX's nv3007_279_init_operations (MIT-licensed reference)
// as a hardware fact — the panel needs those exact registers to come up.

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

#ifndef NV3007_SPI_HZ
  #define NV3007_SPI_HZ 20000000
#endif

// Native-orientation column/row offsets for the 142×428 panel revision used
// on the badge. Same values Meshtastic passes to Arduino_NV3007.
#define NV3007_COL_OFFSET 12
#define NV3007_ROW_OFFSET 0
#define NV3007_PANEL_W    142
#define NV3007_PANEL_H    428

// TFT_MESH foreground from the badge notes doc: RGB888 (0x67, 0xEA, 0x94)
// → RGB565 0x6752. MSB-first SPI ships high byte first (0x67, 0x52).
#define TFT_MESH_HI 0x67
#define TFT_MESH_LO 0x52

// NV3007 command opcodes used here.
static constexpr uint8_t NV3007_SLPOUT = 0x11;
static constexpr uint8_t NV3007_DISPON = 0x29;
static constexpr uint8_t NV3007_CASET  = 0x2A;
static constexpr uint8_t NV3007_RASET  = 0x2B;
static constexpr uint8_t NV3007_RAMWR  = 0x2C;

// Tiny init-stream encoding: { op, n, [data x n] }.
//   op == 0xFE: data byte is a delay in ms.
//   any other op: SPI command byte, followed by n data bytes.
static const uint8_t NV3007_INIT[] = {
  // Hardware-fact byte sequence — Arduino_GFX MIT-licensed reference,
  // nv3007_279_init_operations array. Re-encoded into our flat format.
  0xFF, 1, 0xA5,
  0x9A, 1, 0x08,  0x9B, 1, 0x08,  0x9C, 1, 0xB0,  0x9D, 1, 0x16,  0x9E, 1, 0xC4,
  0x8F, 2, 0x55, 0x04,
  0x84, 1, 0x90,  0x83, 1, 0x7B,  0x85, 1, 0x33,
  0x60, 1, 0x00,  0x70, 1, 0x00,  0x61, 1, 0x02,  0x71, 1, 0x02,
  0x62, 1, 0x04,  0x72, 1, 0x04,  0x6C, 1, 0x29,  0x7C, 1, 0x29,
  0x6D, 1, 0x31,  0x7D, 1, 0x31,  0x6E, 1, 0x0F,  0x7E, 1, 0x0F,
  0x66, 1, 0x21,  0x76, 1, 0x21,  0x68, 1, 0x3A,  0x78, 1, 0x3A,
  0x63, 1, 0x07,  0x73, 1, 0x07,  0x64, 1, 0x05,  0x74, 1, 0x05,
  0x65, 1, 0x02,  0x75, 1, 0x02,  0x67, 1, 0x23,  0x77, 1, 0x23,
  0x69, 1, 0x08,  0x79, 1, 0x08,  0x6A, 1, 0x13,  0x7A, 1, 0x13,
  0x6B, 1, 0x13,  0x7B, 1, 0x13,  0x6F, 1, 0x00,  0x7F, 1, 0x00,
  0x50, 1, 0x00,  0x52, 1, 0xD6,  0x53, 1, 0x08,  0x54, 1, 0x08,
  0x55, 1, 0x1E,  0x56, 1, 0x1C,
  0xA0, 3, 0x2B, 0x24, 0x00,
  0xA1, 1, 0x87,  0xA2, 1, 0x86,  0xA5, 1, 0x00,  0xA6, 1, 0x00,
  0xA7, 1, 0x00,  0xA8, 1, 0x36,  0xA9, 1, 0x7E,  0xAA, 1, 0x7E,
  0xB9, 1, 0x85,  0xBA, 1, 0x84,  0xBB, 1, 0x83,  0xBC, 1, 0x82,
  0xBD, 1, 0x81,  0xBE, 1, 0x80,  0xBF, 1, 0x01,  0xC0, 1, 0x02,
  0xC1, 1, 0x00,  0xC2, 1, 0x00,  0xC3, 1, 0x00,
  0xC4, 1, 0x33,  0xC5, 1, 0x7E,  0xC6, 1, 0x7E,
  0xC8, 2, 0x33, 0x33,
  0xC9, 1, 0x68,  0xCA, 1, 0x69,  0xCB, 1, 0x6A,  0xCC, 1, 0x6B,
  0xCD, 2, 0x33, 0x33,
  0xCE, 1, 0x6C,  0xCF, 1, 0x6D,  0xD0, 1, 0x6E,  0xD1, 1, 0x6F,
  0xAB, 2, 0x03, 0x67,
  0xAC, 2, 0x03, 0x6B,
  0xAD, 2, 0x03, 0x68,
  0xAE, 2, 0x03, 0x6C,
  0xB3, 1, 0x00,  0xB4, 1, 0x00,  0xB5, 1, 0x00,
  0xB6, 1, 0x32,  0xB7, 1, 0x7E,  0xB8, 1, 0x7E,
  0xE0, 1, 0x00,
  0xE1, 2, 0x03, 0x0F,
  0xE2, 1, 0x04,  0xE3, 1, 0x01,  0xE4, 1, 0x0E,  0xE5, 1, 0x01,
  0xE6, 1, 0x19,  0xE7, 1, 0x10,  0xE8, 1, 0x10,  0xEA, 1, 0x12,
  0xEB, 1, 0xD0,  0xEC, 1, 0x04,  0xED, 1, 0x07,  0xEE, 1, 0x07,
  0xEF, 1, 0x09,  0xF0, 1, 0xD0,  0xF1, 1, 0x0E,  0xF9, 1, 0x17,
  0xF2, 4, 0x2C, 0x1B, 0x0B, 0x20,
  0xE9, 1, 0x29,  0xEC, 1, 0x04,
  0x35, 1, 0x00,
  0x44, 2, 0x00, 0x10,
  0x46, 1, 0x10,
  0xFF, 1, 0x00,
  0x3A, 1, 0x05,
  NV3007_SLPOUT, 0,
  0xFE, 1, 120,
  NV3007_DISPON, 0,
};

static SPIClass tft_spi(HSPI);

static inline void writeCmd(uint8_t c) {
  digitalWrite(PIN_TFT_DC, LOW);
  tft_spi.write(c);
}
static inline void writeData(uint8_t d) {
  digitalWrite(PIN_TFT_DC, HIGH);
  tft_spi.write(d);
}

static void setWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t x0 = x + NV3007_COL_OFFSET;
  uint16_t x1 = x0 + w - 1;
  uint16_t y0 = y + NV3007_ROW_OFFSET;
  uint16_t y1 = y0 + h - 1;
  writeCmd(NV3007_CASET);
  writeData(x0 >> 8); writeData(x0 & 0xFF);
  writeData(x1 >> 8); writeData(x1 & 0xFF);
  writeCmd(NV3007_RASET);
  writeData(y0 >> 8); writeData(y0 & 0xFF);
  writeData(y1 >> 8); writeData(y1 & 0xFF);
  writeCmd(NV3007_RAMWR);
  digitalWrite(PIN_TFT_DC, HIGH);
}

static void runInitSequence() {
  const uint8_t* p = NV3007_INIT;
  const uint8_t* end = p + sizeof(NV3007_INIT);
  while (p < end) {
    uint8_t op = *p++;
    uint8_t n  = *p++;
    if (op == 0xFE) { delay(*p++); continue; }
    writeCmd(op);
    while (n--) writeData(*p++);
  }
}

bool NV3007Display::begin() {
  pinMode(PIN_TFT_CS,  OUTPUT); digitalWrite(PIN_TFT_CS,  HIGH);
  pinMode(PIN_TFT_DC,  OUTPUT); digitalWrite(PIN_TFT_DC,  HIGH);
  pinMode(PIN_TFT_RST, OUTPUT); digitalWrite(PIN_TFT_RST, HIGH);
  pinMode(PIN_TFT_BL,  OUTPUT); digitalWrite(PIN_TFT_BL,  LOW);

  // Reset pulse
  delay(10);
  digitalWrite(PIN_TFT_RST, LOW);  delay(20);
  digitalWrite(PIN_TFT_RST, HIGH); delay(120);

  tft_spi.begin(PIN_TFT_SCK, -1, PIN_TFT_MOSI);
  tft_spi.setHwCs(false);
  tft_spi.beginTransaction(SPISettings(NV3007_SPI_HZ, MSBFIRST, SPI_MODE0));
  digitalWrite(PIN_TFT_CS, LOW);

  runInitSequence();
  delay(50);

  // Iter 1: paint the whole panel solid TFT_MESH green so we can verify
  // SPI + init reached the panel. Framebuffer comes in iter 2.
  setWindow(0, 0, NV3007_PANEL_W, NV3007_PANEL_H);
  uint8_t row[NV3007_PANEL_W * 2];
  for (int i = 0; i < NV3007_PANEL_W; i++) {
    row[i * 2]     = TFT_MESH_HI;
    row[i * 2 + 1] = TFT_MESH_LO;
  }
  for (int y = 0; y < NV3007_PANEL_H; y++) {
    tft_spi.writeBytes(row, sizeof(row));
  }

  digitalWrite(PIN_TFT_CS, HIGH);
  tft_spi.endTransaction();

  // Backlight on — we want photons.
  digitalWrite(PIN_TFT_BL, HIGH);

  _on = true;
  return true;
}

// Iter 1 hook — runs the same bring-up without going through the full
// MeshCore DisplayDriver pipeline. Called from HackadayBoard::begin() so we
// can verify the panel without pulling in the companion_radio UI stack.
void nv3007_smoke_test() {
  static NV3007Display d;
  d.begin();
}

void NV3007Display::turnOn() {
  digitalWrite(PIN_TFT_BL, HIGH);
  _on = true;
}

void NV3007Display::turnOff() {
  digitalWrite(PIN_TFT_BL, LOW);
  _on = false;
}
