// S30: the debug overlay (F3 in a later step wires it into the window). It only READS the machine's memory (const Bus) and draws on a COPY of the
// picture, in a larger buffer: the emulated picture (224 x 288) on the left and a text panel on the right, so nothing of the emulated picture is
// covered except small outline markers on the copy. It can never change the emulation: it has no access to anything it could write.
// The ghost targets are not stored in RAM (the ROM computes them in a register when it picks a direction), so the overlay recomputes them from RAM
// with the rules that tests/test_behaviour_b.cpp verified against the running ROM (scatter corners, Blinky, Pinky, Inky, Clyde).
// Tiles: a tile is a byte pair (E, D): row = E - 32, column = 59 - D; the picture is 28 columns x 36 rows of 8 pixels.
#pragma once
#include <cstdint>
#include "../board/bus.h"
#include "../video/gfx.h"

namespace overlay {
const int kPictureW = 224, kPictureH = 288, kPanelW = 128, kOutW = kPictureW + kPanelW, kOutH = kPictureH;

struct GhostView {
    int col, row;    // the ghost's own tile
    int tcol, trow;  // its target tile (may lie outside the maze, even outside the screen)
    int target_kind; // 0 corner (scatter), 1 chase rule, 2 frightened (no target: random), 3 eyes or in the house (not shown)
    bool edible;     // 4DA7-4DAA
};
struct Info {
    int level;                     // 4E13 + 1
    int phase, counter, limit;     // 4DC1, 4DC2/3, the table word at 4D86 + 2*phase (0 in phase 7)
    bool chase_phase;              // odd phase
    bool pill;                     // 4DA6
    bool elroy1, elroy2;           // 4DB6, 4DB7
    int dots, state, sub;          // 4E0E, 4E00, 4E04
    int pac_col, pac_row, pac_dir; // 0 right, 1 down, 2 left, 3 up (derived from the vector 4D1C; -1 unknown)
    GhostView g[4];                // Blinky, Pinky, Inky, Clyde
};

Info read_info(const Bus &bus); // pure read of work RAM
// picture: kPictureW x kPictureH pixels 0xRRGGBB (the emulated frame, not modified). out: kOutW x kOutH. The font comes from the game's own tile ROM.
void compose(const Gfx &gfx, const Bus &bus, const uint32_t *picture, uint32_t *out);
} // namespace overlay
