// N26: the timed events (the three-byte slots at 4C90 .. 4CBF, counted down by the interrupt handler every frame), written natively (no Z80). ROM code replaced (entries):
//   0221  the loop over the 16 slots: a slot with a non-zero first byte whose top two bits (the speed class) are below the class counter (4C8A) loses one count; when the low six bits
//         reach zero the slot is cleared and its handler runs (the number in the second byte, the parameter in the third) through RST 20h and the table at 0247
//   025B  the point where the ROM comes back from a handler: POP HL, POP BC, next slot (the same loop, resumed)
//   0894 06A3 058E 1272 212B 21F0 22B9   seven handlers that add one to a state byte (4E04, 4E03, 4E02, 4DD1, 4E06, 4E07, 4E08) and return
//   1000  1004   a handler that clears 4DD4 (1000 only) and the pair 4DD2/4DD3
// The handlers 100B and 0263 (they queue a task with RST 28h) stay ROM code and are reached through the hook at 0020 (N12-N16); so does the RST 20h itself: the loop hands over
// at the RST 20h with the stack exactly as the ROM leaves it (BC, HL, 025B and the return address 0247 pushed). Every instruction is accounted for (T-states, fetches, flags, WZ).
// No ROM bytes (the table at 0247 is read by the ROM's own RST 20h routine). Mem: rd, wr.
#pragma once
#include "state.h"
#include <cstdint>
#include "sprite_pics.h"
#include "handler.h"

namespace game {
namespace evt {
using spp::Z;
using spp::Result;

// the loop; resume = entered at 025B (a handler has just returned) instead of at 0221
template <class Mem> inline hnd::Out slots_loop(Z<Mem> &z, uint16_t &sp, bool resume) {
    hnd::Out o;
    bool skip_check = false;
    if (!resume) {
        z.set_hl(ram::timed_events); z.t(10);
        z.ld_a_nn(ram::byte_4C8A);
        z.c = z.a; z.t(4);
        z.b = 0x10; z.t(7);
    } else {
        { const uint16_t v = hnd::pop16(z, sp); z.set_hl(v); z.t(10); }
        { const uint16_t v = hnd::pop16(z, sp); z.b = (uint8_t)(v >> 8); z.c = (uint8_t)v; z.t(10); }
        skip_check = true;
    }
    for (;;) {
        if (!skip_check) {                                           // 022A
            z.ld_a_mem(z.hl());
            z.and_a();
            if (!z.jr((z.f & flag::Z) != 0, 0x025D)) {
                z.and_n(0xC0);
                z.f = flag::rlca(&z.a, z.f); z.t(4);
                z.f = flag::rlca(&z.a, z.f); z.t(4);
                z.f = flag::cp8(z.a, z.c); z.t(4);
                if (!z.jr(!(z.f & flag::C), 0x025D)) {
                    z.dec_mem(z.hl());
                    z.ld_a_mem(z.hl());
                    z.and_n(0x3F);
                    if (!z.jr((z.f & flag::Z) == 0, 0x025D)) {
                        z.ld_mem_r(z.hl(), z.a);
                        hnd::push16(z, sp, (uint16_t)(z.b << 8 | z.c)); z.t(11);
                        hnd::push16(z, sp, z.hl()); z.t(11);
                        z.inc_l();
                        z.ld_a_mem(z.hl());
                        z.inc_l();
                        z.b = z.mem.rd(z.hl()); z.t(7);
                        z.set_hl(0x025B); z.t(10);
                        hnd::push16(z, sp, z.hl()); z.t(11);
                        hnd::push16(z, sp, 0x0247); z.t(11);         // RST 20h
                        z.wz = 0x0020; o.next = 0x0020;
                        return o;
                    }
                }
            }
        }
        skip_check = false;
        z.inc_l(); z.inc_l(); z.inc_l();                             // 025D
        z.b = (uint8_t)(z.b - 1);                                    // DJNZ
        if (z.b != 0) { z.wz = 0x022A; z.t(13); } else { z.t(8); break; }
    }
    o.next = hnd::pop16(z, sp); z.t(10); z.wz = o.next;              // RET
    return o;
}

// the handlers: LD HL,addr / INC (HL) / RET
template <uint16_t Addr, class Mem> inline Result inc_byte(Z<Mem> &z) {
    z.set_hl(Addr); z.t(10);
    z.inc_mem(Addr);
    return z.ret(10);
}
// 1000: XOR A, LD (4DD4),A, then 1004: LD HL,0000 / LD (4DD2),HL / RET
template <class Mem> inline Result clear_1004(Z<Mem> &z) {
    z.set_hl(0x0000); z.t(10);
    z.mem.wr(ram::fruit_position, 0); z.mem.wr((ram::fruit_position + 1), 0); z.wz = (ram::fruit_position + 1); z.t(16);
    return z.ret(10);
}
template <class Mem> inline Result clear_1000(Z<Mem> &z) {
    z.xor_r(z.a);
    z.ld_nn_a(ram::fruit_points);
    return clear_1004(z);
}

} // namespace evt
} // namespace game
