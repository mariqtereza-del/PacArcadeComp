// N25: the rest of the interrupt handler's own code (the parts N20 and N22 to N24 did not cover), written natively (no Z80). The hooks CONTINUE at the next ROM address
// (the handler is one long run of code, not a set of routines); the block 009D to 018B (sound registers, waveform choices, sprite copies) was made native in N20.
//   008D  entry: PUSH AF, the watchdog write (50C0), clear the interrupt-enable latch (5000), DI, PUSH BC DE HL IX IY            -> continue at 009D
//   018C 018F 0192 019B 019E 01A1 01A4 01A7 01AA 01B9 01BC   the CALL instructions of the handler body                          -> continue at the called routine
//   0195  A = (4E00), AND A, JR Z                                                                                              -> 019B or 01AD
//   01AD  A = (4E00) - 1; when 0 (state 1) clear 4EAC and 4EBC                                                                  -> continue at 01B9
//   01BF  POP IY IX HL DE BC; state 0 or the reset switch (5040 bit 4 = 0) ... JP Z,0000 (restart)                              -> 0000 or 01D4
//   01D4  A = 1 into the interrupt-enable latch (5000), EI, POP AF, RET                                                          -> the interrupted address
// Every instruction is accounted for (T-states, opcode fetches, flags, WZ, the stack bytes). No ROM bytes. Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "sprite_pics.h"

namespace game {
namespace hnd {
using spp::Z;

struct Out { uint16_t next = 0; int iff = -1; int prefix = 0; int im = -1; int ireg = -1; int port0 = -1; bool halted = false; bool ei_last = false; };   // N47: im = interrupt mode, ireg = register I, port0 = byte written by OUT (0),A, halted = the run ended in HALT, ei_last = the run ended right after EI   // prefix (N45j): 1 / 2 = the run stopped after a DD / FD prefix byte (the frame ended inside the indexed instruction)   // iff: -1 keep, 0 off (DI), 1 on (EI)

template <class Mem> inline void push16(Z<Mem> &z, uint16_t &sp, uint16_t v) { sp = (uint16_t)(sp - 2); z.mem.wr((uint16_t)(sp + 1), (uint8_t)(v >> 8)); z.mem.wr(sp, (uint8_t)v); }   // N45j: the high byte first, as the Z80 does (matters when the stack is in the I/O area)
template <class Mem> inline uint16_t pop16(Z<Mem> &z, uint16_t &sp) { const uint16_t v = (uint16_t)(z.mem.rd(sp) | (z.mem.rd((uint16_t)(sp + 1)) << 8)); sp = (uint16_t)(sp + 2); return v; }

template <class Mem> inline Out entry_008d(Z<Mem> &z, uint16_t &sp) {
    Out o;
    push16(z, sp, (uint16_t)(z.a << 8 | z.f)); z.t(11);
    z.ld_nn_a(0x50C0);
    z.xor_r(z.a);
    z.ld_nn_a(0x5000);
    z.t(4); o.iff = 0;                                                       // DI
    push16(z, sp, (uint16_t)(z.b << 8 | z.c)); z.t(11);
    push16(z, sp, (uint16_t)(z.d << 8 | z.e)); z.t(11);
    push16(z, sp, z.hl()); z.t(11);
    push16(z, sp, z.ix); z.t(15, 2);
    push16(z, sp, z.iy); z.t(15, 2);
    o.next = 0x009D;
    return o;
}

template <class Mem> inline Out call_site(Z<Mem> &z, uint16_t &sp, uint16_t target, uint16_t ret_addr) {
    Out o;
    z.t(17); push16(z, sp, ret_addr); z.wz = target; o.next = target;
    return o;
}

template <class Mem> inline Out check_0195(Z<Mem> &z) {
    Out o;
    z.ld_a_nn(ram::main_state); z.and_a();
    o.next = z.jr((z.f & flag::Z) != 0, 0x01AD) ? 0x01AD : 0x019B;
    return o;
}

template <class Mem> inline Out state1_01ad(Z<Mem> &z) {
    Out o; o.next = 0x01B9;
    z.ld_a_nn(ram::main_state);
    z.f = flag::dec8(z.a, z.f); z.a = (uint8_t)(z.a - 1); z.t(4);
    if (z.jr(z.a != 0, 0x01B9)) return o;
    z.ld_nn_a(ram::voice2_effect_a); z.ld_nn_a(ram::voice3_effect_a);
    return o;
}

template <class Mem> inline Out exit_01bf(Z<Mem> &z, uint16_t &sp) {
    Out o; o.next = 0x01D4;
    { const uint16_t v = pop16(z, sp); z.iy = v; z.t(14, 2); }
    { const uint16_t v = pop16(z, sp); z.ix = v; z.t(14, 2); }
    { const uint16_t v = pop16(z, sp); z.set_hl(v); z.t(10); }
    { const uint16_t v = pop16(z, sp); z.set_de(v); z.t(10); }
    { const uint16_t v = pop16(z, sp); z.b = (uint8_t)(v >> 8); z.c = (uint8_t)v; z.t(10); }
    z.ld_a_nn(ram::main_state); z.and_a();
    if (z.jr((z.f & flag::Z) != 0, 0x01D4)) return o;
    z.ld_a_nn(0x5040); z.and_n(0x10);
    z.jp(0x0000);
    if (z.f & flag::Z) o.next = 0x0000;
    return o;
}

template <class Mem> inline Out exit_01d4(Z<Mem> &z, uint16_t &sp) {
    Out o;
    z.a = 0x01; z.t(7);
    z.ld_nn_a(0x5000);
    z.t(4); o.iff = 1;                                                       // EI
    { const uint16_t v = pop16(z, sp); z.a = (uint8_t)(v >> 8); z.f = (uint8_t)v; z.t(10); }
    o.next = pop16(z, sp); z.t(10); z.wz = o.next;                           // RET
    return o;
}

// N27: the game-mode dispatchers: LD A,(state byte) / RST 20h. The RST pushes the address after it (the jump table that follows) and continues at 0020, the jump-table routine.
//   03C8  state 4E00 (table 03CC)    03D4  state 4E01 (table 03D8)    05E5  state 4E03 (table 05E9)    06BE  state 4E04 (table 06C2)
template <class Mem> inline Out mode_dispatch(Z<Mem> &z, uint16_t &sp, uint16_t state_byte, uint16_t table) {
    Out o;
    z.ld_a_nn(state_byte);
    z.t(11); hnd::push16(z, sp, table); z.wz = 0x0020; o.next = 0x0020;   // RST 20h
    return o;
}

} // namespace hnd
} // namespace game
