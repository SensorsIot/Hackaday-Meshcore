#pragma once

#include <Arduino.h>

struct KeyEvent {
  enum Type { CHAR, FUNCTION, NAV, MODIFIER };
  enum NavKey { UP, DOWN, LEFT, RIGHT, ENTER, ESC, BACKSPACE, TAB };
  Type type;
  union {
    char chr;
    uint8_t fn_num;
    NavKey nav;
  };
  bool pressed;
};

class TCA8418Keyboard {
public:
  bool begin();
  bool getNextKey(KeyEvent& out);
};
