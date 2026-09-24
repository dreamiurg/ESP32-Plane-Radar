/**
 * Plane Radar — WiFi setup, then radar UI on the round display (GC9A01 or ST7701).
 */

#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "hardware/display.h"
#include "hardware/touch.h"
#include "services/adsb_client.h"
#include "services/radar_location.h"
#include "services/wifi_setup.h"
#include "ui/radar_display.h"
#include "ui/radar_range.h"
#include "ui/status_screens.h"

namespace {

bool g_radar_visible = false;
unsigned long g_wifi_down_since = 0;
unsigned long g_last_reconnect_ms = 0;
unsigned long g_last_adsb_fetch_ms = 0;
/** Tap seen while a fetch was blocking; acted on from loop(). */
bool g_tap_pending = false;

void showRadarIfConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    g_radar_visible = false;
    return;
  }
  ui::radarDisplayDraw();
  g_radar_visible = true;
}

void onRangeTap() {
  ui::radar::rangeNext();
  char range_label[12];
  ui::radar::formatCurrentRing3Label(range_label, sizeof(range_label));
  Serial.printf("Range: %s (outer ~%.0f km)\n", range_label,
                ui::radar::rangeCurrent().outer_km);

  if (g_radar_visible && WiFi.status() == WL_CONNECTED) {
    ui::radarDisplayDraw();
  }
}

void handleBootButton() {
  bootButtonPollLongPress();
  if (bootButtonConsumeTap()) {
    onRangeTap();
  }
}

/** Touch anywhere acts like a BOOT tap: cycle the range preset. */
void handleTouch() {
  hardware::touch::Point p{};
  if (hardware::touch::readTap(&p)) {
    g_tap_pending = true;
  }
  if (g_tap_pending) {
    g_tap_pending = false;
    onRangeTap();
  }
}

/** Runs while a fetch blocks: keep WiFi alive and latch taps for later. */
void pollDuringFetch() {
  wifiLoop();
  hardware::touch::Point p{};
  if (hardware::touch::readTap(&p)) {
    g_tap_pending = true;
  }
}

void fetchAndDrawAircraft() {
  const float fetch_km = ui::radar::fetchRadiusKm();
  if (!services::adsb::fetchUpdate(services::location::lat(),
                                   services::location::lon(), fetch_km)) {
    ui::radarDisplayMarkFault();
    handleBootButton();
    return;
  }
  ui::radarDisplayRefreshAircraft();
  handleBootButton();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Plane Radar");

  bootButtonInit();
  displayInit();
  hardware::touch::init();
  if (wifiShowsSetupScreenOnBoot()) {
    statusScreenPortal();
  }
  services::location::init();
  ui::radar::rangeInit();
  services::adsb::setPollFn(pollDuringFetch);

  if (wifiSetupConnect()) {
    showRadarIfConnected();
  }
}

void loop() {
  handleBootButton();
  handleTouch();
  wifiLoop();
  ui::radarDisplayBlinkTick();

  if (WiFi.status() != WL_CONNECTED) {
    if (g_radar_visible) {
      Serial.println("WiFi lost — will reconnect");
      ui::radarDisplayMarkFault();
      g_radar_visible = false;
    }

    if (g_wifi_down_since == 0) {
      g_wifi_down_since = millis();
    }

    const unsigned long down_ms = millis() - g_wifi_down_since;
    if (down_ms >= config::kWifiDownGraceMs &&
        millis() - g_last_reconnect_ms >= config::kWifiReconnectIntervalMs) {
      g_last_reconnect_ms = millis();
      if (wifiReconnect()) {
        g_wifi_down_since = 0;
        showRadarIfConnected();
      }
    }
  } else {
    g_wifi_down_since = 0;
    if (!g_radar_visible) {
      showRadarIfConnected();
    } else if (millis() - g_last_adsb_fetch_ms >= config::kAdsbFetchIntervalMs) {
      g_last_adsb_fetch_ms = millis();  // fixed-rate: interval includes fetch time
      fetchAndDrawAircraft();
    }
  }

  delay(10);
}
