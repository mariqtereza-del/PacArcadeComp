// N21: the sprite pictures (which picture and colour each sprite shows), written natively (no Z80). They are called from the interrupt handler (ROM 019E and 01A1) and run
// in the actors' staged sprite block (IX = 4C00 in the game). ROM blocks replaced (entry addresses):
//   141F / 1490  positions of the actors -> the sprite position bytes (+12h .. +1Dh) for the two screen orientations, then on to 14FE
//   14FE         decide: ghosts only (154B), the blue-ghost version (15B4), or Pac-Man's picture by direction 4D30 (the table at 1514, entered by RST 20h)
//   168C 16B1 16D6 16F7   Pac-Man's mouth picture for the four directions (the table targets)
//   151C         after Pac-Man's picture: the "flip" bits of the sprite byte +0Ah for the directions left / up
//   154B         the four ghosts' pictures (colour step 4DC0, blue 4DAC.., flashing, eyes by 4D2C..4D2F)
//   15B4 15B7 15BA   the three CALLs 15E6, 162D, 1652 (each is a replaced block too)
//   15BD         set the two top bits of the sprite bytes +2, +4, +6, +8, +0C when B <> 0
//   15E6 162D 1652   the fruit picture and colour, the score numbers after eating, the Pac-Man death animation pictures
// Every instruction is accounted for (T-states, opcode fetches, flags, WZ). No ROM bytes: addresses and values only. Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "helpers.h"
#include "maze.h"

