// N23: the coin inputs and the coin counter pulse of the interrupt handler, written natively (no Z80). ROM routines replaced (entries):
//   0267  reads the coin inputs (5000) and counts credits: the credits byte 4E6E against 99 (the coin lockout output 5006), then the three coin input shift counters
//         4E66 / 4E67 / 4E68 (coin slot 1, coin slot 2, the service coin), calling the credit adder 02DF and counting the pulses to give in 4E69
//   02AD  the coin counter pulse timing: 4E69 pulses to give, the step counter 4E6A, the coin counter output 5007; calls 02DF once per pulse start
// 02DF itself (the credit adder) was made native in N14; here it is run inline so that the CALL's return address stays on the stack exactly as in the ROM.
// Every instruction is accounted for (T-states, opcode fetches, flags, WZ). No ROM bytes. Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "sprite_pics.h"
#include "numbers.h"

namespace game {
namespace coin {
using spp::Z;
using spp::Result;

// RLA / RRA / RLC B with the flags of the Z80 (the undocumented XY bits included)
template <class Mem> inline void rla(Z<Mem> &z) {
    const uint8_t c = z.f & flag::C, n = (uint8_t)((z.a << 1) | c), co = z.a >> 7;
    z.a = n; z.f = (uint8_t)((z.f & (flag::S | flag::Z | flag::PV)) | (n & flag::XY) | co); z.t(4);
}
template <class Mem> inline void rra(Z<Mem> &z) {
    const uint8_t c = z.f & flag::C, n = (uint8_t)((z.a >> 1) | (c << 7)), co = z.a & 1;
    z.a = n; z.f = (uint8_t)((z.f & (flag::S | flag::Z | flag::PV)) | (n & flag::XY) | co); z.t(4);
}
template <class Mem> inline void rlc_b(Z<Mem> &z) {
    const uint8_t co = z.b >> 7, n = (uint8_t)((z.b << 1) | co);
    z.b = n; z.f = (uint8_t)(flag::logic8(n, false) | co); z.t(8, 2);
}
template <class Mem> inline void sub_a_mem(Z<Mem> &z, uint16_t ad) { uint8_t r; z.f = flag::sub8(z.a, z.mem.rd(ad), &r); z.a = r; z.t(7); }
template <class Mem> inline void dec_b(Z<Mem> &z) { z.f = flag::dec8(z.b, z.f); z.b = (uint8_t)(z.b - 1); z.t(4); }

// CALL 02DF (17 T-states), the credit adder, and its RET; last = the return address the CALL left on the stack
template <class Mem> inline void call_02df(Z<Mem> &z, uint16_t ret_addr, uint16_t &last) {
    z.t(17); z.wz = 0x02DF; last = ret_addr;
    z.ld_a_nn(ram::coins_per_credit);
    z.set_hl(ram::partial_credit_coins); z.t(10);
    z.inc_mem(z.hl());
    sub_a_mem(z, z.hl());
    if (z.a != 0) { z.t(11); z.wz = ret_addr; return; }          // RET NZ taken
    z.t(5);
    z.ld_mem_r(z.hl(), z.a);
    z.ld_a_nn(ram::credits_per_coin);
    z.set_hl(ram::credits); z.t(10);
    z.add_a_mem(z.hl());
    { uint8_t a = z.a; z.f = flag::daa(&a, z.f); z.a = a; z.t(4); }
    z.jp(0x02F6);
    if (z.f & flag::C) { z.a = 0x99; z.t(7); }
    z.ld_mem_r(z.hl(), z.a);
    z.set_hl(ram::voice1_effect_a); z.t(10);
    z.mem.wr(z.hl(), (uint8_t)(z.mem.rd(z.hl()) | 0x02)); z.t(15, 2);
    z.t(10); z.wz = ret_addr;                                      // RET
}

template <class Mem> inline Result coins_0267(Z<Mem> &z) {
    uint16_t last = 0;
    z.ld_a_nn(ram::credits); z.cp_n(0x99); rla(z); z.ld_nn_a(0x5006); rra(z);
    if (!(z.f & flag::C)) return z.ret(11);                       // RET NC
    z.t(5);
    z.ld_a_nn(0x5000); z.b = z.a; z.t(4); rlc_b(z);
    z.ld_a_nn(ram::byte_4E66); rla(z); z.and_n(0x0F); z.ld_nn_a(ram::byte_4E66); z.sub_n(0x0C);
    if (z.f & flag::Z) call_02df(z, 0x0286, last); else z.t(10);   // CALL Z,02DF
    rlc_b(z);
    z.ld_a_nn(ram::byte_4E67); rla(z); z.and_n(0x0F); z.ld_nn_a(ram::byte_4E67); z.sub_n(0x0C);
    z.jp(0x029A);                                                  // JP NZ,029A (always 10 T-states, WZ = target)
    if (z.f & flag::Z) { z.set_hl(ram::byte_4E69); z.t(10); z.inc_mem(z.hl()); }
    rlc_b(z);
    z.ld_a_nn(ram::byte_4E68); rla(z); z.and_n(0x0F); z.ld_nn_a(ram::byte_4E68); z.sub_n(0x0C);
    if (!(z.f & flag::Z)) { Result r = z.ret(11); r.leftover = last; return r; }   // RET NZ
    z.t(5);
    z.set_hl(ram::byte_4E69); z.t(10); z.inc_mem(z.hl());
    Result r = z.ret(10); r.leftover = last; return r;
}

template <class Mem> inline Result pulse_02ad(Z<Mem> &z) {
    uint16_t last = 0;
    z.ld_a_nn(ram::byte_4E69); z.and_a();
    if (z.f & flag::Z) return z.ret(11);                          // RET Z
    z.t(5);
    z.b = z.a; z.t(4);
    z.ld_a_nn(ram::byte_4E6A); z.e = z.a; z.t(4);
    z.cp_n(0x00);
    z.jp(0x02C4);                                                  // JP NZ,02C4
    if (z.f & flag::Z) {
        z.a = 0x01; z.t(7);
        z.ld_nn_a(0x5007);
        call_02df(z, 0x02C4, last);
    }
    z.a = z.e; z.t(4);
    z.cp_n(0x08);
    z.jp(0x02CE);                                                  // JP NZ,02CE
    if (z.f & flag::Z) { z.xor_r(z.a); z.ld_nn_a(0x5007); }
    z.f = flag::inc8(z.e, z.f); z.e = (uint8_t)(z.e + 1); z.t(4);
    z.a = z.e; z.t(4);
    z.ld_nn_a(ram::byte_4E6A);
    z.sub_n(0x10);
    if (z.a != 0) { Result r = z.ret(11); r.leftover = last; return r; }   // RET NZ
    z.t(5);
    z.ld_nn_a(ram::byte_4E6A);
    dec_b(z);
    z.a = z.b; z.t(4);
    z.ld_nn_a(ram::byte_4E69);
    Result r = z.ret(10); r.leftover = last; return r;
}

} // namespace coin
} // namespace game
