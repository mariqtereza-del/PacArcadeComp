// display.cpp: the display and pacing calculations declared in display.h (picture placement, smoothed pacing, frame statistics). No emulation here.
#include "display.h"
#include <cmath>
#include <cstdio>

namespace display {

Layout compute_layout(int win_w, int win_h, int cw, int ch, Fit fit) {
    Layout l{};
    if (win_w < 1) win_w = 1;
    if (win_h < 1) win_h = 1;
    double s = std::fmin((double)win_w / cw, (double)win_h / ch);
    if (fit == Fit::Integer && s >= 1.0) {
        s = std::floor(s);
        l.integer_scale = true;
    } else l.integer_scale = std::fabs(s - std::round(s)) < 1e-9;
    l.scale = s;
    l.dest.w = (int)std::lround(cw * s);
    l.dest.h = (int)std::lround(ch * s);
    l.dest.x = (win_w - l.dest.w) / 2;
    l.dest.y = (win_h - l.dest.h) / 2;
    return l;
}

int refreshes_per_tick(double refresh_hz, double tick_hz, double max_dev, double *resulting_hz) {
    if (refresh_hz < 1 || tick_hz <= 0) return 0;
    int n = (int)std::lround(refresh_hz / tick_hz);
    if (n < 1) n = 1;
    double hz = refresh_hz / n;
    if (resulting_hz) *resulting_hz = hz;
    return std::fabs(hz - tick_hz) / tick_hz <= max_dev ? n : 0;
}

void PacingStats::present(bool new_picture, double dt_ms) {
    ++presents;
    dt_sum += dt_ms;
    if (dt_ms > dt_max) dt_max = dt_ms;
    if (new_picture) {
        if (run > 0) {
            ++hist[run > 9 ? 9 : run];
            ++pictures;
        }
        run = 1;
    } else ++run;
}

std::string PacingStats::report() const {
    char b[600];
    std::string s;
    std::snprintf(b, sizeof b,
                  "Pacing: %llu presents, %llu machine ticks, %llu pictures finished. Refreshes each picture stayed on screen:", (unsigned long long)presents,
                  (unsigned long long)ticks, (unsigned long long)pictures);
    s = b;
    for (int n = 1; n < 10; ++n)
        if (hist[n]) {
            std::snprintf(b, sizeof b, " %d x %llu", n, (unsigned long long)hist[n]);
            s += b;
        }
    std::snprintf(b, sizeof b, "\nPacing: time between presents average %.3f ms, longest %.3f ms", presents ? dt_sum / presents : 0.0, dt_max);
    return s + b;
}

uint64_t memory_hash(const Bus &bus) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const uint8_t *p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    };
    mix(bus.videoram, 0x400);
    mix(bus.colorram, 0x400);
    mix(bus.workram, 0x3F0);
    mix(bus.spriteram, 0x10);
    return h;
}
} // namespace display
