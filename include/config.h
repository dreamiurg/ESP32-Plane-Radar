#pragma once

#include <cstdint>

#include <driver/gpio.h>

namespace config {

// --- Wi-Fi portal ---
constexpr char kPortalApName[] = "PlaneRadar-Setup";
constexpr char kPortalIp[] = "192.168.4.1";
/** mDNS host (no ".local" suffix); browser: http://plane-radar.local */
constexpr char kPortalHostname[] = "plane-radar";
constexpr char kPortalHostUrl[] = "plane-radar.local";

/** Per-attempt STA connect wait (ms); retried kWifiConnectAttempts times. */
constexpr unsigned long kWifiConnectAttemptMs = 15000;
constexpr uint8_t kWifiConnectAttempts = 3;
constexpr unsigned long kWifiPortalTimeoutSec = 0;  // 0 = no timeout while configuring
constexpr unsigned long kWifiConnectingFrameMs = 50;
/** Wait after disconnect before reconnecting (avoids portal on brief drops). */
constexpr unsigned long kWifiDownGraceMs = 4000;
/** Minimum interval between background reconnect tries. */
constexpr unsigned long kWifiReconnectIntervalMs = 15000;

#if defined(ARDUINO_WAVESHARE_ESP32_S3_TOUCH_LCD_2_1)

// --- BOOT button (Waveshare ESP32-S3-Touch-LCD-2.1, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_0;

// --- Display: ST7701 2.1" round 480×480 over the S3 RGB bus ---
// Pins from the Waveshare hardware table, cross-checked against two other
// radar ports for this board.
constexpr int kDisplayWidth = 480;
constexpr int kDisplayHeight = 480;

// ST7701 init-command SPI (write only, CS lives on the TCA9554 expander).
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_2;  // LCD_SCL
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_1;  // LCD_SDA

// RGB timing + 16-bit data bus.
constexpr gpio_num_t kDisplayPinHsync = GPIO_NUM_38;
constexpr gpio_num_t kDisplayPinVsync = GPIO_NUM_39;
constexpr gpio_num_t kDisplayPinDe = GPIO_NUM_40;
constexpr gpio_num_t kDisplayPinPclk = GPIO_NUM_41;
constexpr gpio_num_t kDisplayPinD0 = GPIO_NUM_5;
constexpr gpio_num_t kDisplayPinD1 = GPIO_NUM_45;
constexpr gpio_num_t kDisplayPinD2 = GPIO_NUM_48;
constexpr gpio_num_t kDisplayPinD3 = GPIO_NUM_47;
constexpr gpio_num_t kDisplayPinD4 = GPIO_NUM_21;
constexpr gpio_num_t kDisplayPinD5 = GPIO_NUM_14;
constexpr gpio_num_t kDisplayPinD6 = GPIO_NUM_13;
constexpr gpio_num_t kDisplayPinD7 = GPIO_NUM_12;
constexpr gpio_num_t kDisplayPinD8 = GPIO_NUM_11;
constexpr gpio_num_t kDisplayPinD9 = GPIO_NUM_10;
constexpr gpio_num_t kDisplayPinD10 = GPIO_NUM_9;
constexpr gpio_num_t kDisplayPinD11 = GPIO_NUM_46;
constexpr gpio_num_t kDisplayPinD12 = GPIO_NUM_3;
constexpr gpio_num_t kDisplayPinD13 = GPIO_NUM_8;
constexpr gpio_num_t kDisplayPinD14 = GPIO_NUM_18;
constexpr gpio_num_t kDisplayPinD15 = GPIO_NUM_17;

// ponytail: 16 MHz pclk works for the Selbyl port; if the frame shifts or
// tears, drop to 8 MHz with porches 8/50/10 + 8/20/10 (hackra76 calibration).
constexpr uint32_t kDisplayRgbPclkHz = 16000000;
constexpr int kDisplayHsyncPulseWidth = 8;
constexpr int kDisplayHsyncBackPorch = 10;
constexpr int kDisplayHsyncFrontPorch = 50;
constexpr int kDisplayVsyncPulseWidth = 3;
constexpr int kDisplayVsyncBackPorch = 8;
constexpr int kDisplayVsyncFrontPorch = 8;
constexpr bool kDisplayInvert = false;
constexpr bool kDisplayRgbOrder = false;

// Shared I2C bus: TCA9554 expander, CST820 touch, RTC, IMU.
constexpr gpio_num_t kI2cSclPin = GPIO_NUM_7;
constexpr gpio_num_t kI2cSdaPin = GPIO_NUM_15;
constexpr uint32_t kI2cFreqHz = 400000;

// TCA9554 expander: EXIO numbering as in the Waveshare docs (1-based).
constexpr uint8_t kTca9554Address = 0x20;
constexpr uint8_t kTca9554OutputReg = 0x01;
constexpr uint8_t kTca9554ConfigReg = 0x03;
constexpr uint8_t kTca9554LcdResetPin = 1;    // EXIO1
constexpr uint8_t kTca9554TouchResetPin = 2;  // EXIO2
constexpr uint8_t kTca9554LcdCsPin = 3;       // EXIO3
constexpr uint8_t kTca9554SdCsPin = 4;        // EXIO4
constexpr uint8_t kTca9554BuzzerPin = 8;      // EXIO8

constexpr gpio_num_t kDisplayBacklightPin = GPIO_NUM_6;
constexpr uint32_t kDisplayBacklightPwmHz = 20000;

#else

// --- BOOT button (ESP32-C3 Super Mini, active LOW) ---
constexpr gpio_num_t kBootPin = GPIO_NUM_9;

// --- Display: GC9A01 1.28" round 240×240 (SPI) ---
constexpr gpio_num_t kDisplayPinRst = GPIO_NUM_0;
constexpr gpio_num_t kDisplayPinCs = GPIO_NUM_1;
constexpr gpio_num_t kDisplayPinDc = GPIO_NUM_10;
constexpr gpio_num_t kDisplayPinMosi = GPIO_NUM_3;  // display SDA
constexpr gpio_num_t kDisplayPinSclk = GPIO_NUM_4;  // display SCL

constexpr int kDisplayWidth = 240;
constexpr int kDisplayHeight = 240;

constexpr uint32_t kDisplaySpiWriteHz = 40000000;
// GC9A01 modules often need invert + BGR for correct black/green output
constexpr bool kDisplayInvert = true;
constexpr bool kDisplayRgbOrder = true;

#endif

constexpr unsigned long kBootResetHoldMs = 3000UL;
/** Ignore BOOT taps shorter than this (debounce). */
constexpr unsigned long kBootTapMinMs = 40UL;

// --- Radar center defaults (overridden via WiFi setup portal) ---
constexpr double kDefaultRadarLat = 52.3676;
constexpr double kDefaultRadarLon = 4.9041;

/** Poll adsb.fi (API public limit: 1 req/s). */
constexpr unsigned long kAdsbFetchIntervalMs = 3000;
/** false = hide aircraft with alt_baro "ground"; true = show them too. */
constexpr bool kAdsbShowGroundAircraft = false;
/** Flash the center dot green for kCenterBlinkMs after each successful fetch. */
constexpr bool kCenterBlinkOnRefresh = true;
constexpr unsigned long kCenterBlinkMs = 150;

// --- UI colors (RGB565) — status screens ---
constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kTextOnYellow = kColorBlack;
constexpr uint16_t kTextOnBlack = 0xFFFF;

}  // namespace config
