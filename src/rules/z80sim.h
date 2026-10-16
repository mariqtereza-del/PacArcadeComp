// T02e: a small model of the Z80 instructions that the movement routines of the ROM run, used by the adapters in rules.h.
// The readable movement rules (src/logic/movement.h) say WHAT happens to the game state. The ROM routine also leaves behind: the time it takes, the
// registers and flags of its last instructions, and stale values on the stack (the vblank interrupt pushes the registers onto the stack and the replay
// frame hashes include the whole work RAM). The model below walks through the same instructions as the ROM routine (same branches, taken from the
// memory the routine would read) and only keeps track of those leftovers; it never decides what happens in the game.
// It writes to its own copy of the work RAM; the real RAM gets the result of the readable rules.
#pragma once
#include <cstdint>
#include <cstring>
#include "../board/machine.h"
#include "z80ops.h"
#include "../logic/ghost_targeting.h"
#include "../logic/movement.h"
#include "../logic/mode_timers.h"

namespace rules {

// The stack leftovers: the ROM's CALLs and PUSHes write below the stack pointer; the last write to each address stays. Writes are recorded in order.
struct Stack {
    uint16_t sp;
    struct W {
        uint16_t addr;
        uint8_t v;
    } w[400];
    int n = 0;
    explicit Stack(uint16_t at) : sp(at) {}
    void put(uint16_t a, uint8_t v) { w[n++] = W{a, v}; }
    void push16(uint16_t v) {
        sp = (uint16_t)(sp - 2);
        put(sp, (uint8_t)(v & 255));
        put((uint16_t)(sp + 1), (uint8_t)(v >> 8));
    }
    void pop16() { sp = (uint16_t)(sp + 2); }
    void apply(Machine &m) const {
        for (int i = 0; i < n; ++i) m.bus.write(w[i].addr, w[i].v);
    }
};

namespace sim {
using z80::Cost;
using z80::kC;
using z80::kH;
using z80::kN;
using z80::kPV;
using z80::kS;
using z80::kZ;
constexpr uint8_t kXYm = 0x28;

inline uint8_t szxy(uint8_t r) { return (uint8_t)((r & (kS | kXYm)) | (r == 0 ? kZ : 0)); }
inline uint8_t add8(uint8_t a, uint8_t b, unsigned cin, uint8_t &f) {
    const unsigned r = (unsigned)a + b + cin;
    f = (uint8_t)(szxy((uint8_t)r) | (((a ^ b ^ r) & 0x10) ? kH : 0) | (((a ^ r) & (b ^ r) & 0x80) ? kPV : 0) | (r > 0xFF ? kC : 0));
    return (uint8_t)r;
}
inline uint8_t sub8(uint8_t a, uint8_t b, unsigned cin, uint8_t &f) {
    const int r = (int)a - b - (int)cin;
    f = (uint8_t)(szxy((uint8_t)r) | kN | (((a ^ b ^ r) & 0x10) ? kH : 0) | (((a ^ b) & (a ^ r) & 0x80) ? kPV : 0) | (r < 0 ? kC : 0));
    return (uint8_t)r;
}
inline void cp8(uint8_t a, uint8_t b, uint8_t &f) {
    sub8(a, b, 0, f);
    f = (uint8_t)((f & ~kXYm) | (b & kXYm));
}
inline uint8_t and8(uint8_t a, uint8_t b, uint8_t &f) {
    const uint8_t r = a & b;
    f = (uint8_t)(szxy(r) | kH | (z80::even_parity(r) ? kPV : 0));
    return r;
}
inline uint8_t or8(uint8_t a, uint8_t b, uint8_t &f) {
    const uint8_t r = a | b;
    f = (uint8_t)(szxy(r) | (z80::even_parity(r) ? kPV : 0));
    return r;
}
inline uint8_t inc8(uint8_t v, uint8_t &f) {
    const uint8_t r = (uint8_t)(v + 1);
    f = (uint8_t)((f & kC) | szxy(r) | ((v & 0x0F) == 0x0F ? kH : 0) | (v == 0x7F ? kPV : 0));
    return r;
}
inline uint8_t dec8(uint8_t v, uint8_t &f) {
    const uint8_t r = (uint8_t)(v - 1);
    f = (uint8_t)((f & kC) | szxy(r) | kN | ((v & 0x0F) == 0 ? kH : 0) | (v == 0x80 ? kPV : 0));
    return r;
}
inline uint16_t add16(uint16_t x, uint16_t y, uint8_t &f) {
    const unsigned r = (unsigned)x + y;
    f = (uint8_t)((f & (kS | kZ | kPV)) | ((r >> 8) & kXYm) | (((x ^ y ^ r) & 0x1000) ? kH : 0) | (r > 0xFFFF ? kC : 0));
    return (uint16_t)r;
}
inline uint16_t adc16(uint16_t x, uint16_t y, uint8_t &f) {
    const unsigned cin = f & kC;
    const unsigned r = (unsigned)x + y + cin;
    const uint16_t r16 = (uint16_t)r;
    f = (uint8_t)(((r16 >> 8) & (kS | kXYm)) | (r16 == 0 ? kZ : 0) | (((x ^ y ^ r) & 0x1000) ? kH : 0) | (((x ^ r) & (y ^ r) & 0x8000) ? kPV : 0) | (r > 0xFFFF ? kC : 0));
    return r16;
}
inline uint16_t sbc16(uint16_t x, uint16_t y, uint8_t &f) {
    const unsigned cin = f & kC;
    const int r = (int)x - y - (int)cin;
    const uint16_t r16 = (uint16_t)r;
    f = (uint8_t)(((r16 >> 8) & (kS | kXYm)) | (r16 == 0 ? kZ : 0) | (((x ^ y ^ r) & 0x1000) ? kH : 0) | (((x ^ y) & (x ^ r) & 0x8000) ? kPV : 0) | kN | (r < 0 ? kC : 0));
    return r16;
}
inline uint8_t sla8(uint8_t v, uint8_t &f) {
    const uint8_t r = (uint8_t)(v << 1);
    f = (uint8_t)(szxy(r) | (z80::even_parity(r) ? kPV : 0) | (v >> 7));
    return r;
}
inline uint8_t rl8(uint8_t v, uint8_t &f) {
    const uint8_t r = (uint8_t)((v << 1) | (f & kC));
    f = (uint8_t)(szxy(r) | (z80::even_parity(r) ? kPV : 0) | (v >> 7));
    return r;
}
inline void bit8(unsigned n, uint8_t v, uint8_t &f) {
    const bool set = (v >> n) & 1;
    f = (uint8_t)((f & kC) | kH | (set ? 0 : (kZ | kPV)) | ((n == 7 && set) ? kS : 0) | (v & kXYm));
}

enum class Exit { kRet, kTo19CD, kTo2BEA, kFallback };

struct Sim {
    Machine &m;
    uint8_t work[logic::kWorkRamSize];
    uint8_t a, f, b, c, d, e, h, l;
    uint16_t ix, iy;
    Cost k;
    Stack st;
    bool stray = false; // the routine read or wrote memory that the model does not cover
    explicit Sim(Machine &mm) : m(mm), st((uint16_t)mm.on_get_sp()) {
        std::memcpy(work, m.bus.workram, sizeof work);
        const uint16_t af = (uint16_t)m.on_get_af(), bc = (uint16_t)m.on_get_bc(), de = (uint16_t)m.on_get_de(), hl_ = (uint16_t)m.on_get_hl();
        a = af >> 8, f = af & 255, b = bc >> 8, c = bc & 255, d = de >> 8, e = de & 255, h = hl_ >> 8, l = hl_ & 255;
        ix = (uint16_t)m.on_get_ix();
        iy = (uint16_t)m.on_get_iy();
    }
    void t(unsigned c_, unsigned fe = 1) { k.t(c_, fe); }
    uint16_t hl() const { return (uint16_t)(l | (h << 8)); }
    void set_hl(uint16_t v) { l = v & 255, h = v >> 8; }
    uint16_t bc() const { return (uint16_t)(c | (b << 8)); }
    uint16_t de() const { return (uint16_t)(e | (d << 8)); }
    uint16_t af() const { return (uint16_t)(f | (a << 8)); }

