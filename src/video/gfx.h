// S19: decode the graphics ROMs into plain pixel arrays. Pure data: no drawing, no screen.
// Layouts follow MAME pacman.cpp tilelayout/spritelayout (docs/notes/hardware_facts.md section 4); colour PROM resistor weights as in section 4.
#pragma once
#include <cstdint>
#include "../board/romset.h"

struct Gfx {
    uint8_t tiles[256][8][8];    // [code][y][x], value 0-3 (pacman.5e)
    uint8_t sprites[64][16][16]; // [code][y][x], value 0-3 (pacman.5f); the sprite number in sprite RAM is byte0 >> 2
    uint8_t colours[32][3];      // 82s123.7f decoded to R, G, B
    uint8_t palettes[64][4][3];  // 82s126.4a: 64 palettes of 4 entries, each decoded to R, G, B through `colours` (index = PROM byte & 0x0F)
};

// Fills `out` from a verified ROM set. Returns false only if a ROM part has the wrong size.
bool decode_gfx(const RomSet &roms, Gfx &out);
