// S31: display and pacing logic that does not touch the emulation: where the picture goes in the window (whole-number scaling or fill), how many display
// refreshes make one arcade frame (smoothed pacing), pacing statistics, and a hash of the machine's memory used to prove that no display option changes
// the emulation. No SDL in here: main.cpp does the drawing.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../board/bus.h"

namespace display {
struct Rect {
    int x, y, w, h;
};
enum class Fit { Integer, Fill }; // Integer: whole-number scale only (every arcade pixel the same size); Fill: as large as the window allows, aspect kept
struct Layout {
    Rect dest;
    double scale;
    bool integer_scale;
};

// Where a content_w x content_h picture goes in a win_w x win_h window (centred, black bars). Integer falls back to Fill when the window is smaller than 1x.
Layout compute_layout(int win_w, int win_h, int content_w, int content_h, Fit fit);

// Smoothed pacing: the number N of display refreshes that make one arcade frame, so that the machine ticks exactly once every N refreshes (every picture is
// then shown for the same time). Returns 0 when no whole N brings the tick rate within max_dev (e.g. 0.03 = 3 %) of tick_hz. *resulting_hz = refresh_hz / N.
int refreshes_per_tick(double refresh_hz, double tick_hz, double max_dev, double *resulting_hz);

// Pacing statistics: for every presented picture, how many display refreshes it stayed on screen, and the time between presents.
struct PacingStats {
    uint64_t presents = 0, ticks = 0, pictures = 0;
    int run = 0;
    uint64_t hist[10] = {0};       // hist[n] = pictures shown for n refreshes (9 = 9 or more)
    double dt_sum = 0, dt_max = 0; // milliseconds between presents
    void present(bool new_picture, double dt_ms);
    std::string report() const;
};

// FNV-1a over video RAM, colour RAM, work RAM and sprite RAM: two runs with the same hash ended in the same machine state.
uint64_t memory_hash(const Bus &bus);
} // namespace display
