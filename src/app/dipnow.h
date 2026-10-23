// dipnow.h (S34D): put new DIP switch settings (lives, bonus life, difficulty, ghost names) into a machine that is already running, without a restart.
// The arcade program reads its DIP switches only once, in the routine at ROM 26D0 (about frame 296 after power-on), and keeps the result in work RAM.
// So the exact same thing is done here at a quiet moment: the ROM's own routine 26D0 runs on a COPY of the machine with the new switch byte, and the
// work RAM of the copy (which differs from the real one only in the bytes that routine writes) is put back into the real machine. The CPU, the cycle
// count and the stack of the real machine are not touched. "A quiet moment" = the attract screen (main state 1): no game is being played or about to start.
#pragma once
#include <cstdint>
#include <cstring>
#include <memory>
#include "../board/machine.h"
#include "../rules/rules.h"

namespace dipnow {

inline bool quiet(const Machine &m) { return const_cast<Machine &>(m).bus.read(0x4E00) == 1; }   // 4E00: 1 = attract mode (0 = power-on, 2 = credit inserted, 3 = playing)

// Runs ROM 26D0 with `dsw1` on a copy and copies the work RAM back. Returns false (and changes nothing) if the routine did not return.
inline bool apply(Machine &m, uint8_t dsw1) {
    std::unique_ptr<Machine> t(new Machine(*&m));
    const uint16_t sp = 0x4FF0;                 // a scratch stack inside the copy: the return address 0000 stops the run
    t->bus.write(sp, 0x00);
    t->bus.write((uint16_t)(sp + 1), 0x00);
    t->on_set_sp(sp);
    t->on_set_pc(0x26D0);
    t->rules = rules::table_for(false);         // the original rules, whatever the real machine runs: the plain ROM with the Z80 core, the native table (standard tuning) without it
    t->set_ticks_in_frame(0);                   // a whole frame of room for the routine
    t->bus.dsw1 = dsw1;
    unsigned guard = 0;
    while (t->on_get_pc() != 0x0000 && guard++ < 2000) { const Machine::RuleHook h = t->rules ? t->rules[t->on_get_pc()] : nullptr; if (h) h(*t); else t->on_step(); }
    if (t->on_get_pc() != 0x0000) return false;
    for (uint16_t a = 0x4FE8; a < 0x4FF0; ++a) t->bus.write(a, m.bus.read(a));   // the scratch stack (the routine's own RST 10h pushes just below it) stays as it was
    std::memcpy(m.bus.workram, t->bus.workram, sizeof m.bus.workram);
    m.bus.dsw1 = dsw1;
    return true;
}

} // namespace dipnow
