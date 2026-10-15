// T01f: the timers that switch the ghosts between scatter and chase, the frightened timer, the ghost turn-round flags, and the per-level settings, as readable C++.
//
// ROM routines copied (docs/notes/gameplay_routines.md items 0E36 and the power pill; the level tables at 0796, 0843-0873 and 330F):
//   0E36  every frame: counts frames and, when the end of a phase is reached, starts the next phase (scatter, chase, scatter, ...) and tells all four
//         ghosts to turn round (the flags 4DB1-4DB4 that the movement of T01e uses).
//   1376  every frame: counts the frightened time down (the power pill itself is start_power_pill in T01c); at the end the ghosts become normal again.
//   070E  the start of a level: from the level number it copies that level's speed patterns (Pac-Man's and the ghosts'), the phase lengths, the ghost house
//         dot limits, the two Cruise Elroy limits, the frightened time and the inactivity time into work RAM.
//
// THE PHASES: the phase number 4DC1 counts 0 scatter, 1 chase, 2 scatter, 3 chase, 4 scatter, 5 chase, 6 scatter, 7 chase for ever. A frame counter (4DC2, a word)
// counts up all the time except while the ghosts are frightened; the phase ends when it equals the next limit of the level's table of seven limits (4D86).
// The limits are counted from the start of the level, not from the start of the phase. Even phases are scatter (see ghost_targeting.h).
//
// Not copied: the drawing of the level's fruits (ROM 2BEA, which the level start also calls; it only draws in video RAM).
#pragma once
#include <cstdint>
#include "work_ram.h"
#include "dots_and_fruit.h"
#include "movement.h"

