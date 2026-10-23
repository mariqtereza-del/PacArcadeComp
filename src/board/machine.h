// S18, N49: the machine = CPU + Bus (bus.h) + the frame loop with the VBLANK interrupt.
// No video, no sound. Frame rules (hardware_facts.md section 1): 50,688 CPU cycles per frame, then the VBLANK interrupt is taken if
// the program has enabled it (latch bit 5000) and the CPU accepts interrupts (IFF1 set, not directly after EI); mode 2: the CPU pushes PC,
// clears IFF1/IFF2 and jumps to the word at (I*256 + vector byte written by OUT (0),A).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "bus.h"
#ifdef PAC_NO_Z80
#include "cpuregs.h"   // N49a: the machine without the Z80 core (the program build of N49b); the tests build the other way
#else
#include "../../tests/oracle/z80.h"
#endif

#ifdef PAC_NO_Z80
// N49c: called with a short message just before the program stops because of an internal error (the program sets it to write its log file)
inline void (*fatal_hook)(const char *) = nullptr;
class Machine : public CpuRegs {
public:
#else
class Machine : public z80::z80_cpu<Machine> {
public:
    typedef z80::z80_cpu<Machine> base;
#endif
    explicit Machine(const std::vector<uint8_t> &program);

    // Runs one frame: instructions until at least 50,688 cycles have passed (the instruction that crosses the limit completes and the
    // excess is not carried over), then the interrupt. Returns the frame number (1-based).
    uint32_t run_frame();

    // Reproduce the Python prototype's harness exactly (tools/analysis/harness.py with the z80 package): its wrapper also ends a frame
    // whenever its own free-running tick counter crosses a multiple of 100,000, which cuts about every second frame short, and it
    // adds 1 to PC when interrupting a halted CPU. Off by default: the port uses the exact frame.
    bool prototype_quirks = false;

    Bus bus;
    uint32_t frame = 0;
    static const unsigned cycles_per_frame = 50688; // 3,072,000 Hz / 60.606 Hz, hardware_facts.md section 1

    // z80 core callbacks
#ifdef PAC_NO_Z80
    typedef unsigned fast_u8_t;
    unsigned on_read(unsigned a) { return bus.read((uint16_t)a); }
    void on_write(unsigned a, unsigned v) { bus.write((uint16_t)a, (uint8_t)v); }
    unsigned on_input(unsigned p) { return bus.port_in((uint16_t)p); }
    // The only step left of the CPU: a halted CPU keeps fetching (4 cycles, R + 1, interrupts allowed again). Anything else means that an address has
    // no native routine: that is a bug, the program stops (the N48 test proves it never happens).
    void on_step() {
        if (!on_is_halted()) {
            char msg[160];
            std::snprintf(msg, sizeof msg, "no native routine at address %04X (the machine without the Z80 cannot run a ROM instruction)", on_get_pc());
            std::fprintf(stderr, "FATAL: %s\n", msg);
            if (fatal_hook) fatal_hook(msg);   // N49c: the program writes the message into its log file before it stops
            std::abort();
        }
        on_set_is_int_disabled(false);
        on_set_r((on_get_r() & 0x80) | ((on_get_r() + 1) & 0x7F));
        on_tick(4);
    }
    void on_output(unsigned p, unsigned v) { bus.port_out((uint16_t)p, (uint8_t)v); }
#else
    z80::fast_u8 on_read(z80::fast_u16 a) { return bus.read(a); }
    void on_write(z80::fast_u16 a, z80::fast_u8 v) { bus.write(a, v); }
    z80::fast_u8 on_input(z80::fast_u16 p) { return bus.port_in(p); }
    void on_output(z80::fast_u16 p, z80::fast_u8 v) { bus.port_out(p, v); }
#endif
    void on_tick(unsigned t) {
        ticks += t;
        frame_tick += t;
        if (frame_tick >= 100000) {
            frame_tick %= 100000;
            wrapped = true;
        }
    }

    // T02a: the readable rules (src/rules/rules.h). nullptr (the standard) = the original ROM routines run, exactly as before. Otherwise a table
    // of 65,536 entries, one per address: when the next instruction's address has an entry, that function runs INSTEAD of the ROM routine that
    // starts there, and must leave the machine as the routine's RET would (src/rules/rules.h, return_like_ret). Purist mode never sets it.
    typedef void (*RuleHook)(Machine &);
    const RuleHook *rules = nullptr;

    // N49: the only CPU detail the native code (src/rules) needs besides the registers: which index register (IX / IY) the instruction prefix byte
    // that the frame end just cut selects. Wrapped here so that the native code does not name the Z80 core; the native machine of N49a has its own.
    enum class Prefix { hl, ix, iy };
#ifdef PAC_NO_Z80
    Prefix get_prefix() { return (Prefix)prefix_kind; }
    void set_prefix(Prefix k) { prefix_kind = (int)k; }
#else
    Prefix get_prefix() { const auto k = on_get_iregp_kind(); return k == z80::iregp::ix ? Prefix::ix : k == z80::iregp::iy ? Prefix::iy : Prefix::hl; }
    void set_prefix(Prefix k) { on_set_iregp_kind(k == Prefix::ix ? z80::iregp::ix : k == Prefix::iy ? z80::iregp::iy : z80::iregp::hl); }
#endif

    uint32_t ticks_in_frame() const { return ticks; }
    void set_ticks_in_frame(uint32_t t) { ticks = t; } // tests only (T02b): start a routine at a chosen point of the frame
    void deliver_vblank_interrupt();

private:
    uint32_t ticks = 0, frame_tick = 0;
    bool wrapped = false;
};

// SHA-1 of a byte range as 40 hex characters (used for the per-frame RAM hashes).
std::string sha1_hex(const uint8_t *data, size_t n);
