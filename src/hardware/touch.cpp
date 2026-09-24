#include "hardware/touch.h"

#include "config.h"

#if defined(ARDUINO_WAVESHARE_ESP32_S3_TOUCH_LCD_2_1)

// CST820 capacitive touch on the shared I2C bus; reset line on the TCA9554.
// Polled, not interrupt driven: one 6-byte register read per loop() pass.

#include <Arduino.h>
#include <Wire.h>

#include "hardware/lgfx_config.hpp"

namespace hardware::touch {
namespace {

constexpr uint8_t kAddress = 0x15;
constexpr uint8_t kReadReg = 0x01;             // gesture, points, x, y
constexpr uint8_t kDisableAutoSleepReg = 0xFE;

bool s_ready = false;
bool s_was_down = false;

bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t reg, uint8_t* data, size_t len) {
  Wire.beginTransmission(kAddress);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom(kAddress, static_cast<uint8_t>(len)) != len) {
    while (Wire.available() > 0) {
      Wire.read();
    }
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    data[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

}  // namespace

void init() {
  waveshare_s3touch::tcaSet(config::kTca9554TouchResetPin, false);
  delay(10);
  waveshare_s3touch::tcaSet(config::kTca9554TouchResetPin, true);
  delay(50);

  s_ready = writeRegister(kDisableAutoSleepReg, 0xFF);
  Serial.printf("touch: CST820 %s\n", s_ready ? "ready" : "not responding");
}

bool readTap(Point* out) {
  if (!s_ready || out == nullptr) {
    return false;
  }
  uint8_t buf[6] = {};
  if (!readRegisters(kReadReg, buf, sizeof(buf))) {
    s_was_down = false;
    return false;
  }
  const bool down = buf[1] > 0;
  if (!down) {
    s_was_down = false;
    return false;
  }
  if (s_was_down) {
    return false;  // still the same press
  }
  s_was_down = true;
  out->x = static_cast<uint16_t>(((buf[2] & 0x0F) << 8) | buf[3]);
  out->y = static_cast<uint16_t>(((buf[4] & 0x0F) << 8) | buf[5]);
  return true;
}

}  // namespace hardware::touch

#else

namespace hardware::touch {
void init() {}
bool readTap(Point*) { return false; }
}  // namespace hardware::touch

#endif
