#pragma once

#include <Arduino.h>

// TCA8418 raw event payload. key_num is 1..80 (= row*10 + col + 1 for
// the badge's 8×10 matrix). pressed=true for press, false for release.
struct RawKeyEvent {
  uint8_t key_num;
  bool    pressed;
};

// MeshCore-shaped key event — populated by the keymap layer (iter B+).
struct KeyEvent {
  enum Type { CHAR, FUNCTION, NAV, MODIFIER };
  enum NavKey { UP, DOWN, LEFT, RIGHT, ENTER, ESC, BACKSPACE, TAB };
  Type type;
  union {
    char    chr;       // for CHAR
    uint8_t fn_num;    // 1..5 for F1..F5
    NavKey  nav;       // for NAV
  };
  bool pressed;
};

class TCA8418Keyboard {
public:
  // I2C init + matrix configuration + interrupt attach. Returns false
  // if the chip doesn't respond on I2C.
  bool begin();

  // Drain one entry from the TCA8418 event FIFO. Returns false if
  // nothing pending. Used for iter A hardware verification.
  bool getNextRawEvent(RawKeyEvent& out);

  // Translated key event (CHAR / FUNCTION / NAV). Drains raw events,
  // applies the keymap, and tracks modifier state internally. Returns
  // false when no translatable key is pending.
  bool getNextKey(KeyEvent& out);

private:
  // Held-modifier state. Shift selects SHIFT_MATRIX for the next char;
  // ctrl/alt/meta are tracked but not yet surfaced to the UI.
  bool _shift = false;
  bool _ctrl  = false;
  bool _alt   = false;
  bool _meta  = false;
};