namespace logic {

constexpr unsigned kPhaseNumberAddress = 0x4DC1, kPhaseCounterAddress = 0x4DC2, kPhaseLimitsAddress = 0x4D86;
constexpr unsigned kPacmanColourAddress = 0x4C0B;
constexpr uint8_t kLastPhase = 7; // from phase 7 (chase) on, nothing changes any more

// ---- 0E36: the scatter / chase timer
inline void mode_timer_tick(WorkRam ram) {
    if (ram[kFrightenedAddress] != 0) return; // the phase timer stands still while the ghosts are blue
    const uint8_t phase = ram[kPhaseNumberAddress];
    if (phase == kLastPhase) return;
    const uint16_t frames = (uint16_t)(word_at(ram, kPhaseCounterAddress) + 1);
    set_word(ram, kPhaseCounterAddress, frames);
    const uint8_t doubled = (uint8_t)(phase * 2);                      // the ROM doubles the phase number in one byte (it never gets above 7 in the game)
    if (frames != word_at(ram, kPhaseLimitsAddress + doubled)) return; // not at the end of this phase
    ram[kPhaseNumberAddress] = (uint8_t)(doubled / 2 + 1);
    for (unsigned g = 0; g < 4; ++g) ram[kGhostReverseAddress[g]] = 1; // every ghost turns round at a phase change
}

// ---- 1376: the frightened timer (the energizer's effect wearing off)
inline void frightened_timer_tick(WorkRam ram) {
    if (ram[kFrightenedAddress] == 0) return;
    const bool any_blue = (ram[kGhostEdibleAddress[0]] | ram[kGhostEdibleAddress[1]] | ram[kGhostEdibleAddress[2]] | ram[kGhostEdibleAddress[3]]) != 0;
    if (any_blue) { // count down; the end is when the counter reaches 0 (or no ghost is blue any more)
        const uint16_t left = (uint16_t)(word_at(ram, kFrightenedCounterAddress) - 1);
        set_word(ram, kFrightenedCounterAddress, left);
        if (left != 0) return;
    }
    ram[kPacmanColourAddress] = 9;   // Pac-Man's sprite colour (4C0B) is set back to 9
    for (unsigned g = 0; g < 4; ++g) // a ghost that is not "dead" (eyes, 4DAC+g != 0 would mean that) stops being edible
        if (ram[kGhostBlockedAddress[g]] == 0) ram[kGhostEdibleAddress[g]] = 0;
    set_word(ram, kFrightenedCounterAddress, 0);
    ram[kFrightenedAddress] = 0;
    ram[kSirenAddress] = 0;
    ram[kGhostChainAddress] = 0;
    ram[kSoundBitsAddress] &= (uint8_t) ~(0x20 | 0x80);
}

// ---- 070E: the settings of a level
// The ROM keeps, for each of 7 speed rows, a block of seven speed patterns and seven phase limits (table at 330F); here they are listed by kind.
// The order of the seven patterns in memory is Pac-Man normal, Pac-Man while ghosts are blue, Blinky's Cruise Elroy 2 and 1, then the ghosts' normal, blue
// and tunnel speeds (the three ghosts after Blinky get a copy of Blinky's last three).
// The speed rows (one per speed row number 0-6 of the level table). Pac-Man's speeds, Blinky's Cruise Elroy speeds and the ghosts' speeds are listed by kind.
// A speed pattern is 32 bits that are shifted left once per update; a 1 bit that falls out means "move one pixel" (0x55555555 moves every second update).
constexpr uint32_t kPacmanNormalSpeed[7] = {0x2A552A55, 0x4A5294A5, 0x2A552A55, 0x55555555, 0x6AD56AD5, 0x6D6D6D6D, 0x6AD56AD5};
constexpr uint32_t kPacmanBlueSpeed[7] = {0x55555555, 0x2AAA5555, 0x55555555, 0x6AD56AD5, 0x5AD6B5AD, 0x6D6D6D6D, 0x6AD56AD5};
constexpr uint32_t kElroy2Speed[7] = {0x2A552A55, 0x2A552A55, 0x2AAA5555, 0x6AAAD555, 0x5AD6B5AD, 0x6DB6DB6D, 0x6DB6DB6D};
constexpr uint32_t kElroy1Speed[7] = {0x4A5294A5, 0x4A5294A5, 0x2A552A55, 0x55555555, 0x6AD56AD5, 0x6D6D6D6D, 0x6D6D6D6D};
constexpr uint32_t kGhostNormalSpeed[7] = {0x25252525, 0x24924925, 0x4A5294A5, 0x2AAA5555, 0x6AAAD555, 0x5AD6B5AD, 0x5AD6B5AD};
constexpr uint32_t kGhostBlueSpeed[7] = {0x22222222, 0x24489122, 0x24489122, 0x24922492, 0x24924925, 0x25252525, 0x24489122};
constexpr uint32_t kGhostTunnelSpeed[7] = {0x01010101, 0x01010101, 0x44210844, 0x22222222, 0x24489122, 0x24922492, 0x24922492};
// How many frames each of the seven phases lasts, counted from the start of the level (so each limit includes the earlier ones), one line per speed row.
constexpr uint16_t kPhaseEnd[7][7] = {
    {600, 1800, 2400, 3600, 4200, 6000, 6420},   {0, 0, 0, 0, 0, 0, 0},
    {600, 2100, 2520, 4020, 4440, 5640, 5940},   {420, 1620, 2040, 3240, 3540, 4740, 5040},
    {420, 1620, 2040, 3240, 3540, 65534, 65535}, {300, 1500, 1800, 3000, 3300, 65534, 65535},
    {300, 1500, 1800, 3000, 3300, 65534, 65535},
};
// What each level uses (index = the level's row number 0-20, as in the ROM's level table). Each list is by kind, one number per level.
// speed row (into the speed tables above), the byte kept in 4DB0, ghost house limits row, Cruise Elroy row, frightened time row, inactivity time row.
constexpr uint8_t kSpeedRowOfLevel[21] = {3, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6};
constexpr uint8_t kSetting4DB0OfLevel[21] = {1, 1, 1, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 2, 1, 2, 2, 2, 2, 2, 2};
constexpr uint8_t kHouseLimitsRowOfLevel[21] = {1, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
constexpr uint8_t kElroyRowOfLevel[21] = {0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7};
constexpr uint8_t kFrightenedRowOfLevel[21] = {2, 3, 4, 5, 6, 3, 6, 6, 7, 3, 6, 7, 7, 5, 7, 7, 8, 7, 8, 8, 8};
constexpr uint8_t kInactivityRowOfLevel[21] = {0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr uint8_t kHouseLimits[4][3] = {{20, 30, 70}, {0, 30, 60}, {0, 0, 50}, {0, 0, 0}};
constexpr uint8_t kElroy1DotsLeft[9] = {20, 30, 40, 50, 60, 80, 100, 120, 140}; // Blinky's first speed-up: when this many dots are left
constexpr uint8_t kElroy2DotsLeft[9] = {10, 15, 20, 25, 30, 40, 50, 60, 70};    // and the second one
constexpr uint16_t kFrightenedTime[9] = {960, 840, 720, 600, 480, 360, 240, 120, 1};
constexpr uint16_t kInactivityTime[3] = {240, 240, 180};
constexpr unsigned kLevelCount = 21;
constexpr unsigned kSpeedPatternAddress[7] = {0x4D46, 0x4D4A, 0x4D4E, 0x4D52, 0x4D56, 0x4D5A, 0x4D5E}; // high word at this address, low word 2 on
constexpr unsigned kLevelSettingAddress = 0x4DB0, kHouseLimitsAddress = 0x4DB8, kElroyDotsLeftAddress = 0x4DBB, kInactivityTimeAddress = 0x4D95;

inline void set_pattern(WorkRam ram, unsigned address, uint32_t pattern) {
    set_word(ram, address, (uint16_t)(pattern >> 16));
    set_word(ram, address + 2, (uint16_t)pattern);
}

// `level_row` is the row number (0 to 20) of the level.
inline void start_level_timers(WorkRam ram, unsigned level_row) {
    const unsigned row = kSpeedRowOfLevel[level_row];
    const uint32_t patterns[7] = {kPacmanNormalSpeed[row], kPacmanBlueSpeed[row], kElroy2Speed[row],     kElroy1Speed[row],
                                  kGhostNormalSpeed[row],  kGhostBlueSpeed[row],  kGhostTunnelSpeed[row]};
    for (unsigned i = 0; i < 7; ++i) set_pattern(ram, kSpeedPatternAddress[i], patterns[i]);
    for (unsigned g = 1; g < 4; ++g) // Pinky, Inky and Clyde move at Blinky's normal, blue and tunnel speeds
        for (unsigned i = 0; i < 3; ++i) set_pattern(ram, kSpeedNormalAddress[g] + 4 * i, patterns[4 + i]);
    for (unsigned i = 0; i < 7; ++i) set_word(ram, kPhaseLimitsAddress + 2 * i, kPhaseEnd[row][i]);
    ram[kLevelSettingAddress] = kSetting4DB0OfLevel[level_row];
    for (unsigned i = 0; i < 3; ++i) ram[kHouseLimitsAddress + i] = kHouseLimits[kHouseLimitsRowOfLevel[level_row]][i];
    ram[kElroyDotsLeftAddress] = kElroy1DotsLeft[kElroyRowOfLevel[level_row]];
    ram[kElroyDotsLeftAddress + 1] = kElroy2DotsLeft[kElroyRowOfLevel[level_row]];
    set_word(ram, kFrightenedTimeAddress, kFrightenedTime[kFrightenedRowOfLevel[level_row]]);
    set_word(ram, kInactivityTimeAddress, kInactivityTime[kInactivityRowOfLevel[level_row]]);
}

} // namespace logic
