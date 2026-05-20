// Minimal TCA8418 probe — no fancy diagnostics. Just the exact init
// sequence from Hackaday's official MicroPython keyboard.py, then a
// scan + register read. Repeats forever every 3 s.

#include <Arduino.h>
#include <Wire.h>

static int read_reg(TwoWire& w, uint8_t addr, uint8_t reg) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0) return -1;
  if (w.requestFrom(addr, (uint8_t)1) != 1) return -2;
  return w.read();
}

static void scan(TwoWire& w, const char* tag) {
  w.setTimeOut(50);
  Serial.printf("[probe] scan %s:", tag);
  int found = 0;
  unsigned long t0 = millis();
  for (uint8_t a = 0x08; a < 0x78; a++) {
    w.beginTransmission(a);
    if (w.endTransmission() == 0) {
      Serial.printf(" 0x%02X", a);
      found++;
    }
  }
  Serial.printf("  (%d found, %lu ms)\n", found, millis() - t0);
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("===== TCA8418 probe (minimal) =====");
  Serial.printf("SDA=%d SCL=%d INT=%d RST=%d ADDR=0x%02X\n",
                PIN_I2C_SDA, PIN_I2C_SCL, PIN_KB_INT, PIN_KB_RST,
                TCA8418_KB_ADDR);

  // Drive RST low BEFORE any I2C setup. This matches MicroPython,
  // which constructs the Pin object first (defaulting to 0/low) and
  // then assembles the I2C bus.
  pinMode(PIN_KB_RST, OUTPUT);
  digitalWrite(PIN_KB_RST, LOW);

  Wire1.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire1.setClock(100000);

  delayMicroseconds(120);
  digitalWrite(PIN_KB_RST, HIGH);
  delayMicroseconds(120);

  scan(Wire1, "post-RST");

  int cfg = read_reg(Wire1, TCA8418_KB_ADDR, 0x01);
  Serial.printf("[probe] reg 0x01 CFG = %d (0x%02X) %s\n",
                cfg, cfg < 0 ? 0 : cfg, cfg < 0 ? "FAILED" : "OK");
}

void loop() {
  delay(3000);
  scan(Wire1, "loop");
  int cfg = read_reg(Wire1, TCA8418_KB_ADDR, 0x01);
  Serial.printf("[probe] reg 0x01 CFG = %d (0x%02X) %s\n",
                cfg, cfg < 0 ? 0 : cfg, cfg < 0 ? "FAILED" : "OK");
}
