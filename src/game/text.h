// N15: the text printer, written natively (no Z80). ROM routine replaced (entry): 2C5E  print the text string number B from the text table.
// The ROM routine: the string table at 36A5 gives (RST 18h, index B) the address of a string record: a word (the screen offset), a byte (bit 7 set: the text
// runs along a row = down the memory by 1, clear: along a column = by 20h), then the characters up to the end mark 2Fh, then a second part that goes to the colour RAM:
// either one byte with bit 7 set (the colour, repeated for every character) or one colour byte per character. B with bit 7 set means "erase": the characters are
// replaced by 40h (blank) and the colour part is skipped with a CPIR for the next 2Fh. The characters go to 4000h + offset (video RAM), the colours to 4400h + offset.
// The tables are the player's ROM at run time: every byte is read through Mem (the bus), no ROM bytes are in the source.
// Mem: uint8_t rd(uint16_t), void wr(uint16_t, uint8_t).
#pragma once
#include <cstdint>
#include "helpers.h"
#include "screen.h"

namespace game {

constexpr uint16_t kTextTableAddress = 0x36A5;   // the table of string addresses (an address, not ROM content)
constexpr uint8_t kTextEndMark = 0x2F;

template <class Mem> inline void put_word(Mem &mem, uint16_t at, uint16_t v) { mem.wr(at, (uint8_t)v); mem.wr((uint16_t)(at + 1), (uint8_t)(v >> 8)); }

// In: r.b = the string number (bit 7 = erase), sp = the stack pointer at the entry (the return address is at sp).
// Out: registers (A, F, B, C, D, E, H, L), ix = the IX register at the end. 'cost' = the whole routine including its RET.
template <class Mem> inline void text_2c5e(Mem &mem, Regs &r, uint16_t &ix, uint16_t sp, Cost &cost) {
    // LD HL,36A5 / RST 18h (pushes 2C62; the RST 10h inside pushes 001B one slot lower)
    r.set_hl(kTextTableAddress);
    put_word(mem, (uint16_t)(sp - 2), 0x2C62);
    put_word(mem, (uint16_t)(sp - 4), 0x001B);
    Cost inner;
    table_word_0018(mem, r, inner);
    cost.add(10 + 11 + kTableWordCycles, 1 + 1 + kTableWordFetches);
    // LD E,(HL) / INC HL / LD D,(HL)
    r.e = mem.rd(r.hl());
    r.set_hl((uint16_t)(r.hl() + 1));
    r.d = mem.rd(r.hl());
    cost.add(7 + 6 + 7, 3);
    // LD IX,4400 / ADD IX,DE / PUSH IX / LD DE,FC00 / ADD IX,DE / LD DE,FFFF
    ix = 0x4400;
    r.f = flag::add16(ix, r.de(), r.f, &ix);
    cost.add(14 + 15 + 15, 2 + 2 + 2);
    put_word(mem, (uint16_t)(sp - 2), ix);
    r.set_de(0xFC00);
    r.f = flag::add16(ix, r.de(), r.f, &ix);
    cost.add(10 + 15, 1 + 2);
    r.set_de(0xFFFF);
    cost.add(10, 1);
    // BIT 7,(HL) / JR NZ
    const bool along_row = (mem.rd(r.hl()) & 0x80) != 0;
    cost.add(12, 2);
    if (along_row) cost.add(12, 1);
    else { cost.add(7, 1); r.set_de(0xFFE0); cost.add(10, 1); }
    // INC HL / LD A,B / LD BC,0 / ADD A,A / JR C
    r.set_hl((uint16_t)(r.hl() + 1));
    r.a = r.b;
    r.b = 0; r.c = 0;
    uint8_t twice;
    r.f = flag::add8(r.a, r.a, 0, &twice);
    r.a = twice;
    cost.add(6 + 4 + 10 + 4, 4);
    const bool erase = (r.f & flag::C) != 0;
    if (erase) {
        cost.add(12, 1);
        for (;;) {   // 2CAC: blank every character
            r.a = mem.rd(r.hl());
            r.f = flag::cp8(r.a, kTextEndMark);
            if (r.a == kTextEndMark) { cost.add(7 + 7 + 12, 3); break; }
            mem.wr(ix, 0x40);
            r.set_hl((uint16_t)(r.hl() + 1));
            r.f = flag::add16(ix, r.de(), r.f, &ix);
            r.f = flag::inc8(r.b, r.f);
            r.b = (uint8_t)(r.b + 1);
            cost.add(7 + 7 + 7 + 19 + 6 + 15 + 4 + 12, 10);
        }
        r.set_hl((uint16_t)(r.hl() + 1));            // INC HL
        r.f = flag::inc8(r.b, r.f);
        r.b = (uint8_t)(r.b + 1);                     // INC B
        cost.add(6 + 4, 2);
        uint16_t bc = (uint16_t)(r.b << 8 | r.c);     // CPIR: look for the next end mark
        for (;;) {
            const uint8_t x = mem.rd(r.hl());
            const uint8_t cpf = flag::cp8(r.a, x);
            bc = (uint16_t)(bc - 1);
            const uint8_t t = (uint8_t)(r.a - x - ((cpf & flag::H) ? 1 : 0));
            r.f = (uint8_t)((cpf & (flag::S | flag::Z | flag::H)) | ((t << 4) & 0x20) | (t & 0x08) | (bc != 0 ? flag::PV : 0) | flag::N | (r.f & flag::C));
            r.set_hl((uint16_t)(r.hl() + 1));
            if (bc && !(r.f & flag::Z)) { cost.add(21, 2); continue; }
            cost.add(16, 2);
            break;
        }
        r.b = (uint8_t)(bc >> 8); r.c = (uint8_t)bc;
        cost.add(12, 1);                              // JR 2C93
    } else {
        cost.add(7, 1);
        for (;;) {   // 2C84: the characters
            r.a = mem.rd(r.hl());
            r.f = flag::cp8(r.a, kTextEndMark);
            if (r.a == kTextEndMark) { cost.add(7 + 7 + 12, 3); break; }
            mem.wr(ix, r.a);
            r.set_hl((uint16_t)(r.hl() + 1));
            r.f = flag::add16(ix, r.de(), r.f, &ix);
            r.f = flag::inc8(r.b, r.f);
            r.b = (uint8_t)(r.b + 1);
            cost.add(7 + 7 + 7 + 19 + 6 + 15 + 4 + 12, 10);
        }
        r.set_hl((uint16_t)(r.hl() + 1));            // 2C92 INC HL
        cost.add(6, 1);
    }
    // 2C93: POP IX / LD A,(HL) / AND A / JP M
    ix = (uint16_t)(mem.rd((uint16_t)(sp - 2)) | (mem.rd((uint16_t)(sp - 1)) << 8));
    r.a = mem.rd(r.hl());
    r.f = flag::and_a(r.a);
    cost.add(14 + 7 + 4 + 10, 2 + 1 + 1 + 1);
    if (r.a & 0x80) {   // 2CA4: one colour for all
        for (;;) {
            mem.wr(ix, r.a);
            r.f = flag::add16(ix, r.de(), r.f, &ix);
            r.b = (uint8_t)(r.b - 1);
            if (r.b) cost.add(19 + 15 + 13, 5); else { cost.add(19 + 15 + 8, 5); break; }
        }
    } else {            // 2C9A: one colour per character
        for (;;) {
            r.a = mem.rd(r.hl());
            mem.wr(ix, r.a);
            r.set_hl((uint16_t)(r.hl() + 1));
            r.f = flag::add16(ix, r.de(), r.f, &ix);
            r.b = (uint8_t)(r.b - 1);
            if (r.b) cost.add(7 + 19 + 6 + 15 + 13, 7); else { cost.add(7 + 19 + 6 + 15 + 8, 7); break; }
        }
    }
    cost.add(10, 1);    // RET
}

// A cheap upper bound of the routine's time, from reading the string without writing: returns 0 when it cannot be bounded (the adapter then lets the ROM run).
// The scan only reads below 4000h (the ROM): a pointer into RAM could be changed by the routine's own writes.
template <class Mem> inline unsigned text_bound_2c5e(Mem &mem, Regs r) {
    r.set_hl(kTextTableAddress);
    Cost c;
    table_word_0018(mem, r, c);
    const unsigned rec = r.hl();
    unsigned p = rec + 3, first = 0;
    for (;; ++p, ++first) {
        if (p >= 0x4000 || first > 255) return 0;
        if (mem.rd((uint16_t)p) == kTextEndMark) break;
    }
    ++p;
    const unsigned n = first ? first : 256u;     // B counts the characters; 0 means 256 rounds
    if (!(r.b & 0x80)) return 400 + n * 80 + n * 62;
    unsigned second = 0;                          // erase: the CPIR looks for the next end mark (at most 256 rounds: B is at most first + 1)
    for (; second <= 300; ++second) {
        if (p + second >= 0x4000) return 0;
        if (mem.rd((uint16_t)(p + second)) == kTextEndMark) break;
    }
    return 400 + n * 80 + (second + 2) * 21 + 257u * 62;
}

} // namespace game