    uint8_t rd(unsigned addr) {
        addr &= 0xFFFF;
        if (addr >= 0x4C00 && addr < 0x4C00 + logic::kWorkRamSize) return work[addr - 0x4C00];
        if (addr < 0x4800 || addr == 0x5000 || addr == 0x5040) return m.bus.read((uint16_t)addr);
        stray = true;
        return 0;
    }
    void wr(unsigned addr, uint8_t v) {
        addr &= 0xFFFF;
        if (addr >= 0x4C00 && addr < 0x4C00 + logic::kWorkRamSize) work[addr - 0x4C00] = v;
        else stray = true;
    }
    uint8_t rd_nn_checked(unsigned addr) { return rd(addr); }

    // ---- instructions (with their time)
    void ld_a_nn(unsigned addr) { a = rd(addr); t(13); }
    void st_a_nn(unsigned addr) { wr(addr, a); t(13); }
    void ld_hl_nn(unsigned addr) { l = rd(addr); h = rd(addr + 1); t(16); }
    void st_hl_nn(unsigned addr) { wr(addr, l); wr(addr + 1, h); t(16); }
    void ld_a_hl() { a = rd(hl()); t(7); }
    void ld_a_ixd(unsigned d_) { a = rd(ix + d_); t(19, 2); }
    void ld_a_iyd(unsigned d_) { a = rd(iy + d_); t(19, 2); }
    void and_a() { a = and8(a, a, f); t(4); }
    void and_n(uint8_t n) { a = and8(a, n, f); t(7); }
    void cp_n(uint8_t n) { cp8(a, n, f); t(7); }
    void sub_n(uint8_t n) { a = sub8(a, n, 0, f); t(7); }
    void sub_b() { a = sub8(a, b, 0, f); t(4); }
    void rrca_() { a = z80::rrca(a, f); t(4); }
    void bit_a(unsigned n) { bit8(n, a, f); t(8, 2); }
    void add_hl_hl() { set_hl(add16(hl(), hl(), f)); t(11); }
    void adc_hl_hl() { set_hl(adc16(hl(), hl(), f)); t(15, 2); }
    void inc_mem_hl() { const uint8_t v = inc8(rd(hl()), f); wr(hl(), v); t(11); }
    void dec_mem_hl() { const uint8_t v = dec8(rd(hl()), f); wr(hl(), v); t(11); }
    void jr(bool taken) { t(taken ? 12 : 7); }
    void jp() { t(10); }
    void retcc(bool taken) {
        if (taken) {
            t(11);
            st.pop16();
        } else t(5);
    }
    void call(uint16_t ret) {
        st.push16(ret);
        t(17);
    }
    void ret() {
        t(10);
        st.pop16();
    }
    void final_ret() { t(10); } // the RET of the routine itself: the adapter takes the address from the real stack
    void final_retcc(bool taken) { t(taken ? 11 : 5); }

