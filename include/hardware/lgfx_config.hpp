#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include "config.h"

#if defined(ARDUINO_WAVESHARE_ESP32_S3_TOUCH_LCD_2_1)

// ---------------------------------------------------------------------------
// Waveshare ESP32-S3-Touch-LCD-2.1: ST7701 480×480 on the S3 RGB bus.
// LCD reset and CS sit behind a TCA9554 I2C expander, backlight is PWM on
// GPIO6. Init sequence and wiring taken from Waveshare's demo via the Selbyl
// radar port for this board.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>
#include <lgfx/v1/platforms/esp32/Light_PWM.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

namespace waveshare_s3touch {

inline uint8_t& tcaShadow() {
  static uint8_t shadow = 0;
  return shadow;
}

constexpr uint8_t tcaBit(uint8_t pin) { return static_cast<uint8_t>(1U << (pin - 1)); }

inline void tcaWrite(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(config::kTca9554Address);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

inline void tcaSet(uint8_t pin, bool high) {
  uint8_t& shadow = tcaShadow();
  if (high) {
    shadow |= tcaBit(pin);
  } else {
    shadow &= static_cast<uint8_t>(~tcaBit(pin));
  }
  tcaWrite(config::kTca9554OutputReg, shadow);
}

inline void displayCs(bool high) { tcaSet(config::kTca9554LcdCsPin, high); }

inline void initExpander() {
  Wire.begin(static_cast<int>(config::kI2cSdaPin), static_cast<int>(config::kI2cSclPin));
  Wire.setClock(config::kI2cFreqHz);

  tcaShadow() = tcaBit(config::kTca9554LcdResetPin) | tcaBit(config::kTca9554TouchResetPin) |
                tcaBit(config::kTca9554LcdCsPin) | tcaBit(config::kTca9554SdCsPin);
  tcaWrite(config::kTca9554OutputReg, tcaShadow());
  tcaWrite(config::kTca9554ConfigReg, 0x00);  // all EXIO as outputs
  tcaSet(config::kTca9554BuzzerPin, false);
}

inline void resetDisplay() {
  tcaSet(config::kTca9554LcdResetPin, false);
  delay(10);
  tcaSet(config::kTca9554LcdResetPin, true);
  delay(50);
}

}  // namespace waveshare_s3touch

class Panel_ST7701_Waveshare21 : public lgfx::Panel_ST7701_Base {
 public:
  bool init(bool use_reset) override {
    waveshare_s3touch::displayCs(false);
    const bool ok = lgfx::Panel_ST7701_Base::init(use_reset);
    waveshare_s3touch::displayCs(true);
    return ok;
  }

 protected:
  const uint8_t* getInitCommands(uint8_t listno) const override {
    static constexpr const uint8_t list0[] = {
        0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x10,
        0xC0, 2, 0x3B, 0x00,
        0xC1, 2, 0x0B, 0x02,
        0xC2, 2, 0x07, 0x02,
        0xCC, 1, 0x10,
        0xCD, 1, 0x08,
        0xB0, 16, 0x00, 0x11, 0x16, 0x0E, 0x11, 0x06, 0x05, 0x09,
                  0x08, 0x21, 0x06, 0x13, 0x10, 0x29, 0x31, 0x18,
        0xB1, 16, 0x00, 0x11, 0x16, 0x0E, 0x11, 0x07, 0x05, 0x09,
                  0x09, 0x21, 0x05, 0x13, 0x11, 0x2A, 0x31, 0x18,

        0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x11,
        0xB0, 1, 0x6D,
        0xB1, 1, 0x37,
        0xB2, 1, 0x81,
        0xB3, 1, 0x80,
        0xB5, 1, 0x43,
        0xB7, 1, 0x85,
        0xB8, 1, 0x20,
        0xC1, 1, 0x78,
        0xC2, 1, 0x78,
        0xD0, 1, 0x88,
        0xE0, 3, 0x00, 0x00, 0x02,
        0xE1, 11, 0x03, 0xA0, 0x00, 0x00, 0x04, 0xA0, 0x00, 0x00,
                  0x00, 0x20, 0x20,
        0xE2, 13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                  0x00, 0x00, 0x00, 0x00, 0x00,
        0xE3, 4, 0x00, 0x00, 0x11, 0x00,
        0xE4, 2, 0x22, 0x00,
        0xE5, 16, 0x05, 0xEC, 0xA0, 0xA0, 0x07, 0xEE, 0xA0, 0xA0,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xE6, 4, 0x00, 0x00, 0x11, 0x00,
        0xE7, 2, 0x22, 0x00,
        0xE8, 16, 0x06, 0xED, 0xA0, 0xA0, 0x08, 0xEF, 0xA0, 0xA0,
                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xEB, 7, 0x00, 0x00, 0x40, 0x40, 0x00, 0x00, 0x00,
        0xED, 16, 0xFF, 0xFF, 0xFF, 0xBA, 0x0A, 0xBF, 0x45, 0xFF,
                  0xFF, 0x54, 0xFB, 0xA0, 0xAB, 0xFF, 0xFF, 0xFF,
        0xEF, 6, 0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F,

        0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x13,
        0xEF, 1, 0x08,

        0xFF, 5, 0x77, 0x01, 0x00, 0x00, 0x00,
        0x36, 1, 0x00,
        0x3A, 1, 0x66,
        0x11, CMD_INIT_DELAY, 255,
        0x20, CMD_INIT_DELAY, 120,
        0x29, 0,
        0xFF, 0xFF,
    };
    return listno == 0 ? list0 : nullptr;
  }
};

class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_RGB _bus;
  Panel_ST7701_Waveshare21 _panel;
  lgfx::Light_PWM _light;

