// T02g: when the ghosts leave the ghost house, as readable C++.
//
// ROM routines copied (docs/notes/gameplay_routines.md item 7, docs/notes/ram_map.md):
//   1066  every update: the eaten ghost that was waiting (4DAB = which ghost, 1-4) is put into its "dead" state (4DAC-4DAF), one step at a time.
//   2069, 208C, 20AF  every update while the level runs (state 3): decide whether Pinky, Inky and Clyde leave the house now. Each looks at its own
//         "out" flag (4DA1-4DA3). Two ways to decide:
//           * normal: the ghost's own dot counter (4E0F, 4E10, 4E11) has reached its limit (4DB8, 4DB9, 4DBA; the limits come from the level start);
//           * after Pac-Man lost a life on this level (4E12 = 1): ONE common counter (4D9F) counts the dots; Pinky leaves at 7, Inky at 17, and at 32
//             Clyde's turn ends this mode (both 4E12 and 4D9F go back to 0; Clyde himself then follows the normal rule).
//         2086, 20A9, 20D1 (inside the routines) mark the ghost: Pinky 4DA1 = 2, Inky 4DA2 = 3, Clyde 4DA3 = 3.
//   13DD  every frame: the inactivity timer. If no dot was eaten (4E0E equals the value kept at 4D9E) it counts frames in 4D97; at the level's
//         limit (4D95) the first ghost still in the house (Pinky, then Inky, then Clyde) is let out. Eating a dot starts the count again.
//
// The dot counters themselves are counted in dots_and_fruit.h (ROM 1B08); the limits are set in mode_timers.h (ROM 070E).
// Not copied: the ghost state handlers (ROM 1094-10B4) and the eaten-ghost pause (ROM 1235) that sets 4DAB.
#pragma once
#include <cstdint>
#include "work_ram.h"

namespace logic {

constexpr unsigned kEatenGhostWaitingAddress = 0x4DAB;                                // 0 = none, 1-4 = the ghost (Blinky..Clyde) that goes to the "dead" state
constexpr unsigned kDeadStateAddress[4] = {0x4DAC, 0x4DAD, 0x4DAE, 0x4DAF};           // not 0 = that ghost is "dead" (eyes)
constexpr unsigned kOutOfHouseFlagAddress[3] = {0x4DA1, 0x4DA2, 0x4DA3};              // Pinky, Inky, Clyde
constexpr unsigned kOwnDotCounterAddress[3] = {0x4E0F, 0x4E10, 0x4E11};
constexpr unsigned kOwnDotLimitAddress[3] = {0x4DB8, 0x4DB9, 0x4DBA};
constexpr unsigned kCommonCounterModeAddress = 0x4E12, kCommonCounterAddress = 0x4D9F; // after a death: one counter for everybody
constexpr uint8_t kCommonLimit[3] = {7, 17, 32};
constexpr uint8_t kOutFlagValue[3] = {2, 3, 3};                                        // what 2086, 20A9, 20D1 store
constexpr unsigned kInactivityCounterAddress = 0x4D97, kInactivityLimitAddress = 0x4D95, kDotsAtLastMoveAddress = 0x4D9E, kDotsEatenCountAddress = 0x4E0E;

// ---- 1066
inline void eaten_ghost_waiting_tick(WorkRam ram) {
    const uint8_t n = ram[kEatenGhostWaitingAddress];
    if (n == 0) return;
    if (n <= 3) { // ghost n-1 becomes dead, nobody is waiting any more
        ram[kEatenGhostWaitingAddress] = 0;
        ram[kDeadStateAddress[n - 1]] = 1;
        return;
    }
    ram[kDeadStateAddress[3]] = (uint8_t)(n - 3); // the ROM stores the count itself here, and counts the wait down by 4 (never happens for n > 4)
    ram[kEatenGhostWaitingAddress] = (uint8_t)(n - 4);
}

// ---- 2086, 20A9, 20D1: let ghost `who` (0 Pinky, 1 Inky, 2 Clyde) out
inline void let_out(WorkRam ram, unsigned who) { ram[kOutOfHouseFlagAddress[who]] = kOutFlagValue[who]; }

// ---- 2069 (Pinky), 208C (Inky), 20AF (Clyde)
inline void house_release_check(WorkRam ram, unsigned who) {
    if (ram[kOutOfHouseFlagAddress[who]] != 0) return; // already out
    if (ram[kCommonCounterModeAddress] != 0) {
        if (ram[kCommonCounterAddress] != kCommonLimit[who]) return;
        if (who == 2) { // Clyde's turn ends the common counter mode; he is not let out here
            ram[kCommonCounterModeAddress] = 0;
            ram[kCommonCounterAddress] = 0;
            return;
        }
        let_out(ram, who);
        return;
    }
    if (ram[kOwnDotCounterAddress[who]] < ram[kOwnDotLimitAddress[who]]) return;
    let_out(ram, who);
}
inline void house_release_tick(WorkRam ram) { // the three in the order the game runs them
    for (unsigned who = 0; who < 3; ++who) house_release_check(ram, who);
}

// ---- 13DD: the inactivity timer
inline void inactivity_timer_tick(WorkRam ram) {
    auto word = [&](unsigned a) { return (uint16_t)(ram[a] | (ram[a + 1] << 8)); };
    auto set = [&](unsigned a, uint16_t v) {
        ram[a] = (uint8_t)(v & 255);
        ram[a + 1] = (uint8_t)(v >> 8);
    };
    if (ram[kDotsEatenCountAddress] != ram[kDotsAtLastMoveAddress]) { // a dot was eaten since: start again
        set(kInactivityCounterAddress, 0);
        return;
    }
    const uint16_t counter = (uint16_t)(word(kInactivityCounterAddress) + 1);
    set(kInactivityCounterAddress, counter);
    if (counter != word(kInactivityLimitAddress)) return;
    set(kInactivityCounterAddress, 0);
    for (unsigned who = 0; who < 3; ++who)
        if (ram[kOutOfHouseFlagAddress[who]] == 0) { // the first ghost still inside is let out
            let_out(ram, who);
            return;
        }
}

} // namespace logic