    // ---- ROM 2000: HL = (IY) + (IX), byte by byte; the routine ends with a RET (the CALL is the caller's)
    void s2000() {
        a = rd(iy); t(19, 2);
        a = add8(a, rd(ix), 0, f); t(19, 2);
        l = a; t(4);
        a = rd(iy + 1); t(19, 2);
        a = add8(a, rd(ix + 1), 0, f); t(19, 2);
        h = a; t(4);
        ret();
    }
    // ---- ROM 202D (reached by 0065's JP): the video RAM address of the tile in HL
    void s202D() {
        const uint16_t saved_af = af(), saved_bc = bc();
        st.push16(saved_af); t(11);
        st.push16(saved_bc); t(11);
        a = l; t(4);
        a = sub8(a, 0x20, 0, f); t(7);
        l = a; t(4);
        a = h; t(4);
        a = sub8(a, 0x20, 0, f); t(7);
        h = a; t(4);
        b = 0; t(7);
        h = sla8(h, f); t(8, 2);
        h = sla8(h, f); t(8, 2);
        h = sla8(h, f); t(8, 2);
        h = sla8(h, f); t(8, 2);
        b = rl8(b, f); t(8, 2);
        h = sla8(h, f); t(8, 2);
        b = rl8(b, f); t(8, 2);
        c = h; t(4);
        h = 0; t(7);
        set_hl(add16(hl(), bc(), f)); t(11);
        b = 0x40, c = 0x40; t(10);
        set_hl(add16(hl(), bc(), f)); t(11);
        // pop bc; pop af
        b = saved_bc >> 8, c = saved_bc & 255; st.pop16(); t(10);
        a = saved_af >> 8, f = saved_af & 255; st.pop16(); t(10);
        ret();
    }
    // ---- ROM 0065: JP 202D (the CALL is the caller's)
    void s0065() {
        t(10);
        s202D();
    }
    // ---- ROM 200F: call 2000, call 0065, ld a,(hl), and a, ret
    void s200F() {
        call(0x2012);
        s2000();
        call(0x2015);
        s0065();
        a = rd(hl()); t(7);
        and_a();
        ret();
    }
    // ---- ROM 2018: pixel position in HL to the tile
    void s2018() {
        a = l; t(4);
        for (int i = 0; i < 3; ++i) {
            const uint8_t cy = a & 1;
            a = (uint8_t)(a >> 1);
            f = (uint8_t)((a & kXYm) | cy | (a == 0 ? kZ : 0) | (z80::even_parity(a) ? kPV : 0));
            t(8, 2);
        }
        a = add8(a, 0x20, 0, f); t(7);
        l = a; t(4);
        a = h; t(4);
        for (int i = 0; i < 3; ++i) {
            const uint8_t cy = a & 1;
            a = (uint8_t)(a >> 1);
            f = (uint8_t)((a & kXYm) | cy | (a == 0 ? kZ : 0) | (z80::even_parity(a) ? kPV : 0));
            t(8, 2);
        }
        a = add8(a, 0x1E, 0, f); t(7);
        h = a; t(4);
        ret();
    }
    // ---- ROM 2052: video/colour address: HL + 0x400 of the tile address
    void s2052() {
        call(0x2055);
        s0065();
        d = 0x04, e = 0x00; t(10);
        set_hl(add16(hl(), de(), f)); t(11);
        ret();
    }
    // ---- ROM 205A: the tunnel flag of the tile in HL is written to (BC)
    void s205A() {
        call(0x205D);
        s2052();
        a = rd(hl()); t(7);
        cp_n(0x1B);
        const bool nz = !(f & kZ);
        jr(nz);
        if (!nz) {
            a = 1; t(7);
        } else {
            a = 0; f = z80::kFlagsAfterXorA; t(4);
        }
        wr(bc(), a); t(7);
        ret();
    }
    // ---- ROM 1ED0: A = index; sets the carry like the ROM
    void s1ED0() {
        a = add8(a, a, 0, f); t(4);
        c = a; t(4);
        b = 0; t(7);
        set_hl(0x4D09); t(10);
        set_hl(add16(hl(), bc(), f)); t(11);
        a = rd(hl()); t(7);
        cp_n(0x1D);
        jp();
        if (a == 0x1D) {
            wr(hl(), 0x3D); t(10);
            jp();
            f = (uint8_t)((f & (kS | kZ | kPV)) | (a & kXYm) | kC); t(4); // scf
            ret();
            return;
        }
        cp_n(0x3E);
        jp();
        if (a == 0x3E) {
            wr(hl(), 0x1E); t(10);
            jp();
            f = (uint8_t)((f & (kS | kZ | kPV)) | (a & kXYm) | kC); t(4);
            ret();
            return;
        }
        b = 0x21; t(7);
        a = sub8(a, b, 0, f); t(4);
        jp();
        if (f & kC) {
            f = (uint8_t)((f & (kS | kZ | kPV)) | (a & kXYm) | kC); t(4);
            ret();
            return;
        }
        a = rd(hl()); t(7);
        b = 0x3B; t(7);
        a = sub8(a, b, 0, f); t(4);
        jp();
        if (!(f & kC)) {
            f = (uint8_t)((f & (kS | kZ | kPV)) | (a & kXYm) | kC); t(4);
            ret();
            return;
        }
        and_a();
        ret();
    }
    // ---- rst 28h with its two data bytes (queue a task): `after_rst` = address of the first data byte
    void rst28(uint16_t after_rst, uint8_t task_b, uint8_t task_c) {
        st.push16(after_rst); t(11);
        st.pop16(); t(10);
        b = task_b; t(7);
        t(6);
        c = task_c; t(7);
        t(6);
        set_hl((uint16_t)(after_rst + 2));
        st.push16((uint16_t)(after_rst + 2)); t(11);
        t(12); // jr 0042
        ld_hl_nn(0x4C80);
        wr(hl(), b); t(7);
        l = inc8(l, f); t(4);
        wr(hl(), c); t(7);
        l = inc8(l, f); t(4);
        const bool nz = !(f & kZ);
        jr(nz);
        if (!nz) {
            l = 0xC0; t(7);
        }
        st_hl_nn(0x4C80);
        ret();
    }
    // ---- rst 18h: HL = word at HL + 2*B (a table of words)
    void rst18(uint16_t after_rst) {
        st.push16(after_rst); t(11);
        a = b; t(4);
        a = add8(a, a, 0, f); t(4);
        st.push16(0x001B); t(11); // rst 10h
        a = add8(a, l, 0, f); t(4);
        l = a; t(4);
        a = 0; t(7);
        a = add8(a, h, f & kC, f); t(4);
        h = a; t(4);
        a = rd(hl()); t(7);
        st.pop16(); t(10);
        e = a; t(4);
        set_hl((uint16_t)(hl() + 1)); t(6);
        d = rd(hl()); t(7);
        { const uint16_t x = de(), y = hl(); set_hl(x); d = y >> 8, e = y & 255; t(4); } // ex de,hl
        st.pop16(); t(10);
    }
    // ---- ROM 20D7: Cruise Elroy
    void s20D7() {
        ld_a_nn(0x4DA3);
        and_a();
        if (a == 0) { retcc(true); return; }
        retcc(false);
        set_hl(0x4E0E); t(10);
        ld_a_nn(0x4DB6);
        and_a();
        jp();
        if (a == 0) {
            a = 0xF4; t(7);
            a = sub8(a, rd(hl()), 0, f); t(7);
            b = a; t(4);
            ld_a_nn(0x4DBB);
            sub_b();
            const bool cy = f & kC;
            retcc(cy);
            if (cy) return;
            a = 1; t(7);
            st_a_nn(0x4DB6);
        }
        ld_a_nn(0x4DB7);
        and_a();
        const bool nz = a != 0;
        retcc(nz);
        if (nz) return;
        a = 0xF4; t(7);
        a = sub8(a, rd(hl()), 0, f); t(7);
        b = a; t(4);
        ld_a_nn(0x4DBC);
        sub_b();
        const bool cy = f & kC;
        retcc(cy);
        if (cy) return;
        a = 1; t(7);
        st_a_nn(0x4DB7);
        ret();
    }

