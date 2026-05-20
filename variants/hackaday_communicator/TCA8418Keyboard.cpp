#include "TCA8418Keyboard.h"
#include <Wire.h>

// TCA8418 keyboard controller — I2C, 8×10 matrix, interrupt-driven
// event FIFO. Design doc §5: ISR sets a flag only, main loop drains
// the FIFO over I2C when the flag is set. No I2C in the ISR.

#ifndef PIN_I2C_SDA
  #define PIN_I2C_SDA 47
#endif
#ifndef PIN_I2C_SCL
  #define PIN_I2C_SCL 14
#endif
#ifndef PIN_KB_INT
  #define PIN_KB_INT 13
#endif
#ifndef PIN_KB_RST
  #define PIN_KB_RST 48
#endif
#ifndef TCA8418_KB_ADDR
  #define TCA8418_KB_ADDR 0x34
#endif

// TCA8418 register map (subset)
#define REG_CFG           0x01
#define REG_INT_STAT      0x02
#define REG_KEY_LCK_EC    0x03
#define REG_KEY_EVENT_A   0x04
#define REG_KP_GPIO1      0x1D
#define REG_KP_GPIO2      0x1E
#define REG_KP_GPIO3      0x1F

// CFG bits (datasheet table 6)
#define CFG_AI            0x80
#define CFG_GPI_E_CFG     0x40
#define CFG_OVR_FLOW_M    0x20
#define CFG_INT_CFG       0x10  // 1 = INT deasserts on INT_STAT read
#define CFG_OVR_FLOW_IEN  0x08
#define CFG_K_LCK_IEN     0x04
#define CFG_GPI_IEN       0x02
#define CFG_KE_IEN        0x01  // key-event interrupt enable

// INT_STAT bits — write 1 to clear
#define INT_STAT_K_INT    0x01

// --- Keymap --------------------------------------------------------------
// Transcribed clean-room from the reference MicroPython firmware
// (Hack-a-Day/2025-Communicator_Badge firmware/badge/hardware/keyboard.py).
// The TCA8418 reports key_num = row*10 + col + 1 (1..80), used directly as
// an index into these 81-entry tables (index 0 is unused).
//
// Special keys are encoded as byte values outside printable ASCII so chars
// and special keys can share one uint8_t table. Natural control codes
// (ENTER/TAB/BS/ESC/DEL) keep their ASCII values; everything else uses the
// 0x80+ KC_* sentinels below.
#define KC_NONE  0x00
#define KC_BS    0x08  // '\b'
#define KC_TAB   0x09  // '\t'
#define KC_ENTER 0x0A  // '\n'
#define KC_ESC   0x1B
#define KC_DEL   0x7F
#define KC_F1    0x81
#define KC_F2    0x82
#define KC_F3    0x83
#define KC_F4    0x84
#define KC_F5    0x85
#define KC_SFT   0x90
#define KC_CTL   0x91
#define KC_ALT   0x92
#define KC_META  0x93  // Jolly Wrencher (Hack-a-Day logo) key
#define KC_LEFT  0xA0
#define KC_UP    0xA1
#define KC_DOWN  0xA2
#define KC_RIGHT 0xA3

static const uint8_t KEY_MATRIX[81] = {
  KC_NONE,                                                                 // 0 unused
  // C0       C1     C2    C3    C4    C5      C6       C7     C8     C9
  KC_NONE,  KC_F1,  '+',  '9',  '8',  '7',    KC_F2,   KC_F3, KC_F4, KC_F5,   // R0  1-10
  KC_ESC,   'q',    'w',  'e',  'r',  't',    'y',     'u',   'i',   'o',     // R1 11-20
  KC_TAB,   'a',    's',  'd',  'f',  'g',    'h',     'j',   'k',   'l',     // R2 21-30
  KC_SFT,   'z',    'x',  'c',  'v',  'b',    'n',     'm',   ',',   '.',     // R3 31-40
  KC_CTL,   KC_META, KC_ALT, '\\', ' ', KC_NONE, KC_RIGHT, KC_DOWN, KC_LEFT, KC_ALT, // R4 41-50
  KC_NONE,  KC_NONE, '-', '6',  '5',  '4',    ']',     '[',   'p',   KC_NONE, // R5 51-60
  KC_NONE,  KC_NONE, '*', '3',  '2',  '1',    KC_ENTER,'\'',  ';',   KC_NONE, // R6 61-70
  KC_NONE,  KC_NONE, '/', '=',  '.',  '0',    KC_SFT,  KC_UP, KC_BS, KC_NONE, // R7 71-80
};

