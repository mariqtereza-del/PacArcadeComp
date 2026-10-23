// N14: the number helpers, written natively (no Z80). ROM routines replaced (entries):
//   2ACE  one BCD digit to a tile code on the screen, with leading zeros blanked (the counter C says how many zeros may still be blanked)
//   2ABE  a row of BCD bytes (two digits each, high digit first, read downwards from DE) to tiles written downwards from HL
//   2AAF  the score of the current player to the screen (chooses the screen position by the player, then 2ABE)
//   2B0B  the address of the current player's score in RAM (4E80 or 4E84)
//   02DF  the coin counter: add the BCD step to the credits with DAA (limit 99)
// Mem also needs  void stack_note(uint16_t return_address)  (the adapter writes the bytes a CALL inside leaves on the stack, at the moment the ROM does).
// Each function takes the register values the ROM routine starts with and returns the registers and the exact cost (T-states and opcode fetches) of the ROM
// routine. The stack bytes the ROM leaves (the return addresses of the CALLs inside) are written by the adapters in src/rules/rules.h (namespace numbers).
// The proof is tests/test_native_numbers.cpp. No ROM bytes.
#pragma once
#include "state.h"
#include <cstdint>
#include "screen.h"

namespace game {
namespace flag {
inline uint8_t dec8(uint8_t v, uint8_t old_f) { // DEC r: the carry keeps its value, N is set
    const uint8_t r = (uint8_t)(v - 1);
    uint8_t f = (uint8_t)((old_f & C) | N);
    f |= (uint8_t)(r & (S | XY));
    if (r == 0) f |= Z;
    if ((v & 0x0F) == 0) f |= H;
    if (v == 0x80) f |= PV;
    return f;
}
inline uint8_t sub8(uint8_t a, uint8_t v, uint8_t *result) { // SUB v
    const unsigned r = (unsigned)a - v;
    uint8_t f = (uint8_t)(N | (r & (S | XY)));
    if ((r & 0xFF) == 0) f |= Z;
    if ((a & 0x0F) < (v & 0x0F)) f |= H;
    if (((a ^ v) & (a ^ r) & 0x80) != 0) f |= PV;
    if (a < v) f |= C;
    *result = (uint8_t)r;
    return f;
}
inline uint8_t parity_flag(uint8_t v) {
    v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
    return (v & 1) ? 0 : PV;
}
inline uint8_t daa(uint8_t *a, uint8_t f) { // DAA: the decimal adjust after an addition or subtraction
    const bool n = f & N, c = f & C, h = f & H;
    unsigned corr = 0;
    bool carry = false;
    if (h || (*a & 0x0F) > 9) corr |= 0x06;
    if (c || *a > 0x99) { corr |= 0x60; carry = true; }
    const uint8_t r = n ? (uint8_t)(*a - corr) : (uint8_t)(*a + corr);
    uint8_t out = (uint8_t)((r & (S | XY)) | (f & N));
    if (r == 0) out |= Z;
    out |= parity_flag(r);
    if (n ? (h && (*a & 0x0F) < 6) : ((*a & 0x0F) > 9)) out |= H;
    if (carry) out |= C;
    *a = r;
    return out;
}
} // namespace flag

// ---- 2ACE: A = the tile code to write (a digit 1 to 15 is its own tile code, a blanked zero is 40h, a kept zero is 00h), written at (HL), HL = HL - 1.
// Digit not zero: C = 0.  Zero and C = 0: the zero is kept.  Zero and C not zero: blanked, C = C - 1.
// Costs: digit not zero 56 T-states 7 fetches; kept zero 62 and 8; blanked zero 68 and 10.
template <class Mem> inline void digit_tile_2ace(Mem &mem, Regs &r, Cost &cost) {
    r.a &= 0x0F;
    r.f = flag::and_a(r.a);
    if (r.a != 0) {
        r.c = 0;
        cost.add(56, 7);
    } else {
        r.a = r.c;
        r.f = flag::and_a(r.a);
        if (r.a == 0) {
            cost.add(62, 8);
        } else {
            r.a = 0x40;
            r.f = flag::dec8(r.c, r.f);
            r.c = (uint8_t)(r.c - 1);
            cost.add(68, 10);
        }
    }
    mem.wr(r.hl(), r.a);
    r.set_hl((uint16_t)(r.hl() - 1));
}

// ---- 2ABE: B bytes (0 = 256) from (DE) downwards; each gives its high digit then its low digit to 2ACE. DE ends B bytes lower. Per byte 83 T-states and 10 fetches
// plus the two 2ACE calls (the last byte's DJNZ is 5 T-states shorter), then RET 10 T-states and 1 fetch.
template <class Mem> inline void digit_row_2abe(Mem &mem, Regs &r, Cost &cost) {
    unsigned n = r.b ? r.b : 256u;
    for (;;) {
        r.a = mem.rd(r.de());
        r.a = (uint8_t)((r.a >> 4) | (r.a << 4));                 // RRCA x4
        r.f = (uint8_t)((r.f & (flag::S | flag::Z | flag::PV)) | (r.a & flag::XY) | ((r.a >> 7) & 1));
        mem.stack_note(0x2AC6);                                   // the CALL 2ACE at 2AC3 pushes its return address
        digit_tile_2ace(mem, r, cost);
        r.a = mem.rd(r.de());
        mem.stack_note(0x2ACA);                                   // the CALL at 2AC7
        digit_tile_2ace(mem, r, cost);
        r.set_de((uint16_t)(r.de() - 1));
        cost.add(7 + 16 + 17 + 7 + 17 + 6, 9);                  // LD A,(DE) / RRCA x4 / CALL / LD A,(DE) / CALL / DEC DE
        r.b = (uint8_t)(r.b - 1);
        if (--n == 0) { cost.add(8, 1); break; }               // DJNZ falls through
        cost.add(13, 1);
    }
    cost.add(10, 1);                                            // RET
}
constexpr unsigned kDigitRowByteCycles = 83 + 56 + 56, kDigitRowMaxBytes = 256;

// ---- 2AAF: the score of the current player. A = (4E09): 0 = player 1 (screen position 43FC), else player 2 (43E9). BC = 0304 (3 bytes, 4 = the blanking counter C),
// then 2ABE. 49 T-states and 5 fetches (player 1) or 54 and 6 (player 2) before 2ABE.
constexpr uint16_t kPlayerFlag = ram::current_player;
template <class Mem> inline void score_draw_2aaf(Mem &mem, Regs &r, Cost &cost) {
    r.a = mem.rd(kPlayerFlag);
    r.b = 0x03;
    r.c = 0x04;
    r.set_hl(0x43FC);
    r.f = flag::and_a(r.a);
    if (r.a == 0) {
        cost.add(49, 5);
    } else {
        r.set_hl(0x43E9);
        cost.add(54, 6);
    }
    digit_row_2abe(mem, r, cost);
}

// ---- 2B0B: HL = 4E80 (player 1) or 4E84 (player 2), A = (4E09), F = flags of AND A. 38 T-states 4 fetches / 52 and 6.
template <class Mem> inline void score_pointer_2b0b(Mem &mem, Regs &r, Cost &cost) {
    r.a = mem.rd(kPlayerFlag);
    r.set_hl(ram::score_p1);
    r.f = flag::and_a(r.a);
    if (r.a == 0) { cost.add(38, 4); return; }
    r.set_hl(ram::score_p2);
    cost.add(52, 6);
}

// ---- 02DF: the coin counter. (4E6C) is counted up; when it reaches (4E6B) it is reset to 0 and the BCD number (4E6D) is added to (4E6E) (credits) with DAA, 99 at most,
// and bit 1 of (4E9C) is set. Not reached: 52 T-states and 5 fetches, A = (4E6B) - (4E6C), F = flags of the SUB. Reached: 139 T-states (146 when the credits
// overflow) and 17 fetches (18).
template <class Mem> inline void coin_counter_02df(Mem &mem, Regs &r, Cost &cost) {
    r.a = mem.rd(ram::coins_per_credit);
    r.set_hl(ram::partial_credit_coins);
    const uint8_t counted = (uint8_t)(mem.rd(ram::partial_credit_coins) + 1);
    mem.wr(ram::partial_credit_coins, counted);
    r.f = flag::sub8(r.a, counted, &r.a);
    if (r.a != 0) { cost.add(52, 5); return; }
    mem.wr(ram::partial_credit_coins, r.a);                                       // LD (HL),A with A = 0
    r.a = mem.rd(ram::credits_per_coin);
    r.set_hl(ram::credits);
    uint8_t sum;
    r.f = flag::add8(r.a, mem.rd(ram::credits), 0, &sum);
    r.a = sum;
    r.f = flag::daa(&r.a, r.f);
    unsigned c = 139, f = 16;
    if (r.f & flag::C) { r.a = 0x99; c += 7; f += 1; }
    mem.wr(ram::credits, r.a);
    r.set_hl(ram::voice1_effect_a);
    mem.wr(ram::voice1_effect_a, (uint8_t)(mem.rd(ram::voice1_effect_a) | 0x02));
    cost.add(c, f);
}

} // namespace game
