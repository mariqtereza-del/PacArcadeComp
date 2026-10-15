// S20: draw the tile layer (no sprites) into a 224x288 picture. Pure function of video RAM, colour RAM and the decoded graphics.
// Declaration (the step's file list has no header; tests/test_tilemap.cpp repeats it):
//   void render_tilemap(const Gfx &g, const uint8_t *videoram /*0x400*/, const uint8_t *colorram /*0x400*/, uint32_t *out /*224*288, 0x00RRGGBB, row by row*/);
#include <cstdint>
#include "gfx.h"

// Native (unrotated) tile position -> video RAM offset: the board's odd addressing (hardware_facts.md section 1 "tile scan", MAME pacman_scan_rows):
// 36 columns x 28 rows (native, unrotated). Columns 2-33 are the main 32 x 28 area stored in rows of 32 bytes; the two bottom and two top
// rows of the picture (native columns 0-1 and 34-35) are the 2 x 28 byte blocks at the end of video RAM, which the bit-5 test selects.
static int tile_offset(int col, int row) {
    row += 2;
    col -= 2;
    if (col & 0x20) return row + ((col & 0x1f) << 5);
    return col + (row << 5);
}

void render_tilemap(const Gfx &g, const uint8_t *videoram, const uint8_t *colorram, uint32_t *out) {
    // The monitor is mounted on its side: the native picture is 288 wide x 224 tall and is shown turned 90 degrees clockwise,
    // so native pixel (X, Y) lands at (x = 223 - Y, y = X) in the 224 x 288 picture (hardware_facts.md section 1, native picture rotated).
    for (int row = 0; row < 28; ++row)
        for (int col = 0; col < 36; ++col) {
            int o = tile_offset(col, row);
            uint8_t code = videoram[o];
            const uint8_t(*pal)[3] = g.palettes[colorram[o] & 0x1F]; // tile palette = colour RAM & 0x1F (hardware_facts.md section 4)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x) {
                    const uint8_t *c = pal[g.tiles[code][y][x]];
                    int X = col * 8 + x, Y = row * 8 + y;
                    out[X * 224 + (223 - Y)] = (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
                }
        }
}