static const uint8_t SHIFT_MATRIX[81] = {
  KC_NONE,                                                                 // 0 unused
  // C0       C1     C2    C3    C4    C5      C6       C7     C8     C9
  KC_NONE,  KC_F1,  '+',  '(',  '*',  '&',    KC_F2,   KC_F3, KC_F4, KC_F5,   // R0  1-10
  '`',      'Q',    'W',  'E',  'R',  'T',    'Y',     'U',   'I',   'O',     // R1 11-20
  KC_TAB,   'A',    'S',  'D',  'F',  'G',    'H',     'J',   'K',   'L',     // R2 21-30
  KC_SFT,   'Z',    'X',  'C',  'V',  'B',    'N',     'M',   '<',   '>',     // R3 31-40
  KC_CTL,   KC_META, KC_ALT, '|', ' ', KC_NONE, KC_RIGHT, KC_DOWN, KC_LEFT, KC_ALT, // R4 41-50
  KC_NONE,  KC_NONE, '_', '^',  '%',  '$',    '}',     '{',   'P',   KC_NONE, // R5 51-60
  KC_NONE,  KC_NONE, '*', '#',  '@',  '!',    KC_ENTER,'"',   ':',   KC_NONE, // R6 61-70
  KC_NONE,  KC_NONE, '?', '+',  ',',  ')',    KC_SFT,  KC_UP, KC_DEL, KC_NONE,// R7 71-80
};

static volatile bool s_event_flag = false;

static void IRAM_ATTR kb_isr() {
  s_event_flag = true;
}

static bool writeReg(uint8_t reg, uint8_t val) {
  Wire1.beginTransmission(TCA8418_KB_ADDR);
  Wire1.write(reg);
  Wire1.write(val);
  return Wire1.endTransmission() == 0;
}

static int readReg(uint8_t reg) {
  Wire1.beginTransmission(TCA8418_KB_ADDR);
  Wire1.write(reg);
  if (Wire1.endTransmission(false) != 0) return -1;
  if (Wire1.requestFrom((uint8_t)TCA8418_KB_ADDR, (uint8_t)1) != 1) return -1;
  return Wire1.read();
}

bool TCA8418Keyboard::begin() {
  // Use I2C controller 1 (Wire1), NOT controller 0 (Wire). Hackaday's
  // official MicroPython firmware uses controller 1 deliberately, and
  // on this hardware controller 0 fails to talk to the TCA8418 — every
  // address NACKs (50 ms x 112 addresses = 6 s wedge). Cause not fully
  // understood, but switching to Wire1 makes the chip respond in
  // milliseconds with the default CFG=0x00.
  //
  // Order matches the MicroPython init: I2C bus first, then /RESET
  // pulse low->high (120 us each side).
  Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.setClock(100000);

  pinMode(PIN_KB_RST, OUTPUT);
  digitalWrite(PIN_KB_RST, LOW);
  delayMicroseconds(120);
  digitalWrite(PIN_KB_RST, HIGH);
  delayMicroseconds(120);

  // Diagnostic: scan the bus so we can see what's actually responding
  // when bring-up fails.
  Serial.print("[kb] I2C scan SDA=");
  Serial.print(PIN_I2C_SDA);
  Serial.print(" SCL=");
  Serial.print(PIN_I2C_SCL);
  Serial.print(" devices:");
  int found = 0;
  for (uint8_t a = 0x08; a < 0x78; a++) {
    Wire1.beginTransmission(a);
    if (Wire1.endTransmission() == 0) {
      Serial.printf(" 0x%02X", a);
      found++;
    }
  }
  Serial.printf("  (%d found)\n", found);

  // Probe: any successful read confirms the chip responds at 0x34.
  if (readReg(REG_CFG) < 0) return false;

  // Configure the 8×10 keypad matrix: rows R0..R7 + cols C0..C9.
  // KP_GPIO1 = rows (bit i = row i used as matrix scan line),
  // KP_GPIO2 = cols 0..7, KP_GPIO3 = cols 8..9.
  if (!writeReg(REG_KP_GPIO1, 0xFF)) return false;
  if (!writeReg(REG_KP_GPIO2, 0xFF)) return false;
  if (!writeReg(REG_KP_GPIO3, 0x03)) return false;

  // Drain any stale events queued from a previous boot.
  for (int i = 0; i < 10; i++) {
    int evt = readReg(REG_KEY_EVENT_A);
    if (evt <= 0) break;
  }
  writeReg(REG_INT_STAT, INT_STAT_K_INT);  // clear K_INT

  // Enable key-event interrupt + auto-deassert INT on INT_STAT read.
  if (!writeReg(REG_CFG, CFG_KE_IEN | CFG_INT_CFG)) return false;

  pinMode(PIN_KB_INT, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_KB_INT), kb_isr, FALLING);
  s_event_flag = false;

  return true;
}