namespace game {
namespace spp {

enum Kind { kContinue, kRet, kCall, kRst20 };
struct Result { Kind kind = kContinue; uint16_t next = 0; uint16_t ret_addr = 0; uint16_t leftover = 0; };   // leftover: a return address a CALL left at sp-2 (0 = none)

template <class Mem> struct Z {
    Mem &mem;
    uint8_t a, f, b, c, d, e, h, l;
    uint16_t ix, iy, wz;
    uint16_t abc = 0, ade = 0, ahl = 0;   // N47a: the shadow registers (EXX)
    Cost cost;
    Z(Mem &m) : mem(m), a(0), f(0), b(0), c(0), d(0), e(0), h(0), l(0), ix(0), iy(0), wz(0) {}
    void t(unsigned cyc, unsigned fet = 1) { cost.add(cyc, fet); }
    uint16_t at(int8_t d8) const { return (uint16_t)(ix + (int8_t)d8); }
    void ld_a_nn(uint16_t nn) { a = mem.rd(nn); wz = (uint16_t)(nn + 1); t(13); }
    void ld_nn_a(uint16_t nn) { mem.wr(nn, a); wz = (uint16_t)(((nn + 1) & 0xFF) | (a << 8)); t(13); }
    void ld_a_ixd(uint8_t d8) { a = mem.rd(at(d8)); wz = at(d8); t(19, 2); }
    void ld_ixd(uint8_t d8, uint8_t v) { mem.wr(at(d8), v); wz = at(d8); t(19, 2); }   // LD (IX+d),r / LD (IX+d),n
    void and_a() { f = flag::and_a(a); t(4); }
    void and_n(uint8_t n) { a &= n; f = flag::logic8(a, true); t(7); }
    void and_r(uint8_t v) { a &= v; f = flag::logic8(a, true); t(4); }
    void or_r(uint8_t v) { a |= v; f = flag::logic8(a, false); t(4); }
    void xor_r(uint8_t v) { a ^= v; f = flag::logic8(a, false); t(4); }
    void cpl() { a = (uint8_t)~a; f = (uint8_t)((f & (flag::S | flag::Z | flag::PV | flag::C)) | flag::H | flag::N | (a & flag::XY)); t(4); }
    void add_a(uint8_t v, unsigned cyc = 4) { uint8_t r; f = flag::add8(a, v, 0, &r); a = r; t(cyc); }
    void sub_n(uint8_t n) { uint8_t r; f = flag::sub8(a, n, &r); a = r; t(7); }
    void cp_n(uint8_t n) { f = flag::cp8(a, n); t(7); }
    void inc_a() { a = (uint8_t)(a + 1); f = flag::inc8((uint8_t)(a - 1), f); t(4); }
    void inc_d() { f = flag::inc8(d, f); d = (uint8_t)(d + 1); t(4); }
    void rrca() { const uint8_t c0 = a & 1; a = (uint8_t)((a >> 1) | (c0 << 7)); f = (uint8_t)((f & (flag::S | flag::Z | flag::PV)) | (a & flag::XY) | c0); t(4); }
    void bit(int n, uint8_t v) {
        const bool set = (v >> n) & 1;
        f = (uint8_t)((f & flag::C) | flag::H | (v & flag::XY));
        if (!set) f |= flag::Z | flag::PV;
        if (n == 7 && set) f |= flag::S;
        t(8, 2);
    }
    // jumps: the cost is added by the caller's choice of taken / not taken
    bool jr(bool taken, uint16_t target) { if (taken) { wz = target; t(12); } else t(7); return taken; }
    void jp(uint16_t target) { wz = target; t(10); }
    // N22 additions: operations on (HL)/(addr) and 16-bit registers
    uint16_t hl() const { return (uint16_t)(h << 8 | l); }
    uint16_t de() const { return (uint16_t)(d << 8 | e); }
    void set_hl(uint16_t v) { h = (uint8_t)(v >> 8); l = (uint8_t)v; }
    void set_de(uint16_t v) { d = (uint8_t)(v >> 8); e = (uint8_t)v; }
    void inc_mem(uint16_t ad) { const uint8_t v = mem.rd(ad); f = flag::inc8(v, f); mem.wr(ad, (uint8_t)(v + 1)); t(11); }
    void dec_mem(uint16_t ad) { const uint8_t v = mem.rd(ad); f = flag::dec8(v, f); mem.wr(ad, (uint8_t)(v - 1)); t(11); }
    void inc16_hl() { set_hl((uint16_t)(hl() + 1)); t(6); }
    void inc16_de() { set_de((uint16_t)(de() + 1)); t(6); }
    void inc_l() { f = flag::inc8(l, f); l = (uint8_t)(l + 1); t(4); }
    void inc_c() { f = flag::inc8(c, f); c = (uint8_t)(c + 1); t(4); }
    void inc_e() { f = flag::inc8(e, f); e = (uint8_t)(e + 1); t(4); }
    void ex_de_hl() { const uint16_t x = hl(); set_hl(de()); set_de(x); t(4); }
    void cp_mem(uint16_t ad) { f = flag::cp8(a, mem.rd(ad)); t(7); }
    void add_a_mem(uint16_t ad) { add_a(mem.rd(ad), 7); }
    void ld_mem_r(uint16_t ad, uint8_t v) { mem.wr(ad, v); t(7); }
    void ld_mem_n(uint16_t ad, uint8_t v) { mem.wr(ad, v); t(10); }
    void ld_a_mem(uint16_t ad) { a = mem.rd(ad); t(7); }
    Result go(uint16_t pc) { Result r; r.kind = kContinue; r.next = pc; return r; }
    Result ret(unsigned cyc = 10) { t(cyc); Result r; r.kind = kRet; return r; }
};

// ---- 141F and 1490: positions -> sprite position bytes
struct Pair { uint16_t src; bool cpl; char reg; uint8_t dst; };
template <class Mem> inline Result positions(Z<Mem> &z, bool first) {
    z.ld_a_nn(ram::cocktail_mode); z.b = z.a; z.t(4);
    z.ld_a_nn(ram::current_player); z.and_r(z.b);
    if (first) { if (z.f & flag::Z) return z.ret(11); z.t(5); }       // 141F: RET Z
    else { if (!(z.f & flag::Z)) return z.ret(11); z.t(5); }           // 1490: RET NZ
    z.b = z.a; z.t(4);
    if (first) { z.ix = 0x4C00; z.t(14, 2); z.e = 8; z.t(7); z.c = 8; z.t(7); z.d = 7; z.t(7); }
    else { z.e = 9; z.t(7); z.c = 7; z.t(7); z.d = 6; z.t(7); z.ix = 0x4C00; z.t(14, 2); }
    static const Pair p1[12] = {{ram::blinky_pixel, false, 'e', 0x13}, {(ram::blinky_pixel + 1), true, 'd', 0x12}, {ram::pinky_pixel, false, 'e', 0x15}, {(ram::pinky_pixel + 1), true, 'd', 0x14}, {ram::inky_pixel, false, 'e', 0x17}, {(ram::inky_pixel + 1), true, 'c', 0x16},
                                {ram::clyde_pixel, false, 'e', 0x19}, {(ram::clyde_pixel + 1), true, 'c', 0x18}, {ram::pacman_pixel, false, 'e', 0x1B}, {(ram::pacman_pixel + 1), true, 'c', 0x1A}, {ram::fruit_position, false, 'e', 0x1D}, {(ram::fruit_position + 1), true, 'c', 0x1C}};
    static const Pair p2[12] = {{ram::blinky_pixel, true, 'e', 0x13}, {(ram::blinky_pixel + 1), false, 'd', 0x12}, {ram::pinky_pixel, true, 'e', 0x15}, {(ram::pinky_pixel + 1), false, 'd', 0x14}, {ram::inky_pixel, true, 'e', 0x17}, {(ram::inky_pixel + 1), false, 'c', 0x16},
                                {ram::clyde_pixel, true, 'e', 0x19}, {(ram::clyde_pixel + 1), false, 'c', 0x18}, {ram::pacman_pixel, true, 'e', 0x1B}, {(ram::pacman_pixel + 1), false, 'c', 0x1A}, {ram::fruit_position, true, 'e', 0x1D}, {(ram::fruit_position + 1), false, 'c', 0x1C}};
    const Pair *p = first ? p1 : p2;
    for (int i = 0; i < 12; ++i) {
        z.ld_a_nn(p[i].src);
        if (p[i].cpl) z.cpl();
        z.add_a(p[i].reg == 'e' ? z.e : p[i].reg == 'd' ? z.d : z.c);
        z.ld_ixd(p[i].dst, z.a);
    }
    if (first) { z.jp(0x14FE); }
    return z.go(0x14FE);
}

// ---- 14FE
template <class Mem> inline Result decide_14fe(Z<Mem> &z) {
    z.ld_a_nn(ram::pacman_dying); z.and_a(); z.jp(0x154B);
    if (z.a != 0) return z.go(0x154B);
    z.ld_a_nn(ram::ghost_touched); z.and_a(); z.jp(0x15B4);
    if (z.a != 0) return z.go(0x15B4);
    z.h = 0x15; z.l = 0x1C; z.t(10);          // LD HL,151C
    z.t(11);                                   // PUSH HL (the adapter writes the bytes)
    z.ld_a_nn(ram::pacman_direction);
    z.t(11); z.wz = 0x0020;                    // RST 20h
    Result r; r.kind = kRst20; r.next = 0x0020; return r;
}

// ---- 151C
template <class Mem> inline Result after_mouth_151c(Z<Mem> &z) {
    z.a = z.b; z.t(4); z.and_a();
    if (z.jr(z.a == 0, 0x154B)) return z.go(0x154B);
    z.c = 0xC0; z.t(7);
    z.ld_a_nn(ram::byte_4C0A); z.d = z.a; z.t(4); z.and_r(z.c);
    if (z.jr(z.a != 0, 0x152E)) goto l152e;
    z.a = z.d; z.t(4); z.or_r(z.c); z.jp(0x1548);
    goto l1548;
l152e:
    z.ld_a_nn(ram::pacman_direction); z.cp_n(2);
    if (z.jr(z.a != 2, 0x153E)) goto l153e;
    z.bit(7, z.d);
    if (z.jr(!((z.d >> 7) & 1), 0x154B)) return z.go(0x154B);
    z.a = z.d; z.t(4); z.xor_r(z.c); z.jp(0x1548);
    goto l1548;
l153e:
    z.cp_n(3);
    if (z.jr(z.a != 3, 0x154B)) return z.go(0x154B);
    z.bit(6, z.d);
    if (z.jr(!((z.d >> 6) & 1), 0x154B)) return z.go(0x154B);
    z.a = z.d; z.t(4); z.xor_r(z.c);
l1548:
    z.ld_nn_a(ram::byte_4C0A);
    return z.go(0x154B);
}

// ---- 154B
template <class Mem> inline Result ghosts_154b(Z<Mem> &z) {
    z.h = 0x4D; z.l = 0xC0; z.t(10);
    z.d = z.mem.rd(ram::byte_4DC0); z.t(7);
    z.a = 0x1C; z.t(7); z.add_a(z.d);
    for (uint8_t off = 2; off <= 8; off = (uint8_t)(off + 2)) z.ld_ixd(off, z.a);
    z.c = 0x20; z.t(7);
    static const uint16_t fa[4] = {ram::blinky_dead, ram::pinky_dead, ram::inky_dead, ram::clyde_dead}, fb[4] = {ram::blinky_edible, ram::pinky_edible, ram::inky_edible, ram::clyde_edible}, src[4] = {ram::blinky_direction, ram::pinky_direction, ram::inky_direction, ram::clyde_direction};
    static const uint16_t do_lbl[4] = {0x156C, 0x1581, 0x1596, 0x15AB}, next_lbl[4] = {0x1575, 0x158A, 0x159F, 0x15B4};
    for (int g = 0; g < 4; ++g) {
        z.ld_a_nn(fa[g]); z.and_a();
        bool do_it = true;
        if (z.jr(z.a != 0, do_lbl[g])) do_it = true;
        else {
            z.ld_a_nn(fb[g]); z.and_a();
            if (z.jr(z.a != 0, next_lbl[g])) do_it = false;
        }
        if (do_it) {
            z.ld_a_nn(src[g]); z.add_a(z.a); z.add_a(z.d); z.add_a(z.c); z.ld_ixd((uint8_t)(2 + 2 * g), z.a);
        }
    }
    return z.go(0x15B4);
}

// ---- 15B4 / 15B7 / 15BA: the CALLs
template <class Mem> inline Result call_to(Z<Mem> &z, uint16_t target, uint16_t ret_addr) {
    z.t(17); z.wz = target;
    Result r; r.kind = kCall; r.next = target; r.ret_addr = ret_addr; return r;
}

// ---- 15BD
template <class Mem> inline Result tail_15bd(Z<Mem> &z) {
    z.a = z.b; z.t(4); z.and_a();
    if (z.a == 0) return z.ret(11);
    z.t(5);
    z.c = 0xC0; z.t(7);
    static const uint16_t addr[5] = {ram::sprite_data, (ram::sprite_data + 2), (ram::sprite_data + 4), (ram::sprite_data + 6), ram::fruit_sprite_code};
    for (uint16_t ad : addr) { z.ld_a_nn(ad); z.or_r(z.c); z.ld_nn_a(ad); }
    return z.ret(10);
}

// ---- 15E6: the fruit
template <class Mem> inline Result fruit_15e6(Z<Mem> &z) {
    z.ld_a_nn(ram::byte_4E06); z.sub_n(5);
    if (z.f & flag::C) return z.ret(11);
    z.t(5);
    z.ld_a_nn((ram::pacman_pixel + 1)); z.and_n(0x0F); z.cp_n(0x0C);
    if (z.jr(z.f & flag::C, 0x15F9)) {
        z.cp_n(8);
        if (z.jr(z.f & flag::C, 0x1601)) {
            z.cp_n(4);
            if (z.jr(z.f & flag::C, 0x1609)) { z.d = 0x14; z.t(7); }
            else { z.d = 0x10; z.t(7); z.jr(true, 0x160B); }
        } else { z.d = 0x14; z.t(7); z.jr(true, 0x160B); }
    } else { z.d = 0x18; z.t(7); z.jr(true, 0x160B); }
    z.ld_ixd(4, z.d); z.inc_d(); z.ld_ixd(6, z.d); z.inc_d(); z.ld_ixd(8, z.d); z.inc_d(); z.ld_ixd(0x0C, z.d);
    z.ld_ixd(0x0A, 0x3F);
    z.d = 0x16; z.t(7);
    z.ld_ixd(5, z.d); z.ld_ixd(7, z.d); z.ld_ixd(9, z.d); z.ld_ixd(0x0D, z.d);
    return z.ret(10);
}

// ---- 162D
template <class Mem> inline Result score_162d(Z<Mem> &z) {
    z.ld_a_nn(ram::byte_4E07); z.and_a();
    if (z.f & flag::Z) return z.ret(11);
    z.t(5);
    z.d = z.a; z.t(4);
    z.ld_a_nn(ram::pacman_tile_b); z.sub_n(0x3D);
    if (!z.jr(z.a != 0, 0x163E)) z.ld_ixd(0x0B, 0);
    z.a = z.d; z.t(4); z.cp_n(0x0A);
    if (z.f & flag::C) return z.ret(11);
    z.t(5);
    z.ld_ixd(2, 0x32); z.ld_ixd(3, 0x1D);
    z.cp_n(0x0C);
    if (z.f & flag::C) return z.ret(11);
    z.t(5);
    z.ld_ixd(2, 0x33);
    return z.ret(10);
}

// ---- 1652
template <class Mem> inline Result death_1652(Z<Mem> &z) {
    z.ld_a_nn(ram::byte_4E08); z.and_a();
    if (z.f & flag::Z) return z.ret(11);
    z.t(5);
    z.d = z.a; z.t(4);
    z.ld_a_nn(ram::pacman_tile_b); z.sub_n(0x3D);
    if (!z.jr(z.a != 0, 0x1663)) z.ld_ixd(0x0B, 0);
    z.a = z.d; z.t(4); z.cp_n(1);
    if (z.f & flag::C) return z.ret(11);
    z.t(5);
    z.ld_a_nn(ram::byte_4DC0); z.e = 8; z.t(7); z.add_a(z.e); z.ld_ixd(2, z.a);
    z.a = z.d; z.t(4); z.cp_n(3);
    if (z.f & flag::C) return z.ret(11);
    z.t(5);
    z.ld_a_nn((ram::blinky_pixel + 1)); z.and_n(8); z.rrca(); z.rrca(); z.rrca();
    z.e = 0x0A; z.t(7); z.add_a(z.e); z.ld_ixd(0x0C, z.a);
    z.inc_a(); z.inc_a(); z.ld_ixd(2, z.a);
    z.ld_ixd(0x0D, 0x1E);
    return z.ret(10);
}

// ---- 168C 16B1 16D6 16F7: Pac-Man's mouth
// plain: the picture by (position & 7): >= 6, >= 4, >= 2, else. t1..t3 = the JR C targets
template <class Mem> inline Result mouth_plain(Z<Mem> &z, uint16_t src, uint8_t v6, uint8_t v4, uint8_t v2, uint8_t v0, uint16_t t1, uint16_t t2, uint16_t t3) {
    z.ld_a_nn(src); z.and_n(7); z.cp_n(6);
    if (!z.jr(z.f & flag::C, t1)) { z.ld_ixd(0x0A, v6); return z.ret(10); }
    z.cp_n(4);
    if (!z.jr(z.f & flag::C, t2)) { z.ld_ixd(0x0A, v4); return z.ret(10); }
    z.cp_n(2);
    if (!z.jr(z.f & flag::C, t3)) { z.ld_ixd(0x0A, v2); return z.ret(10); }
    z.ld_ixd(0x0A, v0);
    return z.ret(10);
}
template <class Mem> inline Result mouth_168c(Z<Mem> &z) { return mouth_plain(z, (ram::pacman_pixel + 1), 0x30, 0x2E, 0x2C, 0x2E, 0x169A, 0x16A3, 0x16AC); }
template <class Mem> inline Result mouth_16b1(Z<Mem> &z) { return mouth_plain(z, ram::pacman_pixel, 0x2F, 0x2D, 0x2F, 0x30, 0x16BF, 0x16C8, 0x16D1); }
template <class Mem> inline Result mouth_16d6(Z<Mem> &z) {
    z.ld_a_nn((ram::pacman_pixel + 1)); z.and_n(7); z.cp_n(6);
    if (!z.jr(z.f & flag::C, 0x16E7)) { z.e = 0x2E; z.t(7); }
    else {
        z.cp_n(4);
        if (!z.jr(z.f & flag::C, 0x16EF)) { z.e = 0x2C; z.t(7); z.jr(true, 0x16E1); }
        else {
            z.cp_n(2);
            if (z.jr(!(z.f & flag::C), 0x16DF)) { z.e = 0x2E; z.t(7); }
            else { z.e = 0x30; z.t(7); z.jr(true, 0x16E1); }
        }
    }
    z.e |= 0x80; z.t(8, 2);
    z.ld_ixd(0x0A, z.e);
    return z.ret(10);
}
template <class Mem> inline Result mouth_16f7(Z<Mem> &z) {
    z.ld_a_nn(ram::pacman_pixel); z.and_n(7); z.cp_n(6);
    if (!z.jr(z.f & flag::C, 0x1705)) { z.ld_ixd(0x0A, 0x30); return z.ret(10); }
    z.cp_n(4);
    if (!z.jr(z.f & flag::C, 0x1711)) { z.e = 0x2F; z.t(7); }
    else {
        z.cp_n(2);
        if (!z.jr(z.f & flag::C, 0x1719)) { z.e = 0x2D; z.t(7); z.jr(true, 0x170B); }
        else { z.e = 0x2F; z.t(7); z.jr(true, 0x170B); }
    }
    z.e |= 0x40; z.t(8, 2);
    z.ld_ixd(0x0A, z.e);
    return z.ret(10);
}

} // namespace spp
} // namespace game