 public:
  LGFX() {
    {
      auto cfg = _panel.config();
      cfg.pin_rst = -1;  // reset is on the expander, see resetDisplay()
      cfg.memory_width = config::kDisplayWidth;
      cfg.memory_height = config::kDisplayHeight;
      cfg.panel_width = config::kDisplayWidth;
      cfg.panel_height = config::kDisplayHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.invert = config::kDisplayInvert;
      cfg.rgb_order = config::kDisplayRgbOrder;
      _panel.config(cfg);
    }
    {
      auto cfg = _panel.config_detail();
      cfg.pin_cs = -1;  // CS is on the expander, toggled in init()
      cfg.pin_sclk = static_cast<int>(config::kDisplayPinSclk);
      cfg.pin_mosi = static_cast<int>(config::kDisplayPinMosi);
      cfg.use_psram = 2;  // RGB framebuffer lives in PSRAM
      _panel.config_detail(cfg);
    }
    {
      auto cfg = _bus.config();
      cfg.panel = &_panel;
      cfg.pin_d0 = static_cast<int>(config::kDisplayPinD0);
      cfg.pin_d1 = static_cast<int>(config::kDisplayPinD1);
      cfg.pin_d2 = static_cast<int>(config::kDisplayPinD2);
      cfg.pin_d3 = static_cast<int>(config::kDisplayPinD3);
      cfg.pin_d4 = static_cast<int>(config::kDisplayPinD4);
      cfg.pin_d5 = static_cast<int>(config::kDisplayPinD5);
      cfg.pin_d6 = static_cast<int>(config::kDisplayPinD6);
      cfg.pin_d7 = static_cast<int>(config::kDisplayPinD7);
      cfg.pin_d8 = static_cast<int>(config::kDisplayPinD8);
      cfg.pin_d9 = static_cast<int>(config::kDisplayPinD9);
      cfg.pin_d10 = static_cast<int>(config::kDisplayPinD10);
      cfg.pin_d11 = static_cast<int>(config::kDisplayPinD11);
      cfg.pin_d12 = static_cast<int>(config::kDisplayPinD12);
      cfg.pin_d13 = static_cast<int>(config::kDisplayPinD13);
      cfg.pin_d14 = static_cast<int>(config::kDisplayPinD14);
      cfg.pin_d15 = static_cast<int>(config::kDisplayPinD15);
      cfg.pin_hsync = static_cast<int>(config::kDisplayPinHsync);
      cfg.pin_vsync = static_cast<int>(config::kDisplayPinVsync);
      cfg.pin_henable = static_cast<int>(config::kDisplayPinDe);
      cfg.pin_pclk = static_cast<int>(config::kDisplayPinPclk);
      cfg.freq_write = config::kDisplayRgbPclkHz;
      cfg.hsync_pulse_width = config::kDisplayHsyncPulseWidth;
      cfg.hsync_back_porch = config::kDisplayHsyncBackPorch;
      cfg.hsync_front_porch = config::kDisplayHsyncFrontPorch;
      cfg.vsync_pulse_width = config::kDisplayVsyncPulseWidth;
      cfg.vsync_back_porch = config::kDisplayVsyncBackPorch;
      cfg.vsync_front_porch = config::kDisplayVsyncFrontPorch;
      cfg.hsync_polarity = false;
      cfg.vsync_polarity = false;
      cfg.pclk_active_neg = false;
      cfg.pclk_idle_high = false;
      cfg.de_idle_high = false;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _light.config();
      cfg.pin_bl = static_cast<int>(config::kDisplayBacklightPin);
      cfg.freq = config::kDisplayBacklightPwmHz;
      cfg.pwm_channel = 1;
      cfg.invert = false;
      _light.config(cfg);
      _panel.light(&_light);
    }
    setPanel(&_panel);
  }

  bool init_impl(bool use_reset, bool use_clear) override {
    waveshare_s3touch::initExpander();
    waveshare_s3touch::resetDisplay();
    return lgfx::LGFX_Device::init_impl(use_reset, use_clear);
  }
};

#else

/** LovyanGFX device: GC9A01 on SPI (ESP32-C3 Super Mini). Pin values come from config.h. */
class LGFX : public lgfx::LGFX_Device {
  lgfx::Bus_SPI _bus;
  lgfx::Panel_GC9A01 _panel;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.freq_write = config::kDisplaySpiWriteHz;
      cfg.pin_sclk = static_cast<int>(config::kDisplayPinSclk);
      cfg.pin_mosi = static_cast<int>(config::kDisplayPinMosi);
      cfg.pin_miso = -1;
      cfg.pin_dc = static_cast<int>(config::kDisplayPinDc);
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {
      auto cfg = _panel.config();
      cfg.pin_cs = static_cast<int>(config::kDisplayPinCs);
      cfg.pin_rst = static_cast<int>(config::kDisplayPinRst);
      cfg.invert = config::kDisplayInvert;
      cfg.rgb_order = config::kDisplayRgbOrder;
      _panel.config(cfg);
    }
    setPanel(&_panel);
  }
};

#endif