bool TCA8418Keyboard::getNextRawEvent(RawKeyEvent& out) {
  // Two ways an event can be pending: ISR set the flag, or we missed
  // an edge but INT is still asserted (LOW). Either gates the I2C read
  // so we never poll the chip on a quiet bus.
  bool pending = s_event_flag || (digitalRead(PIN_KB_INT) == LOW);
  if (!pending) return false;

  int evt = readReg(REG_KEY_EVENT_A);
  if (evt <= 0) {
    // FIFO drained — clear K_INT so the line returns HIGH and we can
    // detect the next edge.
    writeReg(REG_INT_STAT, INT_STAT_K_INT);
    s_event_flag = false;
    return false;
  }

  out.pressed = (evt & 0x80) != 0;
  out.key_num = evt & 0x7F;

  // If KEC reports no more events queued, this was the last one for
  // the burst; clear the interrupt.
  int kec = readReg(REG_KEY_LCK_EC);
  if (kec >= 0 && (kec & 0x0F) == 0) {
    writeReg(REG_INT_STAT, INT_STAT_K_INT);
    s_event_flag = false;
  }
  return true;
}

bool TCA8418Keyboard::getNextKey(KeyEvent& out) {
  RawKeyEvent raw;
  while (getNextRawEvent(raw)) {
    if (raw.key_num == 0 || raw.key_num > 80) continue;

    // Modifier identity comes from the unshifted matrix — a modifier key
    // is the same key regardless of shift state. Track press/release and
    // emit nothing for the modifier itself (hold-to-shift).
    switch (KEY_MATRIX[raw.key_num]) {
      case KC_SFT:  _shift = raw.pressed; continue;
      case KC_CTL:  _ctrl  = raw.pressed; continue;
      case KC_ALT:  _alt   = raw.pressed; continue;
      case KC_META: _meta  = raw.pressed; continue;
    }

    // Everything else is reported on press only; swallow releases.
    if (!raw.pressed) continue;

    uint8_t code = _shift ? SHIFT_MATRIX[raw.key_num] : KEY_MATRIX[raw.key_num];
    if (code == KC_NONE) continue;

    out.pressed = true;
    switch (code) {
      case KC_F1: case KC_F2: case KC_F3: case KC_F4: case KC_F5:
        out.type = KeyEvent::FUNCTION;
        out.fn_num = (uint8_t)(code - KC_F1 + 1);
        return true;
      case KC_ENTER: out.type = KeyEvent::NAV; out.nav = KeyEvent::ENTER;     return true;
      case KC_TAB:   out.type = KeyEvent::NAV; out.nav = KeyEvent::TAB;       return true;
      case KC_ESC:   out.type = KeyEvent::NAV; out.nav = KeyEvent::ESC;       return true;
      case KC_BS:
      case KC_DEL:   out.type = KeyEvent::NAV; out.nav = KeyEvent::BACKSPACE; return true;
      case KC_LEFT:  out.type = KeyEvent::NAV; out.nav = KeyEvent::LEFT;      return true;
      case KC_RIGHT: out.type = KeyEvent::NAV; out.nav = KeyEvent::RIGHT;     return true;
      case KC_UP:    out.type = KeyEvent::NAV; out.nav = KeyEvent::UP;        return true;
      case KC_DOWN:  out.type = KeyEvent::NAV; out.nav = KeyEvent::DOWN;      return true;
      default:
        if (code >= 0x20 && code < 0x7F) {  // printable ASCII
          out.type = KeyEvent::CHAR;
          out.chr  = (char)code;
          return true;
        }
        continue;  // unknown sentinel — skip
    }
  }
  return false;
}
