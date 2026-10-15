// X40: CRT effect, shadow mask. A phosphor shadow mask is a fine pattern of red, green and blue stripes (one triad every 3 screen pixels) that is laid over the picture at the real size of the window.
// The mask is a picture of its own (never the emulated picture buffer): it is drawn over the already scaled picture and multiplies it (SDL blend mode MOD), column by column:
//   column x % 3 == 0: (255, k, k)  red stripe   |  x % 3 == 1: (k, 255, k)  green stripe  |  x % 3 == 2: (k, k, 255)  blue stripe
// k = 215 for Light and 150 for Strong, so the picture is on average (255 + 2k) / 3 / 255 = 89 % (Light) or 72.5 % (Strong) as bright. Purist mode never draws it.
#pragma once
#include <cstdint>
#include <cstdlib>
#include <vector>
#include <algorithm>

namespace crt {

enum Level { kOff = 0, kLight = 1, kStrong = 2 };
inline const char *name(int l) { return l == kLight ? "light" : l == kStrong ? "strong" : "off"; }
inline int from_name(const char *s) { return !s ? 0 : s[0] == 'l' ? kLight : s[0] == 's' ? kStrong : 0; }
inline int dim(int level) { return level == kLight ? 215 : level == kStrong ? 150 : 255; } // k, the level of the two weaker colours in a stripe

// The multiplier of the picture at column x (ARGB, alpha 255).
inline uint32_t pixel(int x, int level) {
    const unsigned k = (unsigned)dim(level), m = 255;
    const unsigned r = (x % 3 == 0) ? m : k, g = (x % 3 == 1) ? m : k, b = (x % 3 == 2) ? m : k;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}
// A whole mask of w x h pixels (row after row; every row is the same: vertical stripes).
inline void fill(uint32_t *out, int w, int h, int level) {
    for (int x = 0; x < w; ++x) {
        const uint32_t p = pixel(x, level);
        for (int y = 0; y < h; ++y) out[(size_t)y * w + x] = p;
    }
}
// Average brightness of the mask, 0..1 (the share of light that passes).
inline double average(int level) { return (255.0 + 2.0 * dim(level)) / 3.0 / 255.0; }

// X41: glow (bloom). The bright part of the picture (each colour above kGlowThreshold, stretched back to 0..255) is blurred (two box blurs along the rows, two along the columns,
// which together make a soft bell shape; the radius is in picture pixels) and scaled by the strength. The result is a picture of its own; the window adds it onto the scaled picture
// (SDL blend mode ADD, linear scaling), so the glow is soft at any window size. Computed from the finished picture only; the emulated picture buffer is never changed.
constexpr int kGlowThreshold = 96;
inline int glow_radius(int level) { return level == kLight ? 2 : level == kStrong ? 4 : 0; }
inline double glow_gain(int level) { return level == kLight ? 0.3 : level == kStrong ? 0.6 : 0.0; }
inline void box_pass(const float *in, float *out, int n, int stride, int r) { // one line of n values, `stride` apart; edges count as black
    const float inv = 1.0f / (2 * r + 1);
    float acc = 0;
    for (int i = -r; i <= r; ++i)
        if (i >= 0 && i < n) acc += in[(size_t)i * stride];
    for (int i = 0; i < n; ++i) {
        out[(size_t)i * stride] = acc * inv;
        const int add = i + r + 1, sub = i - r;
        if (add < n) acc += in[(size_t)add * stride];
        if (sub >= 0) acc -= in[(size_t)sub * stride];
    }
}
// src: w x h ARGB pixels, `stride` pixels per row. out: w x h ARGB (alpha 255). level 0 gives black.
inline void glow(const uint32_t *src, int stride, int w, int h, int level, uint32_t *out) {
    const int r = glow_radius(level);
    const float gain = (float)glow_gain(level);
    if (r == 0) {
        for (size_t i = 0; i < (size_t)w * h; ++i) out[i] = 0xFF000000u;
        return;
    }
    static thread_local std::vector<float> a, b;
    a.assign((size_t)w * h * 3, 0.f);
    b.assign((size_t)w * h * 3, 0.f);
    const float k = 255.0f / (255 - kGlowThreshold);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t p = src[(size_t)y * stride + x];
            const int c[3] = {(int)((p >> 16) & 255), (int)((p >> 8) & 255), (int)(p & 255)};
            for (int i = 0; i < 3; ++i) a[((size_t)y * w + x) * 3 + i] = c[i] > kGlowThreshold ? (c[i] - kGlowThreshold) * k : 0.f;
        }
    for (int ch = 0; ch < 3; ++ch)
        for (int pass = 0; pass < 2; ++pass) {
            for (int y = 0; y < h; ++y) {
                box_pass(&a[((size_t)y * w) * 3 + ch], &b[((size_t)y * w) * 3 + ch], w, 3, r);
                std::swap_ranges(b.begin() + ((size_t)y * w) * 3, b.begin() + ((size_t)(y + 1) * w) * 3, a.begin() + ((size_t)y * w) * 3);
            }
            for (int x = 0; x < w; ++x) {
                box_pass(&a[(size_t)x * 3 + ch], &b[(size_t)x * 3 + ch], h, w * 3, r);
                for (int y = 0; y < h; ++y) std::swap(a[((size_t)y * w + x) * 3 + ch], b[((size_t)y * w + x) * 3 + ch]);
            }
        }
    for (size_t i = 0; i < (size_t)w * h; ++i) {
        int v[3];
        for (int c = 0; c < 3; ++c) v[c] = std::min(255, (int)(a[i * 3 + c] * gain + 0.5f));
        out[i] = 0xFF000000u | ((uint32_t)v[0] << 16) | ((uint32_t)v[1] << 8) | (uint32_t)v[2];
    }
}

