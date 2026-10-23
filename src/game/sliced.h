// N17: routines that the vertical-blank interrupt can cut in the middle ("sliced" routines), written natively (no Z80).
// The ROM main loop ends a frame at the first instruction boundary at or after 50,688 cycles and then takes the interrupt. A long routine that is running at that moment is
// stopped between two instructions and goes on after the interrupt's return. The native version does the same: it runs only as many whole loop iterations as end before the
// limit, and then leaves the machine exactly in the state the ROM would have reached after those iterations (program counter at the loop head, registers, stack, flags, R, WZ).
// The few instructions up to the limit are then run by the ROM code itself, so the interrupt arrives at exactly the same instruction as in the original, and the routine
// carries on after the interrupt (the hook at the loop head takes it over again).
// `room` = the cycles of the frame still left (50,688 minus the cycles already used); an iteration group of C cycles may be run natively only when C < room (every
// instruction of it then starts before the limit).
// ROM routines replaced (entries): 0008 (block fill, now also cut in the middle), the screen clears 23F3 (all video RAM tiles = 40h), 2400 (the lower part), 240D (all colour RAM = 0),
// and their pass loops 23FB, 2408, 2414 (where the ROM is after a cut). No ROM bytes: the numbers are addresses and cycle counts.
// Mem: rd, wr.
#pragma once
#include <cstdint>
#include "helpers.h"
#include "screen.h"
#include "numbers.h"

namespace game {
namespace sliced {

constexpr unsigned kFillIterationCycles = 26, kFillIterationFetches = 3;   // LD (HL),A / INC HL / DJNZ taken: 7 + 6 + 13

// How many whole iterations of `per` cycles may be run natively inside `room` cycles (at most `most`).
inline unsigned iterations_that_fit(unsigned room, unsigned per, unsigned most) {
    if (room == 0) return 0;
    const unsigned k = (room - 1) / per;
    return k < most ? k : most;
}

// ---- 0008 cut: the fill loop is at its head (after the RST or after a DJNZ). Runs k of the n = B (0 = 256) rounds, k <= n - 1, so that the DJNZ of the last of them is taken
// and the ROM would be at 0008 again. Returns k (0 = nothing done). B = n - k, HL + k, cost 26 k T-states, 3 k fetches.
template <class Mem> inline unsigned fill_cut(Mem &mem, Regs &r, unsigned room, Cost &cost) {
    const unsigned n = fill_count_0008(r.b);
    const unsigned k = iterations_that_fit(room, kFillIterationCycles, n - 1);
    uint16_t hl = r.hl();
    for (unsigned i = 0; i < k; ++i) mem.wr(hl++, r.a);
    r.set_hl(hl);
    r.b = (uint8_t)(n - k);
    cost.add(kFillIterationCycles * k, kFillIterationFetches * k);
    return k;
}

// ---- the screen clears: A = the byte, HL = where, B = bytes per pass (0 = 256), C = passes (0 = 256): "RST 08h / DEC C / JR NZ" at a pass boundary, then RET.
// A pass: RST 11 + fill 26 n + 5 (with its RET) + DEC C 4 + JR 12 (7 on the last pass) [+ RET 10 on the last pass].
// Runs whole passes while they end before the limit. Then, if passes are left, it executes the RST of the next pass natively (pushes `ret_pc`) and as many fill rounds as fit
// (fill_cut), and stops with the ROM at 0008 (paused = true, sp has gone down by 2).
struct ClearResult {
    bool finished = false;   // the routine ran to its RET: the caller returns like RET
    bool paused = false;     // the RST was executed and part of the fill: the caller continues at 0008 with sp - 2
    unsigned rounds = 0;     // fill rounds done in the cut pass
};
// final_ret = false: the loop is inside a longer routine (24D7): the last pass has no RET, the caller goes on at the next instruction.
template <class Mem> inline ClearResult clear_passes(Mem &mem, Regs &r, uint16_t sp, uint16_t ret_pc, unsigned room, Cost &cost, bool final_ret = true) {
    ClearResult res;
    for (;;) {
        const unsigned n = fill_count_0008(r.b);
        const bool last = (uint8_t)(r.c - 1) == 0;
        const unsigned pass_cycles = 11 + fill_cycles_0008(n) + 4 + (last ? 7 + (final_ret ? 10 : 0) : 12);
        const unsigned pass_fetches = 1 + fill_fetches_0008(n) + 1 + 1 + (last && final_ret ? 1 : 0);
        if (cost.cycles + pass_cycles < room) {
            put_word(mem, (uint16_t)(sp - 2), ret_pc);                  // the RST's return address stays below the stack pointer
            uint16_t hl = r.hl();
            for (unsigned i = 0; i < n; ++i) mem.wr(hl++, r.a);
            r.set_hl(hl);
            r.b = 0;
            r.f = flag::dec8(r.c, r.f);
            r.c = (uint8_t)(r.c - 1);
            cost.add(pass_cycles, pass_fetches);
            if (last) { res.finished = true; return res; }
            continue;
        }
        put_word(mem, (uint16_t)(sp - 2), ret_pc);                      // RST 08h
        cost.add(11, 1);
        res.rounds = fill_cut(mem, r, room > cost.cycles ? room - cost.cycles : 0, cost);   // (the RST itself may end the frame)
        res.paused = true;
        return res;
    }
}

} // namespace sliced
} // namespace game
