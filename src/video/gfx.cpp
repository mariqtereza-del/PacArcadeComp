// gfx.cpp: decodes the graphics ROMs and colour PROMs into plain pixel arrays (see gfx.h).
#include "gfx.h"
#include <cstring>

static int bit(const uint8_t *p, unsigned off) { return (p[off >> 3] >> (7 - (off & 7))) & 1; } // MAME numbering: bit 0 = MSB of byte 0

bool decode_gfx(const RomSet &r, Gfx &g) {
    std::memset(&g, 0, sizeof g);
    if (r.tiles.size() != 4096 || r.sprites.size() != 4096 || r.colours.size() != 32 || r.palettes.size() != 256) return false;
    // Tiles: 8x8, 2 bits per pixel, 16 bytes each. MAME tilelayout: planes {0, 4}, x offsets {64+0..3, 0..3}, y offsets 0,8,..56, char increment 128 bits.
    static const unsigned txo[8] = {64, 65, 66, 67, 0, 1, 2, 3};
    for (int c = 0; c < 256; ++c) {
        const uint8_t *t = &r.tiles[c * 16];
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                unsigned off = txo[x] + y * 8;
                g.tiles[c][y][x] = (uint8_t)(bit(t, off) * 2 + bit(t, off + 4)); // plane 0 (offset 0) is the high bit, plane 1 (offset 4) the low bit
            }
    }
    // Sprites: 16x16, 2 bits per pixel, 64 bytes each. MAME spritelayout: planes {0, 4}, x offsets {64+0..3, 128+0..3, 192+0..3, 0..3},
    // y offsets 0,8,..56 then 256+0,8,..56, char increment 512 bits.
    static const unsigned sxo[16] = {64, 65, 66, 67, 128, 129, 130, 131, 192, 193, 194, 195, 0, 1, 2, 3};
    for (int c = 0; c < 64; ++c) {
        const uint8_t *s = &r.sprites[c * 64];
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                unsigned yo = y < 8 ? y * 8 : 256 + (y - 8) * 8;
                unsigned off = sxo[x] + yo;
                g.sprites[c][y][x] = (uint8_t)(bit(s, off) * 2 + bit(s, off + 4));
            }
    }
    // Colour PROM 82s123.7f: bits 0-2 red, 3-5 green (weights 0x21, 0x47, 0x97), bits 6-7 blue (weights 0x51, 0xAE).
    for (int i = 0; i < 32; ++i) {
        unsigned b = r.colours[i];
        g.colours[i][0] = (uint8_t)((b & 1) * 0x21 + ((b >> 1) & 1) * 0x47 + ((b >> 2) & 1) * 0x97);
        g.colours[i][1] = (uint8_t)(((b >> 3) & 1) * 0x21 + ((b >> 4) & 1) * 0x47 + ((b >> 5) & 1) * 0x97);
        g.colours[i][2] = (uint8_t)(((b >> 6) & 1) * 0x51 + ((b >> 7) & 1) * 0xAE);
    }
    // Look-up PROM 82s126.4a: 4 bits per entry (prom & 0x0F), 64 palettes x 4 entries.
    for (int p = 0; p < 64; ++p)
        for (int i = 0; i < 4; ++i) std::memcpy(g.palettes[p][i], g.colours[r.palettes[p * 4 + i] & 0x0F], 3);
    return true;
}
