// T01c: eating dots and energizers, the ghost house dot counters, the power pill start, and the appearance of the fruit, as readable C++.
//
// ROM routines copied (docs/notes/gameplay_routines.md items 19E0, 1B08, 0EAD, and the power pill 1A70):
//   19CD  Pac-Man has moved: look at the maze tile he stands on; if it holds a dot or an energizer, eat it.
//   1B08  count the dot for the ghost house rules (which ghost may leave when).
//   1A70  an energizer was eaten: the ghosts turn blue and the frightened timer starts.
//   0EAD  show the fruit when 70 or 170 dots have been eaten.
// The ROM routines are inline pieces of larger routines (they return to whoever called the bigger routine); here each is one function.
//
// Not copied: the code that lets Pac-Man EAT the fruit (ROM 19A1-19C4), and everything on the screen except the one maze tile that is blanked.
#pragma once
#include <cstdint>
#include "work_ram.h"
#include "scoring.h"

namespace logic {

// ---- where things live
constexpr unsigned kPacmanTileAddress = 0x4D39;       // Pac-Man's tile: 4D39 = "a" (the column-like coordinate, +32), 4D3A = "b" (+30)
constexpr unsigned kDotsEatenAddress = 0x4E0E;        // dots eaten in this level (counts 0..244, a byte)
constexpr unsigned kPacmanPauseAddress = 0x4D9D;      // frames Pac-Man waits: 0xFF = nothing eaten this step, 1 after a dot, 6 after an energizer
constexpr unsigned kFootstepSoundAddress = 0x4EBC;    // sound request bits 0 and 1: the two footstep sounds, used alternately for every dot
constexpr unsigned kFruitPositionAddress = 0x4DD2;    // word, 0 = no fruit on the screen
constexpr unsigned kFruitPointsAddress = 0x4DD4;      // points kind of the fruit on the screen (the PointsIndex), 0 = none
constexpr unsigned kFirstFruitShownAddress = 0x4E0C;  // 1 once the 70-dot fruit has appeared in this level
constexpr unsigned kSecondFruitShownAddress = 0x4E0D; // 1 once the 170-dot fruit has appeared
constexpr unsigned kFruitSpriteAddress = 0x4C0C;      // the fruit's sprite code (4C0C) and colour (4C0D)
constexpr unsigned kPacmanDyingAddress = 0x4DA5;      // non-zero while Pac-Man's death is shown
constexpr unsigned kLevelAddress = 0x4E13;            // level number minus 1
constexpr unsigned kTaskQueueWritePointerAddress = 0x4C80;
constexpr unsigned kTimedEventTableAddress = 0x4C90; // 16 slots of 3 bytes
constexpr unsigned kTimedEventSlots = 16;

constexpr uint8_t kTileDot = 0x10, kTileEnergizer = 0x14, kTileBlank = 0x40;
constexpr unsigned kFruitFirstAtDots = 70, kFruitSecondAtDots = 170; // the numbers 0x46 and 0xAA of the ROM
constexpr uint16_t kFruitPosition = 0x8094;                          // where the fruit appears (below the ghost house)
constexpr uint8_t kTaskAddScore = 0x19;                              // the id of the "add score" task in the task queue

// ---- the two little systems the routines use
// The task queue (ROM 0042): two bytes (task id, parameter) are stored at the write pointer, which then moves on two bytes and wraps inside 4Cxx
// from the end of the page back to 4CC0.
inline bool task_queue_push(WorkRam ram, uint8_t task, uint8_t parameter) { // returns true when the pointer wrapped back to 4CC0
    unsigned hl = ram[kTaskQueueWritePointerAddress] | (ram[kTaskQueueWritePointerAddress + 1] << 8);
    uint8_t l = (uint8_t)(hl & 255), h = (uint8_t)(hl >> 8);
    ram[(unsigned)(h << 8 | l)] = task;
    l = (uint8_t)(l + 1);
    ram[(unsigned)(h << 8 | l)] = parameter;
    l = (uint8_t)(l + 1);
    const bool wrapped = l == 0;
    if (wrapped) l = 0xC0;
    ram[kTaskQueueWritePointerAddress] = l;
    ram[kTaskQueueWritePointerAddress + 1] = h;
    return wrapped;
}

// A timed event (ROM rst 30h): three bytes go into the first free slot (a slot whose first byte is 0). Returns false when all 16 slots are taken
// (the ROM would then run on into the data bytes; that never happens in the game and is not modelled).
// Returns the number of the slot used (0..15), or -1 when none is free.
inline int schedule_timed_event_slot(WorkRam ram, uint8_t b0, uint8_t b1, uint8_t b2) {
    for (unsigned i = 0; i < kTimedEventSlots; ++i) {
        const unsigned slot = kTimedEventTableAddress + 3 * i;
        if (ram[slot] != 0) continue;
        ram[slot] = b0;
        ram[slot + 1] = b1;
        ram[slot + 2] = b2;
        return (int)i;
    }
    return -1;
}
inline bool schedule_timed_event(WorkRam ram, uint8_t b0, uint8_t b1, uint8_t b2) { return schedule_timed_event_slot(ram, b0, b1, b2) >= 0; }

// The video RAM address of a maze tile (ROM 202D), from Pac-Man's tile bytes a and b.
inline unsigned video_address_of_tile(uint8_t a, uint8_t b) { return kVideoRamStart + (uint8_t)(a - 32) + 32 * ((uint8_t)(b - 32) & 0x1F) + 0x40; }

// ---- 1B08: which ghost counts this dot?
// After Pac-Man has lost a life in this level (4E12 = 1) one common counter (4D9F) counts every dot. Otherwise each ghost in the house has its own
// counter, and the dot counts for the first ghost that is still inside: the flags 4DA1, 4DA2 and 4DA3 say that the second, third and fourth ghost
// (Pinky, Inky, Clyde) have left; when all have left nothing counts.
constexpr unsigned kDiedThisLevelAddress = 0x4E12, kGlobalDotCounterAddress = 0x4D9F;
constexpr unsigned kLeftHouseAddress[3] = {0x4DA1, 0x4DA2, 0x4DA3};          // Pinky out, Inky out, Clyde out
constexpr unsigned kPersonalDotCounterAddress[3] = {0x4E0F, 0x4E10, 0x4E11}; // Pinky's, Inky's, Clyde's
enum HouseCounter { kCountsNobody, kCountsCommon, kCountsClyde, kCountsInky, kCountsPinky }; // which counter took the dot (T02c: the game needs to know)
inline HouseCounter count_dot_for_ghost_house(WorkRam ram) {
    if (ram[kDiedThisLevelAddress] != 0) {
        ram[kGlobalDotCounterAddress] = (uint8_t)(ram[kGlobalDotCounterAddress] + 1);
        return kCountsCommon;
    }
    if (ram[kLeftHouseAddress[2]] != 0) return kCountsNobody; // everybody is out
    if (ram[kLeftHouseAddress[1]] != 0) {
        ram[kPersonalDotCounterAddress[2]] = (uint8_t)(ram[kPersonalDotCounterAddress[2]] + 1);
        return kCountsClyde;
    }
    if (ram[kLeftHouseAddress[0]] != 0) {
        ram[kPersonalDotCounterAddress[1]] = (uint8_t)(ram[kPersonalDotCounterAddress[1]] + 1);
        return kCountsInky;
    }
    ram[kPersonalDotCounterAddress[0]] = (uint8_t)(ram[kPersonalDotCounterAddress[0]] + 1);
    return kCountsPinky;
}

// ---- 1A70: an energizer was eaten
constexpr unsigned kFrightenedTimeAddress = 0x4DBD, kFrightenedCounterAddress = 0x4DCB; // words
constexpr unsigned kFrightenedAddress = 0x4DA6, kGhostEdibleAddress[4] = {0x4DA7, 0x4DA8, 0x4DA9, 0x4DAA};
constexpr unsigned kGhostReverseAddress[5] = {0x4DB1, 0x4DB2, 0x4DB3, 0x4DB4, 0x4DB5}; // "turn round" flags of the four ghosts (and one more)
constexpr unsigned kGhostChainAddress = 0x4DD0, kSirenAddress = 0x4DC8, kSoundBitsAddress = 0x4EAC;
constexpr uint8_t kGhostBlueSprite = 0x1C, kGhostBlueColour = 0x11;
inline void start_power_pill(WorkRam ram) {
    ram[kFrightenedCounterAddress] = ram[kFrightenedTimeAddress]; // the level's frightened time starts to run
    ram[kFrightenedCounterAddress + 1] = ram[kFrightenedTimeAddress + 1];
    ram[kFrightenedAddress] = 1;
    for (unsigned a : kGhostEdibleAddress) ram[a] = 1;  // all four ghosts can be eaten
    for (unsigned a : kGhostReverseAddress) ram[a] = 1; // and turn round
    ram[kSirenAddress] = 0;
    ram[kGhostChainAddress] = 0; // the next ghost eaten is the first of the chain (200 points)
    for (unsigned g = 0; g < 4; ++g) {
        ram[0x4C02 + 2 * g] = kGhostBlueSprite;
        ram[0x4C03 + 2 * g] = kGhostBlueColour;
    }                                                                            // the four ghosts' sprites turn blue
    ram[kSoundBitsAddress] = (uint8_t)((ram[kSoundBitsAddress] | 0x20) & ~0x80); // sound bit 5 on, bit 7 off
}

// ---- 19CD: Pac-Man is on a tile
struct EatOutcome { // what happened (T02c: the game needs to know, to carry on exactly like the ROM)
    bool ate = false, energizer = false, queue_wrapped = false;
    HouseCounter counter = kCountsNobody;
};
inline EatOutcome pacman_eats_on_tile(WorkRam ram, VideoRam video) {
    EatOutcome out;
    ram[kPacmanPauseAddress] = 0xFF; // nothing eaten (yet)
    const unsigned at = video_address_of_tile(ram[kPacmanTileAddress], ram[kPacmanTileAddress + 1]);
    const uint8_t tile = video[at];
    if (tile != kTileDot && tile != kTileEnergizer) return out;
    out.ate = true;
    ram[kDotsEatenAddress] = (uint8_t)(ram[kDotsEatenAddress] + 1);
    const bool energizer = tile == kTileEnergizer;
    video[at] = kTileBlank;                                             // the dot disappears from the maze
    out.queue_wrapped = task_queue_push(ram, kTaskAddScore, energizer ? kEnergizer : kDot); // 10 or 50 points
    out.energizer = energizer;
    ram[kPacmanPauseAddress] = energizer ? 6 : 1;                       // Pac-Man stops for 1 frame (dot) or 6 frames (energizer)
    out.counter = count_dot_for_ghost_house(ram);
    if (ram[kPacmanPauseAddress] == 6) start_power_pill(ram);
    if (ram[kDotsEatenAddress] & 1) {
        ram[kFootstepSoundAddress] = (uint8_t)((ram[kFootstepSoundAddress] & ~0x01) | 0x02);
    } // odd dot: the second footstep sound
    else {
        ram[kFootstepSoundAddress] = (uint8_t)((ram[kFootstepSoundAddress] & ~0x02) | 0x01);
    } // even dot: the first
    return out;
}

// ---- the fruit (ROM 0EAD; its table at 0EFD gives, for each level, the fruit's picture, colour and points)
struct FruitKind {
    uint8_t sprite_code, colour;
    PointsIndex points;
};
constexpr FruitKind kFruitKinds[8] = {{0, 0x14, kCherry}, {1, 0x0F, kStrawberry}, {2, 0x15, kOrange}, {4, 0x14, kApple},
                                      {5, 0x17, kMelon},  {6, 0x09, kGalaxian},   {3, 0x16, kBell},   {7, 0x16, kKey}};
// Which fruit a level (counted from 0) brings: cherry, strawberry, orange twice, apple twice, melon twice, galaxian twice, bell twice, then keys.
inline const FruitKind &fruit_for_level(unsigned level) {
    if (level == 0) return kFruitKinds[0];
    if (level == 1) return kFruitKinds[1];
    if (level >= 12) return kFruitKinds[7];
    return kFruitKinds[2 + (level - 2) / 2];
}
constexpr uint8_t kFruitTimeoutEvent[3] = {0x8A, 0x04, 0x00}; // the timed event that takes the fruit away again after a while
constexpr unsigned kFruitLastLevel = 20;                      // the ROM's table stops at level 21 (counted from 0: 20)

// Called every frame: shows the fruit when the dot count is exactly 70 (first fruit) or 170 (second fruit), once each, and only while no fruit is
// showing and Pac-Man is not dying.
enum FruitOutcome { // T02c: which way the ROM routine went
    kFruitDying, kFruitOnScreen, kFruitNotYet, kFruit70AlreadyShown, kFruit170AlreadyShown, kFruitShown70, kFruitShown170, kFruitNoFreeSlot
};
inline FruitOutcome maybe_show_fruit(WorkRam ram, unsigned *slot = nullptr, bool *level_clamped = nullptr) {
    if (ram[kPacmanDyingAddress] != 0) return kFruitDying;
    if (ram[kFruitPointsAddress] != 0) return kFruitOnScreen; // a fruit is already on the screen
    const uint8_t dots = ram[kDotsEatenAddress];
    FruitOutcome outcome;
    if (dots == kFruitFirstAtDots) {
        if (ram[kFirstFruitShownAddress] != 0) return kFruit70AlreadyShown;
        ram[kFirstFruitShownAddress] = (uint8_t)(ram[kFirstFruitShownAddress] + 1);
        outcome = kFruitShown70;
    } else if (dots == kFruitSecondAtDots) {
        if (ram[kSecondFruitShownAddress] != 0) return kFruit170AlreadyShown;
        ram[kSecondFruitShownAddress] = (uint8_t)(ram[kSecondFruitShownAddress] + 1);
        outcome = kFruitShown170;
    } else return kFruitNotYet;
    ram[kFruitPositionAddress] = (uint8_t)(kFruitPosition & 255);
    ram[kFruitPositionAddress + 1] = (uint8_t)(kFruitPosition >> 8);
    unsigned level = ram[kLevelAddress];
    if (level_clamped) *level_clamped = level >= kFruitLastLevel;
    if (level > kFruitLastLevel) level = kFruitLastLevel;
    const FruitKind &fruit = fruit_for_level(level);
    ram[kFruitSpriteAddress] = fruit.sprite_code;
    ram[kFruitSpriteAddress + 1] = fruit.colour;
    ram[kFruitPointsAddress] = (uint8_t)fruit.points;
    const int used = schedule_timed_event_slot(ram, kFruitTimeoutEvent[0], kFruitTimeoutEvent[1], kFruitTimeoutEvent[2]);
    if (slot) *slot = used < 0 ? 0 : (unsigned)used;
    return used < 0 ? kFruitNoFreeSlot : outcome;
}

} // namespace logic
