// T01: the side-by-side probe for the readable game logic (src/logic/). It runs the real machine (the ROM's own code) one instruction at a time,
// exactly as Machine::run_frame does, and lets a test look at the machine before every instruction (to catch the entry and the return of a ROM
// routine). FOR TESTS ONLY. The frame loop here must give the same machine states as Machine::run_frame (checked by tests/test_logic_random.cpp).
#pragma once
#include <cstdint>
#include "../src/board/machine.h"

namespace probe {

// Runs one frame like Machine::run_frame() (50,688 cycles, then the VBLANK interrupt). before(pc) is called before every instruction.
template <class Before> inline void run_frame(Machine &m, Before &&before) {
    m.bus.io_log.clear();
    const uint32_t t0 = m.ticks_in_frame();
    for (;;) {
        before((uint16_t)m.on_get_pc());
        m.on_step();
        if (m.ticks_in_frame() - t0 >= Machine::cycles_per_frame) break;
    }
    ++m.frame;
    m.bus.set_frame(m.frame);
    m.deliver_vblank_interrupt();
}

inline unsigned rd(Machine &m, unsigned a) { return m.bus.read((uint16_t)a); }
inline unsigned rd16(Machine &m, unsigned a) { return rd(m, a) | (rd(m, a + 1) << 8); }

} // namespace probe
