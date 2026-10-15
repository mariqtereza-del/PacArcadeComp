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
#include <cstdint>
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
