#pragma once

namespace ui {

/** Draw the static sonar/radar grid (black disc, green overlay, labels). */
void radarDisplayDraw();

/** Redraw aircraft only (blits cached grid; no full-screen clear). */
void radarDisplayRefreshAircraft();

/** Call from loop(): restores the center dot after its refresh blink. */
void radarDisplayBlinkTick();

}  // namespace ui
