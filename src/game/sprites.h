// N20: the staging of the sprite registers inside the vertical-blank interrupt handler, written natively (no Z80). ROM code replaced (entry 009D, straight through to 018B,
// the CALL at 018C is the next ROM instruction):
//   009D  copy 16 bytes 4E8C -> 5050; pick the three sprite codes for the ghosts (4ECC/4ECF/4E9F, 4EDC/4EDF/4EAF, 4EEC/4EEF/4EBF) into 5045, 504A, 504F
//   00D5  copy the actors' sprite pairs 4C02.. (28 bytes) to 4C22..; turn the six position words' sprite-flip bits (rotate left twice: the +2, +4, ... +0Ch bytes)
//   0114  when 4DD1 = 1 (the demo / attract), exchange the entries picked by 4DA4 with 4C24/4C34 (the order of the sprite slots)
//   0153  when 4DA6 <> 0 (ghosts blue), swap the sprite pairs 4C22/4C32 with 4C2A/4C3A (the order of the sprite slots)
//   0176  copy the staged sprite registers 4C22 -> 4FF2 (12 bytes) and 4C32 -> 5062 (12 bytes)
// Each instruction is accounted for (T-states, opcode fetches, WZ, flags). No ROM bytes: addresses and counts only. Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "helpers.h"
#include "maze.h"
#include "text.h"

