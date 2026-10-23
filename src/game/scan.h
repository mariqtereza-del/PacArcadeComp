// N19: the video-RAM scan loop that is interrupted most often during play, written natively (no Z80). ROM routine replaced (entries):
//   2A35  walk over the tiles 4040h to 43BFh: every dot (10h), power pill (12h) or 14h tile is replaced by the blank tile 40h; loop head 2A38 (where the ROM is after a cut)
// A round is one tile: 108 T-states / 14 fetches when the tile is left alone, 88 / 12, 105 / 14, 122 / 16 when it is replaced (found at the 1st, 2nd, 3rd comparison);
// the end (DE = 43C0h): 40 T-states / 5 fetches and RET. The rounds that end before the frame limit are run natively (N17 mechanism), see src/game/maze.h.
// No ROM bytes. Mem: rd, wr.
#pragma once
#include <cstdint>
#include "helpers.h"
#include "maze.h"

namespace game {

constexpr uint16_t kScanEnd = 0x43C0;

// Loop head 2A38: DE = the tile address. Runs whole rounds that end before `room`; the final ret z is done too when it fits.
template <class Mem> inline LoopResult scan_loop_2a38(Mem &mem, Regs &r, unsigned room, Cost &cost) {
    LoopResult res;
    for (;;) {
        const uint16_t de = r.de();
        if (de == kScanEnd) {
            if (cost.cycles + 40 >= room) return res;
            uint16_t t;
            r.f = flag::and_a(r.a);
            r.f = flag::sbc16(kScanEnd, de, 0, &t);
            r.set_hl(t);
            cost.add(40, 5);
            res.finished = true;
            return res;
        }
        const uint8_t tile = mem.rd(de);
        const unsigned k = tile == 0x10 ? 1 : tile == 0x12 ? 2 : tile == 0x14 ? 3 : 0;
        const unsigned cycles = k ? 71 + 17 * k : 108, fetches = k ? 10 + 2 * k : 14;
        if (cost.cycles + cycles >= room) return res;
        uint16_t t;
        r.f = flag::sbc16(kScanEnd, de, 0, &t);
        r.set_hl(t);
        r.a = tile;
        r.f = flag::cp8(tile, k ? tile : 0x14);
        if (k) {
            r.a = 0x40;
            mem.wr(de, 0x40);
        }
        r.set_de((uint16_t)(de + 1));
        cost.add(cycles, fetches);
        ++res.units;
        res.wz = 0x2A38;
    }
}

} // namespace game
