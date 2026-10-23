// N49a: the register file of the machine without the Z80 core: plain variables with the same function names the native code calls on the
// machine (the list that tests/test_no_z80_header.sh prints). There is no instruction decoder here: the native routine table (src/rules) does
// every step; the machine (machine.h) has the one idle step of a halted CPU and stops the program if an address without a native routine is ever reached.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>

class CpuRegs {
public:
    // power-on state of the Z80 core of this project (checked against the core by tests/test_native_machine.sh)
    unsigned on_get_af() const { return af; }
    void on_set_af(unsigned v) { af = (uint16_t)v; }
    unsigned on_get_a() const { return af >> 8; }
    void on_set_a(unsigned v) { af = (uint16_t)((af & 0x00FF) | ((v & 0xFF) << 8)); }
    unsigned on_get_f() const { return af & 0xFF; }
    void on_set_f(unsigned v) { af = (uint16_t)((af & 0xFF00) | (v & 0xFF)); }
    unsigned on_get_bc() const { return bc; }
    void on_set_bc(unsigned v) { bc = (uint16_t)v; }
    unsigned on_get_de() const { return de; }
    void on_set_de(unsigned v) { de = (uint16_t)v; }
    unsigned on_get_hl() const { return hl; }
    void on_set_hl(unsigned v) { hl = (uint16_t)v; }
    unsigned on_get_ix() const { return ix; }
    void on_set_ix(unsigned v) { ix = (uint16_t)v; }
    unsigned on_get_iy() const { return iy; }
    void on_set_iy(unsigned v) { iy = (uint16_t)v; }
    unsigned on_get_sp() const { return sp; }
    void on_set_sp(unsigned v) { sp = (uint16_t)v; }
    unsigned on_get_pc() const { return pc; }
    void on_set_pc(unsigned v) { pc = (uint16_t)v; }
    unsigned on_get_wz() const { return wz; }
    void on_set_wz(unsigned v) { wz = (uint16_t)v; }
    unsigned on_get_i() const { return i; }
    void on_set_i(unsigned v) { i = (uint8_t)v; }
    unsigned on_get_r() const { return r; }
    void on_set_r(unsigned v) { r = (uint8_t)v; }
    unsigned on_get_ir() const { return (unsigned)(i << 8 | r); }
    bool on_get_iff1() const { return iff1; }
    void on_set_iff1(bool v) { iff1 = v; }
    bool on_get_iff2() const { return iff2; }
    void on_set_iff2(bool v) { iff2 = v; }
    unsigned on_get_int_mode() const { return im; }
    void on_set_int_mode(unsigned v) { im = (uint8_t)v; }
    bool on_is_halted() const { return halted; }
    void on_set_is_halted(bool v) { halted = v; }
    bool on_is_int_disabled() const { return int_disabled; }   // true directly after EI (and after an index-register prefix): no interrupt is taken
    void on_set_is_int_disabled(bool v) { int_disabled = v; }
    unsigned get_alt_af() const { return alt_af; }
    void set_alt_af(unsigned v) { alt_af = (uint16_t)v; }
    unsigned get_alt_bc() const { return alt_bc; }
    void set_alt_bc(unsigned v) { alt_bc = (uint16_t)v; }
    unsigned get_alt_de() const { return alt_de; }
    void set_alt_de(unsigned v) { alt_de = (uint16_t)v; }
    unsigned get_alt_hl() const { return alt_hl; }
    void set_alt_hl(unsigned v) { alt_hl = (uint16_t)v; }
protected:
    uint16_t af = 0xFFFF, bc = 0, de = 0, hl = 0, ix = 0, iy = 0, sp = 0xFFFF, pc = 0, wz = 0, alt_af = 0, alt_bc = 0, alt_de = 0, alt_hl = 0;
    uint8_t i = 0, r = 0, im = 0;
    bool iff1 = false, iff2 = false, halted = false, int_disabled = false;
    int prefix_kind = 0;   // 0 = HL, 1 = IX, 2 = IY: the index register the prefix byte cut by a frame end selects
};
