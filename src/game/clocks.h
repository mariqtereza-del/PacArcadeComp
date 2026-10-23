// N22: the frame counters and clocks, the random-number refresh bytes and the 1UP / 2UP blink of the interrupt handler, written natively (no Z80). ROM routines replaced (entries):
//   01DC  the frame counter 4C84, the seconds / minutes style counters 4C85.. whose limits are the table at 0219 (read from the player's ROM through Mem at run time), the number of
//         counters that rolled over (4C8A) and the two pseudo-random refresh bytes 4C8B and 4C8C
//   02FD  the blink counter 4DCE: every 16th frame the sound/volume bytes 5004 / 5005, then the "1UP" / "2UP" letters at 43D8 and 43C5 (shown, hidden, or blinking)
// Every instruction is accounted for (T-states, opcode fetches, flags, WZ); the CALLs of 02FD leave their return address on the stack. No ROM bytes. Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "sprite_pics.h"

namespace game {
namespace clk {
using spp::Z;
using spp::Result;
using spp::kRet;

constexpr uint16_t kLimitTable = 0x0219;   // address of the limits table in the player's ROM (not its contents)

template <class Mem> inline Result counters_01dc(Z<Mem> &z) {
    z.set_hl(ram::frame_counter_up); z.t(10);
    z.inc_mem(z.hl());
    z.inc16_hl();
    z.dec_mem(z.hl());
    z.inc16_hl();
    z.set_de(kLimitTable); z.t(10);
    z.b = 0x04; z.c = 0x01; z.t(10);
    for (;;) {                                                   // 01E9
        z.inc_mem(z.hl());
        z.ld_a_mem(z.hl());
        z.and_n(0x0F);
        z.ex_de_hl();
        z.cp_mem(z.hl());
        if (z.jr(!(z.f & flag::Z), 0x0204)) break;
        z.inc_c();
        z.a = z.mem.rd(z.de()); z.wz = (uint16_t)(z.de() + 1); z.t(7);
        z.add_a(0x10, 7);
        z.and_n(0xF0);   // (AND with immediate: 7 T-states)
        z.mem.wr(z.de(), z.a); z.t(7);
        z.inc16_hl();
        z.cp_mem(z.hl());
        if (z.jr(!(z.f & flag::Z), 0x0204)) break;
        z.inc_c();
        z.ex_de_hl();
        z.ld_mem_n(z.hl(), 0);
        z.inc16_hl();
        z.inc16_de();
        z.b = (uint8_t)(z.b - 1);                                // DJNZ
        if (z.b != 0) { z.wz = 0x01E9; z.t(13); } else { z.t(8); break; }
    }
    z.set_hl(ram::byte_4C8A); z.t(10);                                    // 0204
    z.ld_mem_r(z.hl(), z.c);
    z.inc_l();
    z.ld_a_mem(z.hl()); z.add_a(z.a); z.add_a(z.a); z.add_a_mem(z.hl()); z.inc_a(); z.ld_mem_r(z.hl(), z.a);
    z.inc_l();
    z.ld_a_mem(z.hl()); z.add_a(z.a); z.add_a_mem(z.hl()); z.add_a(z.a); z.add_a(z.a); z.add_a_mem(z.hl()); z.inc_a(); z.ld_mem_r(z.hl(), z.a);
    return z.ret(10);
}

// the four letter writers 0369 (1UP shown), 0376 (2UP shown), 0383 (1UP hidden), 0390 (2UP hidden), called with CALL (17) / CALL cc (17 taken, 10 not taken):
// three LD (IX/IY+d),n (19 T-states, 2 fetches each) and RET (10): the return address stays on the stack below the stack pointer
template <class Mem> inline void letters(Z<Mem> &z, int which, uint16_t ret_addr, uint16_t &last) {
    static const uint8_t vals[4][3] = {{0x50, 0x55, 0x31}, {0x50, 0x55, 0x32}, {0x40, 0x40, 0x40}, {0x40, 0x40, 0x40}};
    const uint16_t base = (which == 1 || which == 3) ? z.iy : z.ix;
    z.t(17);
    for (int i = 0; i < 3; ++i) { z.mem.wr((uint16_t)(base + i), vals[which][i]); z.t(19, 2); }
    z.wz = (uint16_t)(base + 2);
    z.t(10);
    z.wz = ret_addr;
    last = ret_addr;
}

template <class Mem> inline Result blink_02fd(Z<Mem> &z) {
    uint16_t last = 0;
    z.set_hl(ram::blink_timer_1up); z.t(10);
    z.inc_mem(ram::blink_timer_1up);
    z.ld_a_mem(z.hl());
    z.and_n(0x0F);
    if (!z.jr(z.a != 0, 0x0325)) {
        z.ld_a_mem(z.hl());
        z.rrca(); z.rrca(); z.rrca(); z.rrca();
        z.b = z.a; z.t(4);
        z.ld_a_nn(ram::byte_4DD6); z.cpl(); z.or_r(z.b);
        z.c = z.a; z.t(4);
        z.ld_a_nn(ram::credits); z.sub_n(1);
        if (!z.jr(!(z.f & flag::C), 0x031B)) { z.a = 0; z.f = flag::logic8(0, false); z.t(4); z.c = z.a; z.t(4); }
        if (!z.jr((z.f & flag::Z) != 0, 0x031E)) { z.a = z.c; z.t(4); }
        z.ld_nn_a(0x5005);
        z.a = z.c; z.t(4);
        z.ld_nn_a(0x5004);
    }
    z.ix = 0x43D8; z.t(14, 2);
    z.iy = 0x43C5; z.t(14, 2);
    z.ld_a_nn(ram::main_state); z.cp_n(3); z.jp(0x0344);
    bool l344 = (z.f & flag::Z) != 0;
    if (!l344) {
        z.ld_a_nn(ram::credit_ready_state); z.cp_n(2); z.jp(0x0344);
        l344 = !(z.f & flag::C);
    }
    if (!l344) {
        letters(z, 0, 0x0340, last);
        letters(z, 1, 0x0343, last);
        Result r = z.ret(10); r.leftover = last; return r;
    }
    z.ld_a_nn(ram::current_player); z.and_a(); z.ld_a_nn(ram::blink_timer_1up);
    z.jp(0x0359);
    const bool nonzero = (z.f & flag::Z) == 0;
    z.bit(4, z.a);
    const bool bit_clear = (z.f & flag::Z) != 0;
    if (nonzero) {
        if (bit_clear) letters(z, 1, 0x035E, last); else z.t(10);   // CALL Z,0376
        if (!bit_clear) letters(z, 3, 0x0361, last); else z.t(10);  // CALL NZ,0390
    } else {
        if (bit_clear) letters(z, 0, 0x0353, last); else z.t(10);   // CALL Z,0369
        if (!bit_clear) letters(z, 2, 0x0356, last); else z.t(10);  // CALL NZ,0383
        z.jp(0x0361);
    }
    z.ld_a_nn(ram::two_player_game); z.and_a();
    if (z.f & flag::Z) letters(z, 3, 0x0368, last); else z.t(10);   // CALL Z,0390
    Result r = z.ret(10); r.leftover = last; return r;
}

} // namespace clk
} // namespace game
