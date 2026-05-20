// Keymap verification — compiles the real TCA8418Keyboard driver and
// prints every decoded KeyEvent. Press keys on the badge; each press
// should print exactly one CHAR / FUNCTION / NAV line. Hold a shift key
// to confirm SHIFT_MATRIX selection.

#include <Arduino.h>
#include "TCA8418Keyboard.h"

TCA8418Keyboard keyboard;

static const char* navName(KeyEvent::NavKey n) {
  switch (n) {
    case KeyEvent::UP:        return "UP";
    case KeyEvent::DOWN:      return "DOWN";
    case KeyEvent::LEFT:      return "LEFT";
    case KeyEvent::RIGHT:     return "RIGHT";
    case KeyEvent::ENTER:     return "ENTER";
    case KeyEvent::ESC:       return "ESC";
    case KeyEvent::BACKSPACE: return "BACKSPACE";
    case KeyEvent::TAB:       return "TAB";
  }
  return "?";
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\n===== keymap verify =====");
  if (!keyboard.begin()) {
    Serial.println("[kv] keyboard.begin() FAILED");
    return;
  }
  Serial.println("[kv] ready — type keys (try lowercase, SHIFT+key, F1-F5, arrows, Enter, Backspace)");
}

void loop() {
  KeyEvent ev;
  while (keyboard.getNextKey(ev)) {
    switch (ev.type) {
      case KeyEvent::CHAR:
        Serial.printf("[kv] CHAR '%c' (0x%02X)\n", ev.chr, (uint8_t)ev.chr);
        break;
      case KeyEvent::FUNCTION:
        Serial.printf("[kv] FUNCTION F%u\n", ev.fn_num);
        break;
      case KeyEvent::NAV:
        Serial.printf("[kv] NAV %s\n", navName(ev.nav));
        break;
      case KeyEvent::MODIFIER:
        Serial.println("[kv] MODIFIER");
        break;
    }
  }
  delay(5);
}
