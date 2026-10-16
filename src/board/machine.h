// S18: the machine = Z80 core (src/core/z80.h) + Bus (bus.h) + the frame loop with the VBLANK interrupt.
// No video, no sound. Frame rules (hardware_facts.md section 1): 50,688 CPU cycles per frame, then the VBLANK interrupt is taken if
// the program has enabled it (latch bit 5000) and the CPU accepts interrupts (IFF1 set, not directly after EI); mode 2: the CPU pushes PC,
// clears IFF1/IFF2 and jumps to the word at (I*256 + vector byte written by OUT (0),A).
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "bus.h"
#include "../core/z80.h"

class Machine : public z80::z80_cpu<Machine> {
public:
    typedef z80::z80_cpu<Machine> base;
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
    z80::fast_u8 on_read(z80::fast_u16 a) { return bus.read(a); }
    void on_write(z80::fast_u16 a, z80::fast_u8 v) { bus.write(a, v); }
    z80::fast_u8 on_input(z80::fast_u16 p) { return bus.port_in(p); }
    void on_output(z80::fast_u16 p, z80::fast_u8 v) { bus.port_out(p, v); }
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

    uint32_t ticks_in_frame() const { return ticks; }
    void set_ticks_in_frame(uint32_t t) { ticks = t; } // tests only (T02b): start a routine at a chosen point of the frame
    void deliver_vblank_interrupt();

private:
    uint32_t ticks = 0, frame_tick = 0;
    bool wrapped = false;
};

// SHA-1 of a byte range as 40 hex characters (used for the per-frame RAM hashes).
std::string sha1_hex(const uint8_t *data, size_t n);