    // ---- the speed pattern (ROM 1818-1842 and the same lines for each pattern): `lo` = the low word, `hi` = the high word.
    // Returns true if the routine returns here (no bit fell out). `jump_after`: the pattern ends with a JP (patterns that do not come last).
    bool speed_pattern(unsigned hi, bool jump_after) {
        const unsigned lo = hi + 2;
        ld_hl_nn(lo);
        add_hl_hl();
        st_hl_nn(lo);
        ld_hl_nn(hi);
        adc_hl_hl();
        st_hl_nn(hi);
        const bool no_carry = !(f & kC);
        if (no_carry) {
            final_retcc(true);
            return true;
        }
        final_retcc(false);
        set_hl((uint16_t)lo); t(10);
        inc_mem_hl();
        if (jump_after) jp();
        return false;
    }

    // ---- ROM 1806: Pac-Man's movement. Ends with RET (kRet), or carries on at 19CD (kTo19CD: the dot / energizer part), or kFallback (the fruit is eaten
    // here: the ROM runs the routine itself).
    Exit pacman() {
        set_hl(0x4D9D); t(10);
        a = 0xFF; t(7);
        cp8(a, rd(hl()), f); t(7);
        jp();
        if (!(f & kZ)) {
            dec_mem_hl();
            final_ret();
            return Exit::kRet;
        }
        ld_a_nn(0x4DA6);
        and_a();
        jp();
        if (a != 0) {
            if (speed_pattern(0x4D4A, true)) return Exit::kRet;
        } else {
            if (speed_pattern(0x4D46, false)) return Exit::kRet;
        }
        // 1843
        ld_a_nn(0x4E0E);
        st_a_nn(0x4D9E);
        ld_a_nn(0x4E72);
        c = a; t(4);
        ld_a_nn(0x4E09);
        a = and8(a, c, f); t(4);
        c = a; t(4);
        set_hl(0x4D3A); t(10);
        ld_a_hl();
        b = 0x21; t(7);
        a = sub8(a, b, 0, f); t(4);
        bool tunnel_rows;
        if (f & kC) {
            jr(true);
            tunnel_rows = true;
        } else {
            jr(false);
            ld_a_hl();
            b = 0x3B; t(7);
            a = sub8(a, b, 0, f); t(4);
            if (!(f & kC)) {
                jr(true);
                tunnel_rows = true;
            } else {
                jr(false);
                jp(); // jp 18AB
                tunnel_rows = false;
            }
        }
        bool demo = false;
        if (tunnel_rows) {
            a = 1; t(7);
            st_a_nn(0x4DBF);
        }
        // 1869 / 18AB: the demo and level-change walk
        ld_a_nn(0x4E00);
        cp_n(1);
        jp();
        if (a == 1) demo = true;
        else {
            ld_a_nn(0x4E04);
            cp_n(0x10);
            jp();
            if (!(f & kC)) demo = true;
        }
        if (demo) return pacman_demo_walk();
        // the joystick
        a = c; t(4);
        and_a();
        const bool first_stick = (a == 0);
        jr(first_stick);
        if (!first_stick) {
            ld_a_nn(0x5040);
            jp();
        } else {
            ld_a_nn(0x5000);
        }
        if (tunnel_rows) {
            bit_a(1);
            jp();
            if (!(f & kZ)) {
                bit_a(2);
                jp();
                if (!(f & kZ)) {
                    // jp 1950 with no direction
                } else {
                    ld_hl_nn(0x32FF);
                    a = 0; f = z80::kFlagsAfterXorA; t(4);
                    st_a_nn(0x4D30);
                    st_hl_nn(0x4D1C);
                    jp();
                }
            } else {
                ld_hl_nn(0x3303);
                a = 2; t(7);
                st_a_nn(0x4D30);
                st_hl_nn(0x4D1C);
                jp();
            }
            return pacman_move_and_centre();
        }
        // 18C8: maze rows
        bit_a(1);
        jp();
        bool pressed = true;
        if (!(f & kZ)) {
            bit_a(2);
            jp();
            if (!(f & kZ)) {
                bit_a(0);
                jp();
                if (!(f & kZ)) {
                    bit_a(3);
                    jp();
                    if (!(f & kZ)) pressed = false;
                    else { // 1AF8: down
                        ld_hl_nn(0x3301);
                        a = 1; t(7);
                        st_a_nn(0x4D3C);
                        st_hl_nn(0x4D26);
                        b = 0; t(7);
                        jp();
                    }
                } else { // 1AE8: up
                    ld_hl_nn(0x3305);
                    a = 3; t(7);
                    st_a_nn(0x4D3C);
                    st_hl_nn(0x4D26);
                    b = 0; t(7);
                    jp();
                }
            } else { // 1AD9: right
                ld_hl_nn(0x32FF);
                a = 0; f = z80::kFlagsAfterXorA; t(4);
                st_a_nn(0x4D3C);
                st_hl_nn(0x4D26);
                b = 0; t(7);
                jp();
            }
        } else { // 1AC9: left
            ld_hl_nn(0x3303);
            a = 2; t(7);
            st_a_nn(0x4D3C);
            st_hl_nn(0x4D26);
            b = 0; t(7);
            jp();
        }
        if (!pressed) {
            ld_hl_nn(0x4D1C);
            st_hl_nn(0x4D26);
            b = 1; t(7);
        }
        // 18E4
        ix = 0x4D26; t(14, 2);
        iy = 0x4D39; t(14, 2);
        call(0x18EF);
        s200F();
        a = and8(a, 0xC0, f); t(7);
        a = sub8(a, 0xC0, 0, f); t(7);
        bool nz = !(f & kZ);
        jr(nz);
        bool go_1950 = false;
        if (nz) {
            go_1950 = take_wanted();
        } else {
            b = dec8(b, f); t(4);
            jp();
            if (b == 0) {
                if (blocked_stop(0x1900)) return Exit::kRet;
                go_1950 = take_wanted();
            } else {
                ix = 0x4D1C; t(14, 2);
                call(0x191D);
                s200F();
                a = and8(a, 0xC0, f); t(7);
                a = sub8(a, 0xC0, 0, f); t(7);
                nz = !(f & kZ);
                jr(nz);
                if (!nz) {
                    if (blocked_stop(0x192A)) return Exit::kRet;
                }
            }
        }
        (void)go_1950;
        return pacman_move_and_centre();
    }
    // 1940: take the wanted step (and direction unless b ran out)
    bool take_wanted() {
        ld_hl_nn(0x4D26);
        st_hl_nn(0x4D1C);
        b = dec8(b, f); t(4);
        jp();
        if (b != 0) {
            ld_a_nn(0x4D3C);
            st_a_nn(0x4D30);
        }
        return true;
    }
    // the "stop in the middle of the tile" lines at 18F9 / 1923: returns true when the routine returned
    bool blocked_stop(unsigned) {
        ld_a_nn(0x4D30);
        rrca_();
        const bool odd = f & kC;
        jp();
        if (odd) ld_a_nn(0x4D08);
        else ld_a_nn(0x4D09);
        a = and8(a, 7, f); t(7);
        cp_n(4);
        const bool stop = f & kZ;
        if (stop) {
            final_retcc(true);
            return true;
        }
        final_retcc(false);
        jp();
        return false;
    }
    // the demo walk 1A19, ends at 1985
    Exit pacman_demo_walk() {
        set_hl(0x4D1C); t(10);
        ld_a_hl();
        and_a();
        jp();
        bool middle;
        if (a != 0) {
            ld_a_nn(0x4D08);
            a = and8(a, 7, f); t(7);
            cp_n(4);
            jp();
            middle = f & kZ;
            if (!middle) jp(); // jp 1A5C
        } else {
            ld_a_nn(0x4D09);
            a = and8(a, 7, f); t(7);
            cp_n(4);
            jp();
            middle = !(f & kZ) ? false : true;
        }
        if (middle) {
            a = 5; t(7);
            call(0x1A3D);
            s1ED0();
            const bool cy = f & kC;
            jr(cy);
            if (!cy) rst28(0x1A40, 0x17, 0x00);
            ix = 0x4D26; t(14, 2);
            iy = 0x4D12; t(14, 2);
            call(0x1A4D);
            s2000();
            st_hl_nn(0x4D12);
            ld_hl_nn(0x4D26);
            st_hl_nn(0x4D1C);
            ld_a_nn(0x4D3C);
            st_a_nn(0x4D30);
        }
        ix = 0x4D1C; t(14, 2);
        iy = 0x4D08; t(14, 2);
        call(0x1A67);
        s2000();
        jp(); // jp 1985
        return pacman_tile_and_eating();
    }
    // 1950: the move, then the pull to the middle of the tile
    Exit pacman_move_and_centre() {
        ix = 0x4D1C; t(14, 2);
        iy = 0x4D08; t(14, 2);
        call(0x195B);
        s2000();
        ld_a_nn(0x4D30);
        rrca_();
        const bool odd = f & kC;
        jp();
        if (!odd) {
            a = l; t(4);
            a = and8(a, 7, f); t(7);
            cp_n(4);
            jp();
            if (!(f & kZ)) {
                jp();
                if (f & kC) { l = inc8(l, f); t(4); jp(); }
                else { l = dec8(l, f); t(4); jp(); }
            }
        } else {
            a = h; t(4);
            a = and8(a, 7, f); t(7);
            cp_n(4);
            jp();
            if (!(f & kZ)) {
                jp();
                if (f & kC) { h = inc8(h, f); t(4); }
                else { h = dec8(h, f); t(4); jp(); }
            }
        }
        return pacman_tile_and_eating();
    }
    // 1985: the new tile, then the fruit and the dots
    Exit pacman_tile_and_eating() {
        st_hl_nn(0x4D08);
        call(0x198B);
        s2018();
        st_hl_nn(0x4D39);
        ix = 0x4DBF; t(14, 2);
        ld_a_ixd(0);
        wr(ix, 0); t(19, 2);
        and_a();
        const bool nz = a != 0;
        if (nz) {
            final_retcc(true);
            return Exit::kRet;
        }
        final_retcc(false);
        ld_a_nn(0x4DD2);
        and_a();
        bool z = a == 0;
        jr(z);
        if (!z) {
            ld_a_nn(0x4DD4);
            and_a();
            z = a == 0;
            jr(z);
            if (!z) {
                ld_hl_nn(0x4D08);
                d = 0x80, e = 0x94; t(10);
                and_a();
                set_hl(sbc16(hl(), de(), f)); t(15, 2);
                const bool ne = !(f & kZ);
                jr(ne);
                if (!ne) return Exit::kFallback; // the fruit is eaten: the ROM runs this part
            }
        }
        return Exit::kTo19CD;
    }

