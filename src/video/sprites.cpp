// S21: draw the 8 hardware sprites on top of the tile layer. Pure function of sprite RAM, the sprite coordinate registers and the decoded graphics.
// Declaration (the step's file list has no header; tests/test_sprites.cpp repeats it):
//   void render_sprites(const Gfx &g, const uint8_t *lookup_prom /*82s126.4a, 256 bytes*/, const uint8_t *spriteram /*16 bytes, 4FF0-4FFF*/,
//                       const uint8_t *coords /*16 bytes, 5060-506F*/, uint32_t *out /*224*288 picture, drawn over*/, bool mame_rules);
// Rules (docs/notes/hardware_facts.md section 4; MAME pacman_v.cpp draw_sprites):
//  - sprite n uses spriteram[2n] (bits 2-7 = sprite number, bit 1 = flip Y, bit 0 = flip X, native picture), spriteram[2n+1] (palette = bits 0-4),
//    coords[2n] (y) and coords[2n+1] (x): native position sx = 272 - coords[2n+1], sy = coords[2n] - 31.
//  - drawing order 7, 6, ..., 0 so that a lower number is on top.
//  mame_rules = true  (the real board): sprites 0, 1, 2 are drawn one pixel further along sy (xoffsethack); a second copy at sx - 256 (wrap-around);
//                     clip to native x 16..271, y 0..223; a pixel is transparent when the palette look-up PROM gives colour 0 for it (MAME transpen_mask).
//  mame_rules = false (prototype-compatible, used to prove the drawing code equals tools/analysis/render.py): none of those four; pixel value 0 is
//                     transparent; clip only to the 288 x 224 native picture.
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include "gfx.h"

void render_sprites(const Gfx &g, const uint8_t *lookup, const uint8_t *spriteram, const uint8_t *coords, uint32_t *out, bool mame_rules) {
    const int xmin = mame_rules ? 16 : 0, xmax = mame_rules ? 271 : 287, ymin = 0, ymax = 223;
    for (int n = 7; n >= 0; --n) {
        const int code = spriteram[2 * n] >> 2;
        const bool fx = spriteram[2 * n] & 1, fy = spriteram[2 * n] & 2;
        const int pal = spriteram[2 * n + 1] & 0x1F;
        const int sx = 272 - coords[2 * n + 1];
        const int sy = coords[2 * n] - 31 + ((mame_rules && n <= 2) ? 1 : 0);
        for (int copy = 0; copy < (mame_rules ? 2 : 1); ++copy) {
            const int ox = sx - 256 * copy;
            for (int yy = 0; yy < 16; ++yy)
                for (int xx = 0; xx < 16; ++xx) {
                    const int v = g.sprites[code][fy ? 15 - yy : yy][fx ? 15 - xx : xx];
                    if (mame_rules ? (lookup[pal * 4 + v] & 0x0F) == 0 : v == 0) continue;
                    const int X = ox + xx, Y = sy + yy;
                    if (X < xmin || X > xmax || Y < ymin || Y > ymax) continue;
                    const uint8_t *c = g.palettes[pal][v];
                    out[X * 224 + (223 - Y)] = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
                }
        }
    }
}