namespace game {

struct SpriteState {
    uint8_t a = 0, f = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    uint16_t ix = 0, wz = 0;
    uint16_t hl() const { return (uint16_t)(h << 8 | l); }
    uint16_t de() const { return (uint16_t)(d << 8 | e); }
    uint16_t bc() const { return (uint16_t)(b << 8 | c); }
    void set_hl(uint16_t v) { h = (uint8_t)(v >> 8); l = (uint8_t)v; }
    void set_de(uint16_t v) { d = (uint8_t)(v >> 8); e = (uint8_t)v; }
    void set_bc(uint16_t v) { b = (uint8_t)(v >> 8); c = (uint8_t)v; }
};

namespace spr {
template <class Mem> inline uint16_t word(Mem &mem, uint16_t at) { return (uint16_t)(mem.rd(at) | (mem.rd((uint16_t)(at + 1)) << 8)); }
// LDIR with HL, DE, BC = n: the cost of n repeats (21 T-states each, the last one 16) and 2 fetches per repeat
template <class Mem> inline void ldir(Mem &mem, SpriteState &s, uint16_t at, Cost &c) {
    const unsigned n = s.bc() ? s.bc() : 65536u;
    for (unsigned i = 0; i < n; ++i) {
        const uint8_t v = mem.rd(s.hl());
        mem.wr(s.de(), v);
        s.set_hl((uint16_t)(s.hl() + 1));
        s.set_de((uint16_t)(s.de() + 1));
        s.set_bc((uint16_t)(s.bc() - 1));
        s.f = flag::ldi_flags(s.a, v, s.bc(), s.f);
    }
    if (n > 1) s.wz = (uint16_t)(at + 1);
    c.add(21 * (n - 1) + 16, 2 * n);
}
// the three-way pick: LD A,(p) / AND A / LD A,(q) / JR NZ / [LD A,(r)] / LD (out),A
template <class Mem> inline void pick(Mem &mem, SpriteState &s, uint16_t p, uint16_t q, uint16_t r, uint16_t out, uint16_t jr_target, Cost &c) {
    s.a = mem.rd(p); s.wz = (uint16_t)(p + 1); c.add(13, 1);
    s.f = flag::and_a(s.a); c.add(4, 1);
    const bool nz = s.a != 0;
    s.a = mem.rd(q); s.wz = (uint16_t)(q + 1); c.add(13, 1);
    if (nz) { s.wz = jr_target; c.add(12, 1); }
    else { c.add(7, 1); s.a = mem.rd(r); s.wz = (uint16_t)(r + 1); c.add(13, 1); }
    mem.wr(out, s.a);
    s.wz = (uint16_t)(((out + 1) & 0xFF) | (s.a << 8));
    c.add(13, 1);
}
template <class Mem> inline void ld_hl_nn(Mem &mem, SpriteState &s, uint16_t nn, Cost &c) { s.set_hl(word(mem, nn)); s.wz = (uint16_t)(nn + 1); c.add(16, 1); }
} // namespace spr

// 009D .. 018B. Returns the cost; the machine goes on at 018C.
template <class Mem> inline Cost sprite_stage_009d(Mem &mem, SpriteState &s) {
    Cost c;
    using namespace spr;
    // 009D: LD HL / LD DE / LD BC / LDIR (16)
    s.set_hl(ram::sound_registers_shadow); s.set_de(0x5050); s.set_bc(0x0010); c.add(30, 3);
    ldir(mem, s, 0x00A6, c);
    pick(mem, s, ram::voice1_effect_b, (ram::voice1_effect_b + 3), (ram::voice1_effect_a + 3), 0x5045, 0x00B4, c);
    pick(mem, s, ram::voice2_effect_b, (ram::voice2_effect_b + 3), (ram::voice2_effect_a + 3), 0x504A, 0x00C3, c);
    pick(mem, s, ram::voice3_effect_b, (ram::voice3_effect_b + 3), (ram::voice3_effect_a + 3), 0x504F, 0x00D2, c);
    // 00D5: LD HL,4C02 / LD DE,4C22 / LD BC,001C / LDIR
    s.set_hl(ram::sprite_data); s.set_de(ram::sprite_codes_copy); s.set_bc(0x001C); c.add(30, 3);
    ldir(mem, s, 0x00DE, c);
    // 00E0: LD IX,4C20; six times LD A,(IX+d) / RLCA / RLCA / LD (IX+d),A
    s.ix = ram::byte_4C20; c.add(14, 2);
    for (uint8_t d = 2; d <= 0x0C; d = (uint8_t)(d + 2)) {
        s.a = mem.rd((uint16_t)(s.ix + d)); s.wz = (uint16_t)(s.ix + d);
        s.f = flag::rlca(&s.a, s.f);
        s.f = flag::rlca(&s.a, s.f);
        mem.wr((uint16_t)(s.ix + d), s.a); s.wz = (uint16_t)(s.ix + d);
        c.add(46, 6);
    }
    // 0114: LD A,(4DD1) / CP 1 / JR NZ,0153
    s.a = mem.rd(ram::byte_4DD1); s.wz = ram::fruit_position; c.add(13, 1);
    s.f = flag::cp8(s.a, 1); c.add(7, 1);
    if (s.a != 1) { s.wz = 0x0153; c.add(12, 1); }
    else {
        c.add(7, 1);
        s.ix = ram::byte_4C20; c.add(14, 2);
        s.a = mem.rd(ram::ghost_touched); s.wz = ram::pacman_dying; c.add(13, 1);
        { uint8_t t; s.f = flag::add8(s.a, s.a, 0, &t); s.a = t; c.add(4, 1); }
        s.e = s.a; c.add(4, 1);
        s.d = 0; c.add(7, 1);
        { uint16_t t; s.wz = (uint16_t)(s.ix + 1); s.f = flag::add16(s.ix, s.de(), s.f, &t); s.ix = t; c.add(15, 2); }
        ld_hl_nn(mem, s, (ram::sprite_codes_copy + 2), c);
        s.set_de(word(mem, (ram::sprite_coords_copy + 2))); s.wz = (ram::sprite_coords_copy + 3); c.add(20, 2);
        s.a = mem.rd(s.ix); s.wz = s.ix; mem.wr((ram::sprite_codes_copy + 2), s.a); s.wz = (uint16_t)(0x25 | (s.a << 8)); c.add(19 + 13, 3);
        s.a = mem.rd((uint16_t)(s.ix + 1)); s.wz = (uint16_t)(s.ix + 1); mem.wr((ram::sprite_codes_copy + 3), s.a); s.wz = (uint16_t)(0x26 | (s.a << 8)); c.add(19 + 13, 3);
        s.a = mem.rd((uint16_t)(s.ix + 0x10)); s.wz = (uint16_t)(s.ix + 0x10); mem.wr((ram::sprite_coords_copy + 2), s.a); s.wz = (uint16_t)(0x35 | (s.a << 8)); c.add(19 + 13, 3);
        s.a = mem.rd((uint16_t)(s.ix + 0x11)); s.wz = (uint16_t)(s.ix + 0x11); mem.wr((ram::sprite_coords_copy + 3), s.a); s.wz = (uint16_t)(0x36 | (s.a << 8)); c.add(19 + 13, 3);
        mem.wr(s.ix, s.l); mem.wr((uint16_t)(s.ix + 1), s.h); mem.wr((uint16_t)(s.ix + 0x10), s.e); mem.wr((uint16_t)(s.ix + 0x11), s.d);
        s.wz = (uint16_t)(s.ix + 0x11); c.add(76, 8);
    }
    // 0153: LD A,(4DA6) / AND A / JP Z,0176
    s.a = mem.rd(ram::frightened); s.wz = ram::blinky_edible; c.add(13, 1);
    s.f = flag::and_a(s.a); c.add(4, 1);
    s.wz = 0x0176; c.add(10, 1);
    if (s.a != 0) {
        s.set_bc(word(mem, ram::sprite_codes_copy)); s.wz = (ram::sprite_codes_copy + 1); c.add(20, 2);
        s.set_de(word(mem, ram::sprite_coords_copy)); s.wz = (ram::sprite_coords_copy + 1); c.add(20, 2);
        ld_hl_nn(mem, s, (ram::sprite_codes_copy + 8), c);
        put_word(mem, ram::sprite_codes_copy, s.hl()); s.wz = (ram::sprite_codes_copy + 1); c.add(16, 1);
        ld_hl_nn(mem, s, (ram::sprite_coords_copy + 8), c);
        put_word(mem, ram::sprite_coords_copy, s.hl()); s.wz = (ram::sprite_coords_copy + 1); c.add(16, 1);
        put_word(mem, (ram::sprite_codes_copy + 8), s.bc()); s.wz = (ram::sprite_codes_copy + 9); c.add(20, 2);
        put_word(mem, (ram::sprite_coords_copy + 8), s.de()); s.wz = (ram::sprite_coords_copy + 9); c.add(20, 2);
    }
    // 0176: two blocks of 12 bytes
    s.set_hl(ram::sprite_codes_copy); s.set_de(0x4FF2); s.set_bc(0x000C); c.add(30, 3);
    ldir(mem, s, 0x017F, c);
    s.set_hl(ram::sprite_coords_copy); s.set_de(0x5062); s.set_bc(0x000C); c.add(30, 3);
    ldir(mem, s, 0x018A, c);
    return c;
}

} // namespace game
