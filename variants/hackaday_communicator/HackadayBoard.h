#pragma once

#include <Arduino.h>
#include <helpers/ESP32Board.h>
#include <driver/rtc_io.h>

class HackadayBoard : public ESP32Board {
public:
  void begin();

  const char* getManufacturerName() const override {
    return "Hackaday Communicator";
  }
};