    // ---- the movement of ghost g (ROM 1B36, 1C4B, 1D22, 1DF9)
    Exit ghost(unsigned g) {
        using namespace logic;
        const unsigned shift = 0xD7 * g;
        static const uint16_t kAfterTunnelCall[4] = {0x1B4C, 0x1C5F, 0x1D36, 0x1E0D};
        if (g == 0) {
            ld_a_nn(0x4DA0);
            and_a();
            if (a == 0) { final_retcc(true); return Exit::kRet; }
            final_retcc(false);
        } else {
            ld_a_nn(kGhostActiveAddress[g]);
            cp_n(1);
            if (a != 1) { final_retcc(true); return Exit::kRet; }
            final_retcc(false);
        }
        ld_a_nn(kGhostBlockedAddress[g]);
        and_a();
        if (a != 0) { final_retcc(true); return Exit::kRet; }
        final_retcc(false);
        if (g == 0) {
            call(0x1B43);
            s20D7();
        }
        ld_hl_nn(kGhostMoveTileAddress[g]);
        b = kGhostTunnelFlagAddress[g] >> 8, c = kGhostTunnelFlagAddress[g] & 255; t(10);
        call(kAfterTunnelCall[g]);
        s205A();
        ld_a_nn(kGhostTunnelFlagAddress[g]);
        and_a();
        jp();
        bool done = false;
        if (a != 0) {
            if (speed_pattern(kSpeedTunnelAddress[g], true)) return Exit::kRet;
            done = true;
        }
        if (!done) {
            ld_a_nn(kGhostEdibleAddress[g]);
            and_a();
            jp();
            if (a != 0) {
                if (speed_pattern(kSpeedBlueAddress[g], true)) return Exit::kRet;
                done = true;
            }
        }
        if (!done && g == 0) {
            ld_a_nn(0x4DB7);
            and_a();
            jp();
            if (a != 0) {
                if (speed_pattern(kSpeedElroy2Address, true)) return Exit::kRet;
                done = true;
            }
            if (!done) {
                ld_a_nn(0x4DB6);
                and_a();
                jp();
                if (a != 0) {
                    if (speed_pattern(kSpeedElroy1Address, true)) return Exit::kRet;
                    done = true;
                }
            }
        }
        if (!done) {
            if (speed_pattern(kSpeedNormalAddress[g], false)) return Exit::kRet;
        }
        // the tail (Blinky's addresses + shift)
        const unsigned step = kGhostStepAddress[g], pixel = kGhostPixelAddress[g];
        set_hl((uint16_t)step); t(10);
        ld_a_hl();
        and_a();
        jp();
        bool middle;
        if (a != 0) {
            ld_a_nn(pixel);
            a = and8(a, 7, f); t(7);
            cp_n(4);
            jp();
            middle = f & kZ;
            if (!middle) jp(); // jp 1C36
        } else {
            ld_a_nn(pixel + 1);
            a = and8(a, 7, f); t(7);
            cp_n(4);
            jp();
            middle = f & kZ;
        }
        if (middle) {
            a = (uint8_t)(g + 1); t(7);
            call((uint16_t)(0x1BFC + shift));
            s1ED0();
            const bool cy = f & kC;
            jr(cy);
            if (!cy) {
                ld_a_nn(kGhostEdibleAddress[g]);
                and_a();
                jp();
                if (a != 0) {
                    rst28((uint16_t)(0x1C06 + shift), (uint8_t)(kTaskGhostBlueTurn + g), 0);
                    jp();
                } else {
                    ld_hl_nn(kGhostTileAddress[g]);
                    call((uint16_t)(0x1C11 + shift));
                    s2052();
                    ld_a_hl();
                    cp_n(0x1A);
                    const bool door = f & kZ;
                    jr(door);
                    if (!door) rst28((uint16_t)(0x1C17 + shift), (uint8_t)(kTaskGhostTurn + g), 0);
                }
            }
            call((uint16_t)(0x1C1C + shift));
            turn_round(g);
            ix = (uint16_t)kGhostVectorAddress[g]; t(14, 2);
            iy = (uint16_t)kGhostTileAddress[g]; t(14, 2);
            call((uint16_t)(0x1C27 + shift));
            s2000();
            st_hl_nn(kGhostTileAddress[g]);
            ld_hl_nn(kGhostVectorAddress[g]);
            st_hl_nn(step);
            ld_a_nn(kGhostDirectionAddress[g]);
            st_a_nn(kGhostCurrentDirectionAddress[g]);
        }
        ix = (uint16_t)step; t(14, 2);
        iy = (uint16_t)pixel; t(14, 2);
        call((uint16_t)(0x1C41 + shift));
        s2000();
        st_hl_nn(pixel);
        call((uint16_t)(0x1C47 + shift));
        s2018();
        st_hl_nn(kGhostMoveTileAddress[g]);
        final_ret();
        return Exit::kRet;
    }
    // ---- ROM 1EFE / 1F25 / 1F4C / 1F73
    void turn_round(unsigned g) {
        using namespace logic;
        ld_a_nn(kGhostReverseAddress[g]);
        and_a();
        if (a == 0) { retcc(true); return; }
        retcc(false);
        a = 0; f = z80::kFlagsAfterXorA; t(4);
        st_a_nn(kGhostReverseAddress[g]);
        set_hl(0x32FF); t(10);
        ld_a_nn(kGhostCurrentDirectionAddress[g]);
        a = (uint8_t)(a ^ 2);
        f = (uint8_t)(szxy(a) | (z80::even_parity(a) ? kPV : 0)); t(7); // xor 2
        st_a_nn(kGhostDirectionAddress[g]);
        b = a; t(4);
        rst18((uint16_t)(0x1F14 + 0x27 * g));
        st_hl_nn(kGhostVectorAddress[g]);
        ld_a_nn(0x4E02);
        cp_n(0x22);
        const bool nz = !(f & kZ);
        retcc(nz);
        if (nz) return;
        st_hl_nn(kGhostStepAddress[g]);
        a = b; t(4);
        st_a_nn(kGhostCurrentDirectionAddress[g]);
        ret();
    }

