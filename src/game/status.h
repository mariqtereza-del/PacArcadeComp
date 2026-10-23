// N16: the score, credit, life and fruit pictures on the screen, written natively (no Z80). ROM routines replaced (entries):
//   2AE0  redraw the score area (text 0, clear the score RAM, both scores)        2B33  an extra life: set the flags, count it, then 2B4A
//   2B4A  the row of life pictures (B = lives shown)                               2B6A  clear the life area (inline data after the CALL to 2BCD), then 2B4A
//   2B7E  a blank 2x2 tile block at HL (A = 40h)   2B80  2x2 block, all four tiles A   2B8F  2x2 block with the tiles A, A+1, A+2, A+3
//   2BA1  the credit display (text 1 or 2 and the number)                          2BCD  fill a rectangle given by the data after the CALL
//   2BEA  the row of level fruit pictures (from the table that the player's ROM holds at run time)
// Each function takes the register values the ROM routine starts with and returns the registers and the exact cost (T-states and opcode fetches). `sp` is the
// stack pointer at the entry of that routine (its return address is at sp); the bytes that PUSH, CALL and RST leave below it are written here, in the ROM's order.
// Mem: rd, wr, and a public  uint16_t note_at  (where a CALL inside the routine called 2ABE pushes its return addresses, see numbers.h).
// The proof is tests/test_native_status.cpp. No ROM bytes: the fruit table and the inline data are read from the player's ROM image at run time.
#pragma once
#include "state.h"
#include <cstdint>
#include "helpers.h"
#include "screen.h"
#include "numbers.h"
#include "text.h"