// 2.1.0 (higher frame rates): the same sprites drawn between two machine ticks. DISPLAY ONLY: it reads the sprite bytes of the previous and of the current tick and draws
// each sprite at a position between the two (alpha 0 = previous tick, 1 = current tick). The machine is not touched and the tick-rate pictures (render_sprites above, used at
// 60 and 30 frames per second, in Purist mode and for every test hash) are not changed by it.
//  - a sprite is moved smoothly only when it is the same picture in both ticks (shape, flips, palette equal) and moved at most 8 pixels; otherwise (appearing, changing,
//    the jump through the side tunnel) it is drawn at its current place, exactly like render_sprites.
//  - between pixels the sprite is spread over the neighbouring pixels in proportion (the same as a linear texture filter); at a whole-pixel position the result is
//    pixel for pixel the picture render_sprites draws.
void render_sprites_lerp(const Gfx &g, const uint8_t *lookup, const uint8_t *ram_prev, const uint8_t *crd_prev, const uint8_t *ram_cur, const uint8_t *crd_cur, float alpha,
                         uint32_t *out, bool mame_rules) {
    const int xmin = mame_rules ? 16 : 0, xmax = mame_rules ? 271 : 287, ymin = 0, ymax = 223;
    if (alpha < 0.f) alpha = 0.f;
    if (alpha > 1.f) alpha = 1.f;
    for (int n = 7; n >= 0; --n) {
        const int code = ram_cur[2 * n] >> 2;
        const bool fx = ram_cur[2 * n] & 1, fy = ram_cur[2 * n] & 2;
        const int pal = ram_cur[2 * n + 1] & 0x1F;
        const int hack = (mame_rules && n <= 2) ? 1 : 0;
        const int sx1 = 272 - crd_cur[2 * n + 1], sy1 = crd_cur[2 * n] - 31 + hack;
        const int sx0 = 272 - crd_prev[2 * n + 1], sy0 = crd_prev[2 * n] - 31 + hack;
        float px = (float)sx1, py = (float)sy1;
        if (ram_prev[2 * n] == ram_cur[2 * n] && ram_prev[2 * n + 1] == ram_cur[2 * n + 1] && std::abs(sx1 - sx0) <= 8 && std::abs(sy1 - sy0) <= 8) {
            px = (float)sx0 + alpha * (float)(sx1 - sx0);
            py = (float)sy0 + alpha * (float)(sy1 - sy0);
        }
        int ix = (int)std::floor(px), iy = (int)std::floor(py);
        float wx = px - (float)ix, wy = py - (float)iy;
        if (wx < 1.f / 512) wx = 0.f;
        if (wy < 1.f / 512) wy = 0.f;
        if (wx > 1.f - 1.f / 512) { wx = 0.f; ++ix; }
        if (wy > 1.f - 1.f / 512) { wy = 0.f; ++iy; }
        const float w00 = (1 - wx) * (1 - wy), w10 = wx * (1 - wy), w01 = (1 - wx) * wy, w11 = wx * wy;
        for (int copy = 0; copy < (mame_rules ? 2 : 1); ++copy) {
            const int ox = ix - 256 * copy;
            float acc[17 + 1][17 + 1][4] = {};
            for (int yy = 0; yy < 16; ++yy)
                for (int xx = 0; xx < 16; ++xx) {
                    const int v = g.sprites[code][fy ? 15 - yy : yy][fx ? 15 - xx : xx];
                    if (mame_rules ? (lookup[pal * 4 + v] & 0x0F) == 0 : v == 0) continue;
                    const uint8_t *c = g.palettes[pal][v];
                    const float r = c[0], gg = c[1], b = c[2];
                    const float wv[4] = {w00, w10, w01, w11};
                    const int dx[4] = {0, 1, 0, 1}, dy[4] = {0, 0, 1, 1};
                    for (int k = 0; k < 4; ++k) {
                        if (wv[k] <= 0.f) continue;
                        float *a = acc[xx + dx[k]][yy + dy[k]];
                        a[0] += r * wv[k];
                        a[1] += gg * wv[k];
                        a[2] += b * wv[k];
                        a[3] += wv[k];
                    }
                }
            for (int xx = 0; xx < 17; ++xx)
                for (int yy = 0; yy < 17; ++yy) {
                    const float *a = acc[xx][yy];
                    if (a[3] <= 0.f) continue;
                    const int X = ox + xx, Y = iy + yy;
                    if (X < xmin || X > xmax || Y < ymin || Y > ymax) continue;
                    uint32_t &d = out[X * 224 + (223 - Y)];
                    const float cov = a[3] > 1.f ? 1.f : a[3];
                    const float inv = 1.f - cov;
                    const float k = a[3] > 1.f ? 1.f / a[3] : 1.f;
                    const float r = a[0] * k + (float)((d >> 16) & 255) * inv, gg = a[1] * k + (float)((d >> 8) & 255) * inv, b = a[2] * k + (float)(d & 255) * inv;
                    d = (uint32_t)(r + 0.5f) << 16 | (uint32_t)(gg + 0.5f) << 8 | (uint32_t)(b + 0.5f);
                }
        }
    }
}
