#include <Arduino.h>
#include "HackadayBoard.h"

void nv3007_smoke_test();

void HackadayBoard::begin() {
  ESP32Board::begin();

  // M1 iter 1: bring up the NV3007 display and paint a solid TFT_MESH
  // green so we can confirm SPI + panel init are working before wiring
  // the framebuffer through DisplayDriver in iter 2.
  Serial.println("[badge] NV3007 smoke test starting...");
  nv3007_smoke_test();
  Serial.println("[badge] NV3007 smoke test done.");
}