    // ---- ROM 0E36: the scatter / chase timer (every frame). Always ends with RET.
    void timer_0E36() {
        using namespace logic;
        ld_a_nn(0x4DA6);
        and_a();
        retcc_final(a != 0);
        if (a != 0) return;
        ld_a_nn(0x4DC1);
        cp_n(7);
        retcc_final(a == 7);
        if (a == 7) return;
        a = add8(a, a, 0, f); t(4);
        ld_hl_nn(0x4DC2);
        set_hl((uint16_t)(hl() + 1)); t(6);
        st_hl_nn(0x4DC2);
        e = a; t(4);
        d = 0; t(7);
        ix = 0x4D86; t(14, 2);
        ix = (uint16_t)add16(ix, de(), f); t(15, 2);
        e = rd(ix); t(19, 2);
        d = rd(ix + 1); t(19, 2);
        and_a_flags();
        set_hl(sbc16(hl(), de(), f)); t(15, 2);
        const bool nz = !(f & kZ);
        retcc_final(nz);
        if (nz) return;
        { const uint8_t cy = a & 1; a = (uint8_t)(a >> 1); f = (uint8_t)((a & kXYm) | cy | (a == 0 ? kZ : 0) | (z80::even_parity(a) ? kPV : 0)); t(8, 2); }
        a = inc8(a, f); t(4);
        st_a_nn(0x4DC1);
        set_hl(0x0101); t(10);
        st_hl_nn(0x4DB1);
        st_hl_nn(0x4DB3);
        final_ret();
    }
    void and_a_flags() { a = and8(a, a, f); t(4); }
    void retcc_final(bool taken) { final_retcc(taken); }

