#include <Arduino.h>
#include "HackadayBoard.h"
#include "TCA8418Keyboard.h"

extern TCA8418Keyboard keyboard;

void HackadayBoard::begin() {
  ESP32Board::begin();

  // Keyboard goes up here (not in app setup) because no MeshCore
  // example knows about TCA8418. The chip is on I2C controller 1
  // (Wire1) — see TCA8418Keyboard.cpp for the rationale.
  if (!keyboard.begin()) {
    Serial.println("[badge] WARNING: TCA8418 keyboard init failed");
  }
}