// X42: curve and vignette. The picture is drawn on a mesh of small quads whose corners are moved by curve_map() (a table, rebuilt only when the window size or the setting changes; the
// graphics card does the per-pixel work, so it costs no measurable time). p is the position in the picture from -1 to 1 on both axes, s the position on the screen in the same units:
//   s = p * (1 + k * (2 - |p|^2)) / (1 + 2k)        centre (p = 0): scale exactly 1, nothing moves; the middle of an edge (1, 0) goes to (1+k)/(1+2k); a corner (1, 1) to 1/(1+2k).
// So the picture bulges like the glass of an old tube and its corners are pulled in. curve_unmap() is the exact inverse (for the mouse). The vignette is a picture of its own that
// multiplies the screen (blend mode MOD): 1 in the middle, 1 - a at the corners (a = 0.25 Light, 0.5 Strong), growing with the square of the distance from the middle.
inline double curve_k(int level) { return level == kLight ? 0.04 : level == kStrong ? 0.09 : 0.0; }
inline double vignette_a(int level) { return level == kLight ? 0.25 : level == kStrong ? 0.5 : 0.0; }
inline void curve_map(double k, double px, double py, double *sx, double *sy) {
    const double f = (1 + k * (2 - px * px - py * py)) / (1 + 2 * k);
    *sx = px * f;
    *sy = py * f;
}
inline void curve_unmap(double k, double sx, double sy, double *px, double *py) { // fixed-point iteration: converges quickly for k <= 0.2
    double x = sx, y = sy;
    for (int i = 0; i < 60; ++i) {
        const double f = (1 + k * (2 - x * x - y * y)) / (1 + 2 * k);
        x = sx / f;
        y = sy / f;
    }
    *px = x;
    *py = y;
}
struct MeshVertex {
    float x, y, u, v;
};
// A mesh of nx * ny quads over the screen rectangle (dx, dy, dw, dh); the texture part (u0, v0)-(u1, v1) is the picture. Vertices (nx+1)*(ny+1); indices 6 per quad.
inline void build_curve_mesh(std::vector<MeshVertex> &vert, std::vector<int> &idx, int nx, int ny, double dx, double dy, double dw, double dh, double k, float u0,
                             float v0, float u1, float v1) {
    vert.clear();
    idx.clear();
    for (int j = 0; j <= ny; ++j)
        for (int i = 0; i <= nx; ++i) {
            const double px = 2.0 * i / nx - 1, py = 2.0 * j / ny - 1;
            double sx, sy;
            curve_map(k, px, py, &sx, &sy);
            vert.push_back({(float)(dx + (sx + 1) * 0.5 * dw), (float)(dy + (sy + 1) * 0.5 * dh), u0 + (u1 - u0) * (float)i / nx, v0 + (v1 - v0) * (float)j / ny});
        }
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            const int a = j * (nx + 1) + i, b = a + 1, c = a + nx + 1, d = c + 1;
            idx.insert(idx.end(), {a, b, c, b, d, c});
        }
}
inline uint32_t vignette_pixel(int x, int y, int w, int h, int level) {
    const double nx = 2.0 * (x + 0.5) / w - 1, ny = 2.0 * (y + 0.5) / h - 1, a = vignette_a(level);
    const double v = 1.0 - a * (nx * nx + ny * ny) / 2.0;
    const unsigned g = (unsigned)(255.0 * (v < 0 ? 0 : v) + 0.5);
    return 0xFF000000u | (g << 16) | (g << 8) | g;
}
inline void vignette_fill(uint32_t *out, int w, int h, int level) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) out[(size_t)y * w + x] = vignette_pixel(x, y, w, h, level);
}

// X43: presets. One table; each preset is the four CRT settings together (mask, glow, curve, scanlines). The look shown in the menu is worked out from the four settings, so the
// individual settings stay available: change any one and the look reads CUSTOM. Scanlines (F5) are only drawn while the curve is off (see X42), so the presets with a curve have them off.
struct Preset {
    const char *name;
    int mask, glow, curve;
    bool scanlines;
};
constexpr int kPresetCount = 4;
inline const Preset &preset(int i) {
    static const Preset t[kPresetCount] = {{"OFF", kOff, kOff, kOff, false},
                                           {"SUBTLE", kLight, kLight, kOff, false},
                                           {"ARCADE MONITOR", kLight, kLight, kOff, true},
                                           {"OLD TV", kStrong, kStrong, kStrong, false}};
    return t[i >= 0 && i < kPresetCount ? i : 0];
}
// The index of the preset that the four settings match exactly, or -1 (CUSTOM).
inline int preset_match(int mask, int glow, int curve, bool scanlines) {
    for (int i = 0; i < kPresetCount; ++i) {
        const Preset &p = preset(i);
        if (p.mask == mask && p.glow == glow && p.curve == curve && p.scanlines == scanlines) return i;
    }
    return -1;
}
inline const char *look_name(int mask, int glow, int curve, bool scanlines) {
    const int i = preset_match(mask, glow, curve, scanlines);
    return i < 0 ? "CUSTOM" : preset(i).name;
}
// The next look when the player turns CRT LOOK one step (dir +1 or -1). CUSTOM goes to the first preset (OFF) going on and to the last going back.
inline int preset_step(int mask, int glow, int curve, bool scanlines, int dir) {
    const int i = preset_match(mask, glow, curve, scanlines);
    if (i < 0) return dir < 0 ? kPresetCount - 1 : 0;
    return (i + (dir < 0 ? kPresetCount - 1 : 1)) % kPresetCount;
}

} // namespace crt