    // ---- ROM 1376: the frightened timer (every frame). Always ends with RET.
    void frightened_1376() {
        using namespace logic;
        ld_a_nn(0x4DA6);
        and_a();
        retcc_final(a == 0);
        if (a == 0) return;
        ix = 0x4DA7; t(14, 2);
        a = rd(ix); t(19, 2);
        a = or8(a, rd(ix + 1), f); t(19, 2);
        a = or8(a, rd(ix + 2), f); t(19, 2);
        a = or8(a, rd(ix + 3), f); t(19, 2);
        jp();
        if (a != 0) {
            ld_hl_nn(0x4DCB);
            set_hl((uint16_t)(hl() - 1)); t(6);
            st_hl_nn(0x4DCB);
            a = h; t(4);
            a = or8(a, l, f); t(4);
            retcc_final(a != 0);
            if (a != 0) return;
        }
        set_hl(0x4C0B); t(10);
        wr(hl(), 9); t(10);
        static const unsigned kBlocked[4] = {0x4DAC, 0x4DAD, 0x4DAE, 0x4DAF}, kEdible[4] = {0x4DA7, 0x4DA8, 0x4DA9, 0x4DAA};
        for (int g = 0; g < 4; ++g) {
            ld_a_nn(kBlocked[g]);
            and_a();
            jp();
            if (a == 0) st_a_nn(kEdible[g]);
        }
        a = 0; f = z80::kFlagsAfterXorA; t(4);
        st_a_nn(0x4DCB);
        st_a_nn(0x4DCC);
        st_a_nn(0x4DA6);
        st_a_nn(0x4DC8);
        st_a_nn(0x4DD0);
        set_hl(0x4EAC); t(10);
        wr(hl(), (uint8_t)(rd(hl()) & ~0x20)); t(15, 2);
        wr(hl(), (uint8_t)(rd(hl()) & ~0x80)); t(15, 2);
        final_ret();
    }

