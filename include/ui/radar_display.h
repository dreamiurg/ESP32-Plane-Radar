#pragma once

namespace ui {

/** Draw the static sonar/radar grid (black disc, green overlay, labels). */
void radarDisplayDraw();

/** Redraw aircraft only (blits cached grid; no full-screen clear). */
void radarDisplayRefreshAircraft();

/** Paint the center dot red: last fetch failed or WiFi is down. Cleared by the
 *  next successful refresh (which repaints the frame and blinks green). */
void radarDisplayMarkFault();

/** Call from loop(): restores the center dot after its refresh blink. */
void radarDisplayBlinkTick();

}  // namespace ui
