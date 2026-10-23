// N24: the player labels and the bonus-ghost sprite positions of the interrupt handler, written natively (no Z80). ROM routines replaced (entries):
//   0369  writes "1UP" tiles' bytes 50 55 31 through IX+0..2     0376  "2UP" bytes 50 55 32 through IY+0..2
//   0383  blanks the 1UP label (40 40 40) through IX             0390  blanks the 2UP label (40 40 40) through IY
//         (N22 made 02FD, the blink routine, which calls these four, native; here they are also native when something else (the ROM) calls them)
//   039D  from level 5 on (4E06 >= 5) sets the six sprite position bytes 4D02..4D07 and the two copies 4DD2 / 4DD3 from the position word at 4D08
// Every instruction is accounted for (T-states, opcode fetches, flags, WZ). No ROM bytes. Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "sprite_pics.h"

namespace game {
namespace lab {
using spp::Z;
using spp::Result;

// the four label writers: three LD (IX/IY+d),n (19 T-states, 2 fetches each) and RET (10)
template <class Mem> inline Result label_writer(Z<Mem> &z, int which) {
    static const uint8_t vals[4][3] = {{0x50, 0x55, 0x31}, {0x50, 0x55, 0x32}, {0x40, 0x40, 0x40}, {0x40, 0x40, 0x40}};
    const uint16_t base = (which == 1 || which == 3) ? z.iy : z.ix;
    for (int i = 0; i < 3; ++i) { z.mem.wr((uint16_t)(base + i), vals[which][i]); z.t(19, 2); }
    z.wz = (uint16_t)(base + 2);
    return z.ret(10);
}

template <class Mem> inline Result bonus_039d(Z<Mem> &z) {
    z.ld_a_nn(ram::byte_4E06); z.sub_n(5);
    if (z.f & flag::C) return z.ret(11);                          // RET C
    z.t(5);
    z.l = z.mem.rd(ram::pacman_pixel); z.h = z.mem.rd((ram::pacman_pixel + 1)); z.wz = (ram::pacman_pixel + 1); z.t(16);   // LD HL,(4D08)
    z.b = 8; z.t(7); z.c = 0x10; z.t(7);
    z.a = z.l; z.t(4);
    z.ld_nn_a(ram::clyde_pixel); z.ld_nn_a(ram::fruit_position);
    { uint8_t r; z.f = flag::sub8(z.a, z.c, &r); z.a = r; z.t(4); }
    z.ld_nn_a(ram::pinky_pixel); z.ld_nn_a(ram::inky_pixel);
    z.a = z.h; z.t(4);
    z.add_a(z.b);
    z.ld_nn_a((ram::pinky_pixel + 1)); z.ld_nn_a((ram::clyde_pixel + 1));
    { uint8_t r; z.f = flag::sub8(z.a, z.c, &r); z.a = r; z.t(4); }
    z.ld_nn_a((ram::inky_pixel + 1)); z.ld_nn_a((ram::fruit_position + 1));
    return z.ret(10);
}

} // namespace lab
} // namespace game