    // ---- T02h: the ghost house routines. Each ends with RET (the CALL is the caller's).
    void dec_a() { a = dec8(a, f); t(4); }
    void inc_a() { a = inc8(a, f); t(4); }
    // ROM 1066: the eaten ghost waiting to become "dead"
    void eaten_ghost_1066() {
        ld_a_nn(0x4DAB);
        and_a();
        retcc_final(a == 0);
        if (a == 0) return;
        static const unsigned kDead[3] = {0x4DAC, 0x4DAD, 0x4DAE};
        for (int i = 0; i < 3; ++i) {
            dec_a();
            jr(a != 0);
            if (a == 0) {
                st_a_nn(0x4DAB);
                inc_a();
                st_a_nn(kDead[i]);
                final_ret();
                return;
            }
        }
        st_a_nn(0x4DAF);
        dec_a();
        st_a_nn(0x4DAB);
        final_ret();
    }
    // ROM 2069 (who 0), 208C (who 1), 20AF (who 2): may the ghost leave the house now?
    void house_release(unsigned who) {
        static const unsigned kOut[3] = {0x4DA1, 0x4DA2, 0x4DA3}, kOwn[3] = {0x4E0F, 0x4E10, 0x4E11}, kLimit[3] = {0x4DB8, 0x4DB9, 0x4DBA};
        static const uint8_t kCommon[3] = {7, 0x11, 0x20}, kValue[3] = {2, 3, 3};
        ld_a_nn(kOut[who]);
        and_a();
        retcc_final(a != 0);
        if (a != 0) return;
        ld_a_nn(0x4E12);
        and_a();
        jp();
        bool let_out = false;
        if (a != 0) {
            ld_a_nn(0x4D9F);
            cp_n(kCommon[who]);
            retcc_final(a != kCommon[who]);
            if (a != kCommon[who]) return;
            if (who == 2) {
                a = 0; f = z80::kFlagsAfterXorA; t(4);
                st_a_nn(0x4E12);
                st_a_nn(0x4D9F);
                final_ret();
                return;
            }
            jp();
            let_out = true;
        } else {
            set_hl((uint16_t)kLimit[who]); t(10);
            ld_a_nn(kOwn[who]);
            cp8(a, rd(hl()), f); t(7);
            retcc_final(a < rd(hl()));
            if (a < rd(hl())) return;
            let_out = true;
        }
        if (let_out) {
            a = kValue[who]; t(7);
            st_a_nn(kOut[who]);
            final_ret();
        }
    }
    // ROM 13DD: the inactivity timer
    void inactivity_13DD() {
        set_hl(0x4D9E); t(10);
        ld_a_nn(0x4E0E);
        cp8(a, rd(hl()), f); t(7);
        jp();
        if (a != rd(hl())) {
            set_hl(0); t(10);
            st_hl_nn(0x4D97);
            final_ret();
            return;
        }
        ld_hl_nn(0x4D97);
        set_hl((uint16_t)(hl() + 1)); t(6);
        st_hl_nn(0x4D97);
        e = rd(0x4D95); d = rd(0x4D96); t(20, 2);
        and_a();
        set_hl(sbc16(hl(), de(), f)); t(15, 2);
        retcc_final(!(f & kZ));
        if (f & kZ) {} else return;
        set_hl(0); t(10);
        st_hl_nn(0x4D97);
        static const unsigned kOut[3] = {0x4DA1, 0x4DA2, 0x4DA3};
        static const uint8_t kValue[3] = {2, 3, 3};
        static const uint16_t kAfter[3] = {0x140B, 0x1415, 0x141E};
        for (int g = 0; g < 3; ++g) {
            ld_a_nn(kOut[g]);
            and_a();
            const uint16_t saved = af();
            if (g < 2) { st.push16(saved); t(11); }
            const bool zero = (f & kZ) != 0;
            if (zero) {
                call(kAfter[g]);
                a = kValue[g]; t(7);
                st_a_nn(kOut[g]);
                ret();
            } else t(10);
            if (g < 2) {
                a = saved >> 8, f = saved & 255; st.pop16(); t(10);
                retcc_final(zero);
                if (zero) return;
            } else {
                final_ret();
                return;
            }
        }
    }

    // ---- ROM 070E up to the instruction before its CALL 2BEA (the fruit drawing stays ROM code): returns the level row used, 255 when the ROM
    // would read beyond its level table.
    unsigned level_070E() {
        using namespace logic;
        a = b; t(4);
        and_a();
        const bool given = a != 0;
        jr(given);
        if (!given) {
            ld_hl_nn(0x4E0A);
            a = m.bus.read(hl()); t(7); // the level number the demo points at (a ROM address)
        }
        ix = 0x0796; t(14, 2);
        b = a; t(4);
        const unsigned level = a;
        a = add8(a, a, 0, f); t(4);
        a = add8(a, a, 0, f); t(4);
        a = add8(a, b, 0, f); t(4);
        a = add8(a, b, 0, f); t(4);
        e = a; t(4);
        d = 0; t(7);
        ix = add16(ix, de(), f); t(15, 2);
        if (level >= kLevelCount) return 255;
        a = rd(ix); t(19, 2);
        a = add8(a, a, 0, f); t(4);
        b = a; t(4);
        a = add8(a, a, 0, f); t(4);
        a = add8(a, a, 0, f); t(4);
        c = a; t(4);
        a = add8(a, a, 0, f); t(4);
        a = add8(a, a, 0, f); t(4);
        a = add8(a, c, 0, f); t(4);
        a = add8(a, b, 0, f); t(4);
        e = a; t(4);
        d = 0; t(7);
        set_hl(0x330F); t(10);
        set_hl(add16(hl(), de(), f)); t(11);
        call(0x073A);
        // 0814
        de_imm(0x4D46);
        bc_imm(0x001C);
        ldir();
        const unsigned counts[3] = {12, 12, 12};
        for (int i = 0; i < 3; ++i) {
            bc_imm((uint16_t)counts[i]);
            a = and8(a, a, f); t(4);
            set_hl(sbc16(hl(), bc(), f)); t(15, 2);
            ldir();
        }
        bc_imm(0x000E);
        ldir();
        ret();
        a = rd(ix + 1); t(19, 2);
        st_a_nn(0x4DB0);
        a = rd(ix + 2); t(19, 2);
        b = a; t(4);
        a = add8(a, a, 0, f); t(4);
        a = add8(a, b, 0, f); t(4);
        e = a; t(4);
        d = 0; t(7);
        set_hl(0x0843); t(10);
        set_hl(add16(hl(), de(), f)); t(11);
        call(0x0750);
        de_imm(0x4DB8);
        bc_imm(0x0003);
        ldir();
        ret();
        static const uint16_t kTab[3] = {0x084F, 0x0861, 0x0873};
        static const unsigned kDest[3] = {0x4DBB, 0x4DBD, 0x4D95};
        for (int i = 0; i < 3; ++i) {
            a = rd(ix + 3 + i); t(19, 2);
            a = add8(a, a, 0, f); t(4);
            e = a; t(4);
            d = 0; t(7);
            iy = kTab[i]; t(14, 2);
            iy = add16(iy, de(), f); t(15, 2);
            l = rd(iy); t(19, 2);
            h = rd(iy + 1); t(19, 2);
            st_hl_nn(kDest[i]);
        }
        return level;
    }
    void de_imm(uint16_t v) { d = v >> 8, e = v & 255; t(10); }
    void bc_imm(uint16_t v) { b = v >> 8, c = v & 255; t(10); }
    void ldir() {
        const unsigned n = bc();
        t(21 * (n - 1) + 16, 2 * n);
        set_hl((uint16_t)(hl() + n));
        d = (uint8_t)((de() + n) >> 8), e = (uint8_t)((de() + n) & 255);
        b = 0, c = 0;
    }
};

} // namespace sim
} // namespace rules
