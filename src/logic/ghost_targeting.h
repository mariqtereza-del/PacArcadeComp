// T01d: where each ghost wants to go (its "target tile") and how it chooses its next direction, as readable C++.
//
// ROM routines copied (docs/notes/gameplay_routines.md items 2966 and the target rules):
//   2966  the direction chooser: from the ghost's tile, a target tile and the direction it is going, pick the next direction.
//   29EA  the squared distance between two tiles (with the multiplication helper at 2A12).
//   2730  Blinky's target, 276C Pinky's, 27A9 Inky's, 27F1 Clyde's: each task picks a target and calls the chooser.
//
// A TILE is two bytes (a, b): a runs 32 + (y / 8), b runs 30 + (x / 8) (docs/notes/ram_map.md). A DIRECTION is 0 right, 1 down, 2 left, 3 up.
// A ghost never turns round by itself, never enters a wall and, among the ways left, takes the one whose next tile is nearest to the target
// (squared straight-line distance). If two ways are equally near the one with the HIGHER number wins (right < down < left < up).
#pragma once
#include <cstdint>
#include "work_ram.h"
#include "dots_and_fruit.h"

namespace logic {

struct Tile {
    uint8_t a, b;
};
inline uint16_t as_word(Tile t) { return (uint16_t)(t.a | (t.b << 8)); } // the ROM keeps a tile as a 16-bit word: a low, b high
inline Tile from_word(uint16_t w) { return Tile{(uint8_t)(w & 255), (uint8_t)(w >> 8)}; }

// ---- where things live
constexpr unsigned kGhostTileAddress[4] = {0x4D0A, 0x4D0C, 0x4D0E, 0x4D10}; // Blinky, Pinky, Inky, Clyde: tile a, b
constexpr unsigned kGhostDirectionAddress[4] = {0x4D2C, 0x4D2D, 0x4D2E, 0x4D2F};
constexpr unsigned kGhostVectorAddress[4] = {0x4D1E, 0x4D20, 0x4D22, 0x4D24}; // the chosen direction as a step (da, db)
constexpr unsigned kPacmanVectorAddress = 0x4D1C;                             // Pac-Man's direction as a step (da, db)
constexpr unsigned kPhaseAddress = 0x4DC1;                                    // scatter/chase phase counter: even = scatter, odd = chase
constexpr unsigned kElroyAddress = 0x4DB6;                                    // non-zero while Blinky is "Cruise Elroy" (hunts even in the scatter phase)
constexpr unsigned kLevelStateAddress = 0x4E04;                               // 3 = the maze is running
constexpr uint8_t kLevelStateRunning = 3;

// The four steps (ROM table at 32FF): right, down, left, up as (da, db).
constexpr int8_t kStepA[4] = {0, 1, 0, -1};
constexpr int8_t kStepB[4] = {-1, 0, 1, 0};
inline uint16_t step_word(unsigned direction) { return (uint16_t)((uint8_t)kStepA[direction] | ((uint8_t)kStepB[direction] << 8)); }
inline Tile tile_after_step(Tile t, unsigned direction) { return Tile{(uint8_t)(t.a + kStepA[direction]), (uint8_t)(t.b + kStepB[direction])}; }

// The corner each ghost runs to in the scatter phase (ROM: DE = 221D, 391D, 2040, 3B40; the high byte is b, the low byte a). They lie outside the maze.
constexpr Tile kScatterTarget[4] = {{0x1D, 0x22}, {0x1D, 0x39}, {0x40, 0x20}, {0x40, 0x3B}};

// ---- 2A12 and 29EA: distances
inline uint16_t square(uint8_t n) { return (uint16_t)(n * n); }
inline uint8_t distance_along_one_axis(uint8_t x, uint8_t y) { return x >= y ? (uint8_t)(x - y) : (uint8_t)(y - x); }
// The squared distance between two tiles: da*da + db*db, kept in 16 bits like the ROM does.
inline uint16_t squared_distance(Tile p, Tile q) { return (uint16_t)(square(distance_along_one_axis(p.a, q.a)) + square(distance_along_one_axis(p.b, q.b))); }

// ---- 2966: the direction chooser
struct Choice {
    uint8_t direction;
    uint16_t step;
}; // step = the new direction as the word (da low, db high)

// `video` is the maze: a tile whose code has both top bits set (code & 0xC0 == 0xC0) is a wall.
// The original keeps its working values in RAM; they are written here too so that memory stays identical: 4D3E tile, 4D40 target, 4D3B best
// direction so far, 4D3D the forbidden (reverse) direction, 4D42 the tile last tried, 4D44 best distance so far, 4DC7 the loop counter.
inline Choice choose_direction(WorkRam ram, VideoRam video, Tile ghost, Tile target, uint8_t direction) {
    const uint8_t reverse = direction ^ 2;
    ram[0x4D3E] = ghost.a;
    ram[0x4D3F] = ghost.b;
    ram[0x4D40] = target.a;
    ram[0x4D41] = target.b;
    ram[0x4D3B] = direction;
    ram[0x4D3D] = reverse;
    uint16_t best_distance = 0xFFFF;
    ram[0x4D44] = 0xFF;
    ram[0x4D45] = 0xFF;
    uint8_t best = direction; // if every way is blocked the ghost keeps its direction
    for (uint8_t d = 0; d < 4; ++d) {
        ram[0x4DC7] = d;
        if (d == reverse) continue; // never turn back
        const Tile next = tile_after_step(ghost, d);
        ram[0x4D42] = next.a;
        ram[0x4D43] = next.b;
        if ((video[video_address_of_tile(next.a, next.b)] & 0xC0) == 0xC0) continue; // a wall
        const uint16_t distance = squared_distance(target, next);
        if (best_distance < distance) continue; // farther than the best so far (equal distances: the later direction wins)
        best_distance = distance;
        best = d;
        ram[0x4D44] = (uint8_t)(distance & 255);
        ram[0x4D45] = (uint8_t)(distance >> 8);
        ram[0x4D3B] = d;
    }
    ram[0x4DC7] = 4;
    return Choice{best, step_word(best)};
}

// ---- the four target rules (each task: pick a target, ask the chooser, store the answer)
inline Tile tile_at(WorkRam ram, unsigned address) { return Tile{ram[address], ram[address + 1]}; }
inline void store_choice(WorkRam ram, unsigned ghost, const Choice &c) {
    ram[kGhostVectorAddress[ghost]] = (uint8_t)(c.step & 255);
    ram[kGhostVectorAddress[ghost] + 1] = (uint8_t)(c.step >> 8);
    ram[kGhostDirectionAddress[ghost]] = c.direction;
}
// The ghosts head for their corner only in the scatter phase of a running maze (even phase number).
inline bool scatter_phase(WorkRam ram) { return (ram[kPhaseAddress] & 1) == 0 && ram[kLevelStateAddress] == kLevelStateRunning; }

// Blinky (task at 2730): the corner while scattering, unless he is Cruise Elroy; otherwise Pac-Man's own tile.
inline void blinky_target_task(WorkRam ram, VideoRam video) {
    const bool scatter = scatter_phase(ram) && ram[kElroyAddress] == 0;
    const Tile target = scatter ? kScatterTarget[0] : tile_at(ram, kPacmanTileAddress);
    store_choice(ram, 0, choose_direction(ram, video, tile_at(ram, kGhostTileAddress[0]), target, ram[kGhostDirectionAddress[0]]));
}

// Pinky (276C): the corner while scattering; otherwise FOUR tiles ahead of Pac-Man. The ROM adds four times Pac-Man's step to his tile as 16-bit
// words, so a step of (da, db) = (-1, 0), which is the word 00FF, carries from a into b: "up" aims four tiles up AND four tiles to the side.
inline void pinky_target_task(WorkRam ram, VideoRam video) {
    Tile target = kScatterTarget[1];
    if (!scatter_phase(ram)) {
        const uint16_t pac_step = (uint16_t)(ram[kPacmanVectorAddress] | (ram[kPacmanVectorAddress + 1] << 8));
        target = from_word((uint16_t)(as_word(tile_at(ram, kPacmanTileAddress)) + (uint16_t)(pac_step * 4)));
    }
    store_choice(ram, 1, choose_direction(ram, video, tile_at(ram, kGhostTileAddress[1]), target, ram[kGhostDirectionAddress[1]]));
}

// Inky (27A9): the corner while scattering; otherwise a point found with Blinky: take the tile TWO ahead of Pac-Man (same 16-bit word sum, so
// the same carry quirk as Pinky) and double its distance from Blinky's tile: target = 2 * ahead - blinky, for each of a and b (8 bits each).
inline void inky_target_task(WorkRam ram, VideoRam video) {
    Tile target = kScatterTarget[2];
    if (!scatter_phase(ram)) {
        const Tile blinky = tile_at(ram, kGhostTileAddress[0]);
        const uint16_t pac_step = (uint16_t)(ram[kPacmanVectorAddress] | (ram[kPacmanVectorAddress + 1] << 8));
        const Tile ahead = from_word((uint16_t)(as_word(tile_at(ram, kPacmanTileAddress)) + (uint16_t)(pac_step * 2)));
        target = Tile{(uint8_t)(ahead.a * 2 - blinky.a), (uint8_t)(ahead.b * 2 - blinky.b)};
    }
    store_choice(ram, 2, choose_direction(ram, video, tile_at(ram, kGhostTileAddress[2]), target, ram[kGhostDirectionAddress[2]]));
}

// Clyde (27F1): the corner while scattering; otherwise Pac-Man's tile, but when he is closer than 8 tiles (squared distance under 64) he runs to his corner.
inline void clyde_target_task(WorkRam ram, VideoRam video) {
    const Tile clyde = tile_at(ram, kGhostTileAddress[3]), pac = tile_at(ram, kPacmanTileAddress);
    Tile target = kScatterTarget[3];
    if (!scatter_phase(ram) && squared_distance(pac, clyde) >= 0x40) target = pac;
    store_choice(ram, 3, choose_direction(ram, video, clyde, target, ram[kGhostDirectionAddress[3]]));
}

} // namespace logic
