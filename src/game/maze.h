// N18: drawing the maze and placing the dots at the start of a level, written natively (no Z80). ROM routines replaced (entries):
//   2419  draw the maze: the table of tile codes (read from the player's ROM at run time) is walked; every entry writes a tile and its mirror image (left/right half)
//   2448  put the dots on the screen: the 30 bytes of dot bits at 4E16 (8 bits each) are walked with the offset table (ROM) and a dot tile (10h) is written for every set bit;
//         then the 4 energizer bytes at 4E34 go to their 4 screen places
//   2487  the reverse: read the screen and collect the dot bits (and the energizer bytes)
//   24C9  reset the dot bits (30 bytes of FFh at 4E16, 4 bytes of 14h behind them)
//   24D7  the colour RAM of the maze (fills and, for the first maze part, the coloured rows)
// The long ones can be cut by the interrupt (N17, sliced.h): they run whole loop rounds that end before the limit and leave the machine at the loop head
// (241F, 245C, 2498, 24E7) with exactly the registers, flags, WZ and stack bytes the ROM has there. No ROM bytes: the tables are read through Mem.
// Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "helpers.h"
#include "screen.h"
#include "numbers.h"
#include "sliced.h"
#include "text.h"
#include "status.h"

namespace game {
namespace flag {
inline uint8_t sbc16(uint16_t hl, uint16_t de, unsigned carry, uint16_t *result) { // SBC HL,DE
    const unsigned r = (unsigned)hl - de - carry;
    uint8_t f = N;
    f |= (uint8_t)((r >> 8) & (S | XY));
    if ((r & 0xFFFF) == 0) f |= Z;
    if ((hl & 0x0FFF) < (unsigned)(de & 0x0FFF) + carry) f |= H;
    if (((hl ^ de) & (hl ^ r) & 0x8000) != 0) f |= PV;
    if (hl < (unsigned)de + carry) f |= C;
    *result = (uint16_t)r;
    return f;
}
inline uint8_t logic8(uint8_t r, bool h) { // flags of AND / XOR / OR on a result r
    uint8_t f = (uint8_t)((r & (S | XY)) | parity_flag(r));
    if (r == 0) f |= Z;
    if (h) f |= H;
    return f;
}
inline uint8_t rl8(uint8_t v, unsigned carry, uint8_t *result) { // RL (IX+d)
    const uint8_t r = (uint8_t)((v << 1) | carry);
    *result = r;
    uint8_t f = logic8(r, false);
    if (v & 0x80) f |= C;
    return f;
}
inline uint8_t rlca(uint8_t *a, uint8_t old_f) {
    const uint8_t c = *a >> 7;
    *a = (uint8_t)((*a << 1) | c);
    return (uint8_t)((old_f & (S | Z | PV)) | (*a & XY) | c);
}
// LDI: (DE) = (HL), HL + 1, DE + 1, BC - 1. S, Z, C keep; X and Y from A + the byte; P/V = BC not zero.
inline uint8_t ldi_flags(uint8_t a, uint8_t value, uint16_t bc_after, uint8_t old_f) {
    const uint8_t t = (uint8_t)(a + value);
    return (uint8_t)((old_f & (S | Z | C)) | ((t << 4) & 0x20) | (t & 0x08) | (bc_after != 0 ? PV : 0));
}
} // namespace flag

constexpr uint16_t kMazeTable = 0x3435;          // addresses of the tables in the player's ROM (not their contents)
constexpr uint16_t kDotOffsetTable = 0x35B5;
constexpr uint16_t kDotBits = ram::pill_array_p1;
constexpr uint16_t kEnergizerBits = ram::energizer_array_p1;
constexpr uint16_t kEnergizerScreen[4] = {0x4064, 0x4078, 0x4384, 0x4398};

// A snapshot of the registers the loops use (IX and IY too).
struct LoopRegs {
    Regs r;
    uint16_t ix = 0, iy = 0;
    uint16_t bc() const { return (uint16_t)(r.b << 8 | r.c); }
    void set_bc(uint16_t v) { r.b = (uint8_t)(v >> 8); r.c = (uint8_t)v; }
};

struct LoopResult {
    bool finished = false;   // ran to the RET: the caller returns like RET
    unsigned units = 0;      // loop rounds done
    uint16_t wz = 0;         // the Z80's WZ at the loop head after those rounds (when units > 0)
};

// ---- 2419 (the maze): loop head 241F. State at the head: HL = the position, BC = the table pointer; (A, F and DE come from the round before).
// A round: 181 T-states / 25 fetches (table byte with bit 7 set: the tile code itself), 222 / 31 (a positive byte: skip that many positions first). The end byte 0: 22 / 3 and RET.
template <class Mem> inline LoopResult maze_loop_241f(Mem &mem, Regs &r, uint16_t sp, unsigned room, Cost &cost) {
    LoopResult res;
    for (;;) {
        uint16_t bc = (uint16_t)(r.b << 8 | r.c);
        const uint8_t v = mem.rd(bc);
        if (v == 0) {
            if (cost.cycles + 22 >= room) return res;
            r.a = 0;
            r.f = flag::and_a(0);
            cost.add(22, 3);
            res.finished = true;
            return res;
        }
        const bool negative = (v & 0x80) != 0;
        const unsigned cycles = negative ? 181 : 222, fetches = negative ? 25 : 31;
        if (cost.cycles + cycles >= room) return res;
        r.a = v;
        r.f = flag::and_a(v);
        if (!negative) {
            r.e = r.a; r.d = 0;
            uint16_t t;
            r.f = flag::add16(r.hl(), r.de(), r.f, &t);
            r.set_hl((uint16_t)(t - 1));
            bc = (uint16_t)(bc + 1);
            r.a = mem.rd(bc);
        }
        r.set_hl((uint16_t)(r.hl() + 1));
        mem.wr(r.hl(), r.a);
        put_word(mem, (uint16_t)(sp - 2), (uint16_t)(r.a << 8 | r.f));    // PUSH AF
        put_word(mem, (uint16_t)(sp - 4), r.hl());                         // PUSH HL
        r.set_de(0x83E0);
        r.a = r.l;
        r.a &= 0x1F; r.f = flag::and_a(r.a);
        uint8_t twice;
        r.f = flag::add8(r.a, r.a, 0, &twice); r.a = twice;
        r.h = 0; r.l = r.a;
        uint16_t t;
        r.f = flag::add16(r.hl(), r.de(), r.f, &t); r.set_hl(t);
        r.set_de(get_word(mem, (uint16_t)(sp - 4)));                       // POP DE
        r.f = flag::and_a(r.a);
        r.f = flag::sbc16(r.hl(), r.de(), 0, &t); r.set_hl(t);
        const uint16_t af = get_word(mem, (uint16_t)(sp - 2));             // POP AF
        r.a = af >> 8; r.f = (uint8_t)af;
        r.a ^= 1;
        r.f = flag::logic8(r.a, false);
        mem.wr(r.hl(), r.a);
        const uint16_t pos = r.de(), mirror = r.hl();                      // EX DE,HL
        r.set_hl(pos); r.set_de(mirror);
        bc = (uint16_t)(bc + 1);
        r.b = (uint8_t)(bc >> 8); r.c = (uint8_t)bc;
        cost.add(cycles, fetches);
        ++res.units;
        res.wz = 0x241F;
    }
}

// ---- 2448 (the dots): loop head 245C (a bit of the byte at IX is to be done; A = the byte rotated so far, C = bits left, B = bytes left including this one).
// A bit: 72 T-states / 9 fetches (no dot), 77 / 10 (dot); the last bit of a byte: 5 less, then INC IX / DEC B / JR NZ (+26) and, when bytes are left, LD C,8 / LD A,(IX+0) (+26 / 3 fetches).
// After the last byte: the four LDI (124 T-states / 14 fetches with RET).
template <class Mem> inline LoopResult dots_loop_245c(Mem &mem, LoopRegs &l, unsigned room, Cost &cost) {
    Regs &r = l.r;
    LoopResult res;
    for (;;) {
        const bool dot = (r.a & 0x80) != 0;
        const bool last_bit = r.c == 1;
        const bool last_byte = last_bit && r.b == 1;
        unsigned cycles = 34 + (dot ? 17 : 12) + 10 + 4 + (last_bit ? 7 : 12);
        unsigned fetches = 2 + 1 + 1 + (dot ? 2 : 1) + 2 + 1 + 1;
        if (last_bit) {
            cycles += 10 + 4 + (last_byte ? 7 : 12 + 7 + 19);
            fetches += 2 + 1 + 1 + (last_byte ? 0 : 3);
        }
        const unsigned tail_cycles = last_byte ? 124 : 0, tail_fetches = last_byte ? 14 : 0;
        if (cost.cycles + cycles + tail_cycles >= room) return res;
        r.e = mem.rd(l.iy);
        uint16_t t;
        r.f = flag::add16(r.hl(), r.de(), r.f, &t); r.set_hl(t);
        r.f = flag::rlca(&r.a, r.f);
        if (dot) mem.wr(r.hl(), 0x10);
        l.iy = (uint16_t)(l.iy + 1);
        r.f = flag::dec8(r.c, r.f); r.c = (uint8_t)(r.c - 1);
        res.wz = 0x245C;
        if (r.c == 0) {
            l.ix = (uint16_t)(l.ix + 1);
            r.f = flag::dec8(r.b, r.f); r.b = (uint8_t)(r.b - 1);
            if (r.b != 0) {
                res.wz = 0x2457;
                r.c = 8;
                r.a = mem.rd(l.ix);
                res.wz = l.ix;
            }
        }
        cost.add(cycles, fetches);
        ++res.units;
        if (last_byte) {
            uint16_t bc = 0;
            for (int i = 0; i < 4; ++i) {
                const uint16_t from = (uint16_t)(kEnergizerBits + i);
                const uint8_t value = mem.rd(from);
                mem.wr(kEnergizerScreen[i], value);
                bc = (uint16_t)(bc - 1);
                r.f = flag::ldi_flags(r.a, value, bc, r.f);
            }
            r.set_hl((uint16_t)(kEnergizerBits + 4));
            r.set_de((uint16_t)(kEnergizerScreen[3] + 1));
            r.b = 0xFF; r.c = 0xFC;
            cost.add(tail_cycles, tail_fetches);
            res.finished = true;
            return res;
        }
    }
}

// ---- 2487 (collect the dots): loop head 2498. A bit: 97 / 13 (screen tile is a dot, 10h) or 96 / 14, +12 / +7 for the JR; the last bit of a byte, as above.
// After the last byte: the four LDI (124 / 14 with RET).
template <class Mem> inline LoopResult collect_loop_2498(Mem &mem, LoopRegs &l, unsigned room, Cost &cost) {
    Regs &r = l.r;
    LoopResult res;
    for (;;) {
        const bool last_bit = r.c == 1;
        const bool last_byte = last_bit && r.b == 1;
        // the cost of the round depends on the tile that is read
        const uint8_t e_new = mem.rd(l.iy);
        uint16_t t;
        const uint16_t hl_next = (uint16_t)(r.hl() + (uint16_t)(r.d << 8 | e_new));
        const uint8_t tile = mem.rd(hl_next);
        const bool dot = tile == 0x10;
        unsigned cycles = (dot ? 97 : 96) + (last_bit ? 7 : 12);
        unsigned fetches = dot ? 13 : 14;
        fetches += 0;
        if (last_bit) {
            cycles += 10 + 4 + (last_byte ? 7 : 12 + 7);
            fetches += 2 + 1 + 1 + (last_byte ? 0 : 1);
        }
        const unsigned tail_cycles = last_byte ? 124 : 0, tail_fetches = last_byte ? 14 : 0;
        if (cost.cycles + cycles + tail_cycles >= room) return res;
        r.e = e_new;
        r.f = flag::add16(r.hl(), r.de(), r.f, &t); r.set_hl(t);
        r.a = tile;
        r.f = flag::cp8(r.a, 0x10);
        const unsigned carry = dot ? 1 : 0;
        uint8_t shifted;
        const uint8_t before = mem.rd(l.ix);
        r.f = flag::rl8(before, carry, &shifted);
        mem.wr(l.ix, shifted);
        l.iy = (uint16_t)(l.iy + 1);
        r.f = flag::dec8(r.c, r.f); r.c = (uint8_t)(r.c - 1);
        res.wz = 0x2498;
        if (r.c == 0) {
            l.ix = (uint16_t)(l.ix + 1);
            r.f = flag::dec8(r.b, r.f); r.b = (uint8_t)(r.b - 1);
            if (r.b != 0) { r.c = 8; res.wz = 0x2496; }
        }
        cost.add(cycles, fetches);
        ++res.units;
        if (last_byte) {
            uint16_t bc = 0;
            static const uint16_t from[4] = {0x4064, 0x4078, 0x4384, 0x4398};
            for (int i = 0; i < 4; ++i) {
                const uint8_t value = mem.rd(from[i]);
                mem.wr((uint16_t)(kEnergizerBits + i), value);
                bc = (uint16_t)(bc - 1);
                r.f = flag::ldi_flags(r.a, value, bc, r.f);
            }
            r.set_hl((uint16_t)(0x4398 + 1));
            r.set_de((uint16_t)(kEnergizerBits + 4));
            r.b = 0xFF; r.c = 0xFC;
            cost.add(tail_cycles, tail_fetches);
            res.finished = true;
            return res;
        }
    }
}

} // namespace game