namespace game {

template <class Mem> inline uint16_t get_word(Mem &mem, uint16_t at) { return (uint16_t)(mem.rd(at) | (mem.rd((uint16_t)(at + 1)) << 8)); }

// ---- 2B80 / 2B7E: PUSH HL / PUSH DE / LD (HL),A / INC HL / LD (HL),A / LD DE,1F / ADD HL,DE / LD (HL),A / INC HL / LD (HL),A / POP DE / POP HL / RET: 113 T-states 13 fetches
// (2B7E first loads A = 40h: 120 and 14). Writes HL, HL+1, HL+20h, HL+21h. F = the flags of the ADD.
template <class Mem> inline void block_2b80(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    put_word(mem, (uint16_t)(sp - 2), r.hl());
    put_word(mem, (uint16_t)(sp - 4), r.de());
    const uint16_t hl = r.hl();
    mem.wr(hl, r.a);
    mem.wr((uint16_t)(hl + 1), r.a);
    uint16_t t;
    r.f = flag::add16((uint16_t)(hl + 1), 0x001F, r.f, &t);
    mem.wr(t, r.a);
    mem.wr((uint16_t)(t + 1), r.a);
    cost.add(113, 13);
}
template <class Mem> inline void block_2b7e(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    r.a = 0x40;
    cost.add(7, 1);
    block_2b80(mem, r, sp, cost);
}

// ---- 2B8F: like 2B80 but the four tiles count up: A, A+1, A+2, A+3 (A ends A+3). 125 T-states 16 fetches.
template <class Mem> inline void block_2b8f(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    put_word(mem, (uint16_t)(sp - 2), r.hl());
    put_word(mem, (uint16_t)(sp - 4), r.de());
    const uint16_t hl = r.hl();
    uint8_t a = r.a;
    mem.wr(hl, a);
    r.f = flag::inc8(a, r.f); ++a;
    mem.wr((uint16_t)(hl + 1), a);
    r.f = flag::inc8(a, r.f); ++a;
    uint16_t t;
    r.f = flag::add16((uint16_t)(hl + 1), 0x001F, r.f, &t);
    mem.wr(t, a);
    r.f = flag::inc8(a, r.f); ++a;
    mem.wr((uint16_t)(t + 1), a);
    r.a = a;
    cost.add(125, 16);
}

// ---- 2BCD: the data (E, D, C, B, A: destination, the byte to write, columns, rows) follows the CALL; fills A rows of B bytes (0 = 256) with C, rows 20h apart.
// The routine returns behind the data. In: the return address at sp. Out: DE = 20h, HL = the start of the row after the last, B and C as read, A = 0, F = flags of the last DEC A.
template <class Mem> inline void fill_rect_2bcd(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    uint16_t p = get_word(mem, sp);
    r.e = mem.rd(p++); r.d = mem.rd(p++); r.c = mem.rd(p++); r.b = mem.rd(p++); r.a = mem.rd(p++);
    put_word(mem, sp, p);                       // PUSH HL: the return address behind the data
    r.set_hl(r.de());
    r.set_de(0x0020);
    cost.add(100, 14);
    for (;;) {
        put_word(mem, (uint16_t)(sp - 2), r.hl());
        put_word(mem, (uint16_t)(sp - 4), (uint16_t)(r.b << 8 | r.c));
        unsigned n = r.b ? r.b : 256u;
        uint16_t hl = r.hl();
        for (unsigned i = 0; i < n; ++i) mem.wr(hl++, r.c);
        cost.add(11 + 11 + 26 * n - 5 + 10 + 10 + 11 + 4, 3 * n + 7 - 1);
        uint16_t t;
        r.f = flag::add16(r.hl(), 0x0020, r.f, &t);
        r.set_hl(t);
        r.f = flag::dec8(r.a, r.f);
        r.a = (uint8_t)(r.a - 1);
        if (r.a == 0) { cost.add(7, 1); break; }
        cost.add(12, 1);
    }
    cost.add(10, 1);                            // RET
}

// ---- 2B4A: the life pictures. B = lives shown. HL = 401A, C = 5; B in 1..5: B pictures (2B8F with A = 20h) going left, then blanks (2B7E) up to five in all; B = 0 or more than 5: five blanks. Ends with RET.
template <class Mem> inline void lives_2b4a(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    r.set_hl(0x401A);
    r.c = 5;
    r.a = r.b;
    r.f = flag::and_a(r.a);
    cost.add(10 + 7 + 4 + 4, 4);
    bool pictures = false;
    if (r.a == 0) cost.add(12, 1);
    else {
        r.f = flag::cp8(r.a, 6);
        cost.add(7 + 7, 2);
        if (r.a >= 6) cost.add(12, 1);
        else { cost.add(7, 1); pictures = true; }
    }
    if (pictures) {
        for (;;) {
            r.a = 0x20;
            cost.add(7 + 17, 2);
            put_word(mem, (uint16_t)(sp - 2), 0x2B5C);
            block_2b8f(mem, r, (uint16_t)(sp - 2), cost);
            r.set_hl((uint16_t)(r.hl() - 2));
            r.f = flag::dec8(r.c, r.f); r.c = (uint8_t)(r.c - 1);
            cost.add(6 + 6 + 4, 3);
            r.b = (uint8_t)(r.b - 1);
            if (r.b) cost.add(13, 1); else { cost.add(8, 1); break; }
        }
    }
    for (;;) {   // 2B61
        r.f = flag::dec8(r.c, r.f); r.c = (uint8_t)(r.c - 1);
        cost.add(4, 1);
        if (r.f & flag::S) { cost.add(11, 1); break; }
        cost.add(5, 1);
        cost.add(17, 1);
        put_word(mem, (uint16_t)(sp - 2), 0x2B66);
        block_2b7e(mem, r, (uint16_t)(sp - 2), cost);
        r.set_hl((uint16_t)(r.hl() - 2));
        cost.add(6 + 6 + 12, 3);
    }
}

// ---- 2B33: an extra life. DE points at the byte before the "extra life given" flag (bit 0 of the byte at DE+1). `wz` is the Z80's WZ register at the entry (BIT n,(HL) takes
// its flags X and Y from it). Returns false when the flag was already set (RET NZ).
template <class Mem> inline bool extra_life_2b33(Mem &mem, Regs &r, uint16_t wz, uint16_t sp, Cost &cost) {
    r.set_hl((uint16_t)(r.de() + 1));
    cost.add(6 + 4 + 4 + 6, 4);
    const uint8_t v = mem.rd(r.hl());
    const bool zero = !(v & 1);
    r.f = (uint8_t)((r.f & flag::C) | flag::H | (zero ? (flag::Z | flag::PV) : 0) | ((wz >> 8) & flag::XY));
    cost.add(12, 2);
    if (!zero) { cost.add(11, 1); return false; }
    cost.add(5, 1);
    mem.wr(r.hl(), (uint8_t)(v | 1));
    cost.add(15, 2);
    r.set_hl(ram::voice1_effect_a);
    mem.wr(r.hl(), (uint8_t)(mem.rd(r.hl()) | 1));
    cost.add(10 + 15, 3);
    r.set_hl(ram::lives);
    uint8_t x = mem.rd(r.hl());
    r.f = flag::inc8(x, r.f);
    mem.wr(r.hl(), (uint8_t)(x + 1));
    cost.add(10 + 11, 2);
    r.set_hl(ram::lives_displayed);
    x = mem.rd(r.hl());
    r.f = flag::inc8(x, r.f);
    mem.wr(r.hl(), (uint8_t)(x + 1));
    r.b = (uint8_t)(x + 1);
    cost.add(10 + 11 + 7, 3);
    lives_2b4a(mem, r, sp, cost);
    return true;
}

// ---- 2B6A: not in the demo (4E00 = 1): clear the life area with the data that follows the CALL (2B73..2B77), then the life pictures for the lives in 4E15.
template <class Mem> inline void lives_area_2b6a(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    r.a = mem.rd(ram::main_state);
    r.f = flag::cp8(r.a, 1);
    cost.add(13 + 7, 2);
    if (r.a == 1) { cost.add(11, 1); return; }
    cost.add(5, 1);
    cost.add(17, 1);
    put_word(mem, (uint16_t)(sp - 2), 0x2B73);
    fill_rect_2bcd(mem, r, (uint16_t)(sp - 2), cost);
    r.set_hl(ram::lives_displayed);
    r.b = mem.rd(ram::lives_displayed);
    cost.add(10 + 7 + 12, 3);
    lives_2b4a(mem, r, sp, cost);
}

// ---- 2BA1: the credit display. (4E6E) = FF: text 2 (the free play line) and the end. Otherwise text 1 and the credits as digits at 4034 (tens, only when not zero) and 4033.
// ix = the IX register after the text printer.
template <class Mem> inline void credits_2ba1(Mem &mem, Regs &r, uint16_t &ix, uint16_t sp, Cost &cost) {
    r.a = mem.rd(ram::credits);
    r.f = flag::cp8(r.a, 0xFF);
    cost.add(13 + 7, 2);
    if (r.a == 0xFF) {
        cost.add(7 + 7 + 10, 3);
        r.b = 2;
        text_2c5e(mem, r, ix, sp, cost);
        return;
    }
    cost.add(12 + 7 + 17, 3);
    r.b = 1;
    put_word(mem, (uint16_t)(sp - 2), 0x2BB2);
    text_2c5e(mem, r, ix, (uint16_t)(sp - 2), cost);
    r.a = mem.rd(ram::credits);
    r.a &= 0xF0;
    r.f = flag::and_a(r.a);
    cost.add(13 + 7, 2);
    if (r.a == 0) cost.add(12, 1);
    else {
        cost.add(7, 1);
        for (int i = 0; i < 4; ++i) {   // RRCA x4: S, Z, P/V keep; H and N clear; C = bit 0 that went round; X and Y from A
            const uint8_t c = r.a & 1;
            r.a = (uint8_t)((r.a >> 1) | (c << 7));
            r.f = (uint8_t)((r.f & (flag::S | flag::Z | flag::PV)) | (r.a & flag::XY) | c);
        }
        uint8_t sum;
        r.f = flag::add8(r.a, 0x30, 0, &sum);
        r.a = sum;
        mem.wr(0x4034, r.a);
        cost.add(16 + 7 + 13, 6);
    }
    r.a = mem.rd(ram::credits);
    r.a &= 0x0F;
    r.f = flag::and_a(r.a);
    uint8_t sum;
    r.f = flag::add8(r.a, 0x30, 0, &sum);
    r.a = sum;
    mem.wr(0x4033, r.a);
    cost.add(13 + 7 + 7 + 13 + 10, 5);
}

// ---- 2AE0: redraw the score area: text 0 (B = 0), clear the 8 score bytes at 4E80, draw both scores (player 1 at 43FC, player 2 at 43E9; C = 4 blanks for two players, else 6).
template <class Mem> inline void score_area_2ae0(Mem &mem, Regs &r, uint16_t &ix, uint16_t sp, Cost &cost) {
    r.b = 0;
    cost.add(7 + 17, 2);
    put_word(mem, (uint16_t)(sp - 2), 0x2AE5);
    text_2c5e(mem, r, ix, (uint16_t)(sp - 2), cost);
    r.a = 0;
    r.f = flag::xor_a();
    r.set_hl(ram::score_p1);
    r.b = 8;
    cost.add(4 + 10 + 7 + 11, 4);
    put_word(mem, (uint16_t)(sp - 2), 0x2AEC);
    fill_0008(mem, r);
    cost.add(fill_cycles_0008(8), fill_fetches_0008(8));
    r.b = 3; r.c = 4;
    r.set_de((ram::score_p1 + 2));
    r.set_hl(0x43FC);
    cost.add(10 + 10 + 10 + 17, 4);
    put_word(mem, (uint16_t)(sp - 2), 0x2AF8);
    mem.note_at = (uint16_t)(sp - 4);
    digit_row_2abe(mem, r, cost);
    r.b = 3; r.c = 4;
    r.set_de((ram::score_p2 + 2));
    r.set_hl(0x43E9);
    r.a = mem.rd(ram::two_player_game);
    r.f = flag::and_a(r.a);
    cost.add(10 + 10 + 10 + 13 + 4, 5);
    if (r.a != 0) cost.add(12, 1);
    else { cost.add(7 + 7 + 12, 3); r.c = 6; }
    mem.note_at = (uint16_t)(sp - 2);
    digit_row_2abe(mem, r, cost);
}

// ---- 2BEA: the level fruit row. Not in the demo. The number of pictures is (4E13) + 1 (at most 7 pictures from the start of the table; further levels move along the table).
// The table (the pictures: two tile codes per fruit) is read from the ROM image: tile address 3B08 is an address, not a byte.
constexpr uint16_t kFruitTableAddress = 0x3B08;
template <class Mem> inline void fruit_row_2bea(Mem &mem, Regs &r, uint16_t sp, Cost &cost) {
    r.a = mem.rd(ram::main_state);
    r.f = flag::cp8(r.a, 1);
    cost.add(13 + 7, 2);
    if (r.a == 1) { cost.add(11, 1); return; }
    cost.add(5, 1);
    r.a = mem.rd(ram::level);
    r.f = flag::inc8(r.a, r.f); r.a = (uint8_t)(r.a + 1);
    r.f = flag::cp8(r.a, 8);
    cost.add(13 + 4 + 7 + 10, 4);
    if (r.a >= 8) {   // 2C2E
        r.f = flag::cp8(r.a, 0x13);
        cost.add(7, 1);
        if (r.a < 0x13) cost.add(12, 1);
        else { cost.add(7 + 7, 2); r.a = 0x13; }
        uint8_t d;
        r.f = flag::sub8(r.a, 7, &d); r.a = d;
        r.c = r.a; r.b = 0;
        r.set_hl(kFruitTableAddress);
        uint16_t t;
        r.f = flag::add16(r.hl(), (uint16_t)(r.b << 8 | r.c), r.f, &t); r.set_hl(t);
        r.f = flag::add16(r.hl(), (uint16_t)(r.b << 8 | r.c), r.f, &t); r.set_hl(t);
        const uint16_t old_de = r.de();
        r.set_de(r.hl());
        r.set_hl(old_de);
        r.b = 7;
        cost.add(7 + 4 + 7 + 10 + 11 + 11 + 4 + 7 + 10, 9);
    } else {
        r.set_de(kFruitTableAddress);
        r.b = r.a;
        cost.add(10 + 4, 2);
    }
    r.c = 7;
    r.set_hl(0x4004);
    cost.add(7 + 10, 2);
    for (;;) {   // 2C02: one fruit picture and the matching colour block
        r.a = mem.rd(r.de());
        cost.add(7 + 17, 2);
        put_word(mem, (uint16_t)(sp - 2), 0x2C06);
        block_2b8f(mem, r, (uint16_t)(sp - 2), cost);
        uint8_t s;
        r.f = flag::add8(4, r.h, 0, &s); r.a = s; r.h = s;
        r.set_de((uint16_t)(r.de() + 1));
        r.a = mem.rd(r.de());
        cost.add(7 + 4 + 4 + 6 + 7 + 17, 6);
        put_word(mem, (uint16_t)(sp - 2), 0x2C0F);
        block_2b80(mem, r, (uint16_t)(sp - 2), cost);
        r.a = 0xFC;
        r.f = flag::add8(r.a, r.h, 0, &s); r.a = s; r.h = s;
        r.set_de((uint16_t)(r.de() + 1));
        r.set_hl((uint16_t)(r.hl() + 2));
        r.f = flag::dec8(r.c, r.f); r.c = (uint8_t)(r.c - 1);
        cost.add(7 + 4 + 4 + 6 + 6 + 6 + 4, 7);
        r.b = (uint8_t)(r.b - 1);
        if (r.b) cost.add(13, 1); else { cost.add(8, 1); break; }
    }
    for (;;) {   // 2C19: blank pictures for the places left
        r.f = flag::dec8(r.c, r.f); r.c = (uint8_t)(r.c - 1);
        cost.add(4, 1);
        if (r.f & flag::S) { cost.add(11, 1); break; }
        cost.add(5 + 17, 2);
        put_word(mem, (uint16_t)(sp - 2), 0x2C1E);
        block_2b7e(mem, r, (uint16_t)(sp - 2), cost);
        uint8_t s;
        r.f = flag::add8(4, r.h, 0, &s); r.a = s; r.h = s;
        r.a = 0;
        r.f = flag::xor_a();
        cost.add(7 + 4 + 4 + 4 + 17, 5);
        put_word(mem, (uint16_t)(sp - 2), 0x2C26);
        block_2b80(mem, r, (uint16_t)(sp - 2), cost);
        r.a = 0xFC;
        r.f = flag::add8(r.a, r.h, 0, &s); r.a = s; r.h = s;
        r.set_hl((uint16_t)(r.hl() + 2));
        cost.add(7 + 4 + 4 + 6 + 6 + 12, 6);
    }
}

} // namespace game
