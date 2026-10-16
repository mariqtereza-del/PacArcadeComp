// T01e: how Pac-Man and the four ghosts move, as readable C++.
//
// ROM routines copied (docs/notes/gameplay_routines.md items 1806, 2000-205A, 1B36):
//   1806  Pac-Man's movement for one update: the speed pattern, reading the joystick, turning, walls, the move itself, then eating (the dot part, 19CD,
//         is the T01c code), eating the fruit, and Pac-Man's walk in the attract demo / between levels (1A19).
//   1B36, 1C4B, 1D22, 1DF9  the movement of Blinky, Pinky, Inky and Clyde (one copy of the ROM code each, which differ only in the addresses they use,
//         and in Blinky's Cruise Elroy speed-ups; here they are ONE function with a table of addresses).
//   2000 add a step to a position, 2018 pixel position to tile, 205A tunnel test, 1ED0 "is this tile outside the maze rows", 1EFE-1F73 "turn round".
//
// POSITIONS: a pixel position is two bytes (a, b) like a tile; the tile is (pixel / 8) + (0x20, 0x1E). A ghost / Pac-Man is in the MIDDLE of a tile when
// its pixel position & 7 is 4. The speed is a 32-bit pattern of bits shifted left once per update: when a 1 bit falls out, the figure moves one pixel.
//
// Not copied: the other tasks the movement queues (the ghost direction tasks 08-0F, 17, the score tasks); the movement only queues them.
// Memory the ROM reads outside the screen RAMs (work RAM through a tile address) is not modelled.
#pragma once
#include <cstdint>
#include "work_ram.h"
#include "dots_and_fruit.h"
#include "ghost_targeting.h"

namespace logic {

// ---- the two screen memories, together (a tile address can run from the video RAM into the colour RAM and beyond)
constexpr unsigned kColourRamStart = 0x4400, kColourRamSize = 0x400;
struct Screen {
    uint8_t *video, *colour; // 4000-43FF and 4400-47FF
    // 4800-4BFF reads 0xBF (nothing there) and ignores writes; everything else above is not modelled (reads 0).
    uint8_t read(unsigned address) const {
        if (address >= kVideoRamStart && address < kVideoRamStart + kVideoRamSize) return video[address - kVideoRamStart];
        if (address >= kColourRamStart && address < kColourRamStart + kColourRamSize) return colour[address - kColourRamStart];
        if (address >= 0x4800 && address < 0x4C00) return 0xBF;
        return 0;
    }
};

// ---- small helpers on RAM words
inline uint16_t word_at(WorkRam ram, unsigned address) { return (uint16_t)(ram[address] | (ram[address + 1] << 8)); }
inline void set_word(WorkRam ram, unsigned address, uint16_t v) {
    ram[address] = (uint8_t)(v & 255);
    ram[address + 1] = (uint8_t)(v >> 8);
}

// ROM 2000: position + step, byte by byte (no carry between the bytes).
inline uint16_t add_step(WorkRam ram, unsigned step_address, unsigned position_address) {
    return (uint16_t)((uint8_t)(ram[position_address] + ram[step_address]) | ((uint8_t)(ram[position_address + 1] + ram[step_address + 1]) << 8));
}
// ROM 2018: pixel position (low byte = x-like, high byte = y-like) to the tile that holds it.
inline uint16_t tile_of_pixel(uint16_t pixel) { return (uint16_t)((uint8_t)((pixel & 255) / 8 + 0x20) | ((uint8_t)((pixel >> 8) / 8 + 0x1E) << 8)); }
// ROM 205A: the colour RAM of the tile says whether it is a tunnel tile (colour 0x1B); the answer goes to a flag byte.
inline unsigned colour_address_of_tile(uint16_t tile) { return video_address_of_tile((uint8_t)(tile & 255), (uint8_t)(tile >> 8)) + 0x400; }
inline void store_tunnel_flag(WorkRam ram, Screen screen, uint16_t tile, unsigned flag_address) {
    ram[flag_address] = screen.read(colour_address_of_tile(tile)) == 0x1B ? 1 : 0;
}
// ROM 1ED0 (tile word at 4D09 + 2*index, looking at its high byte): outside the maze rows (b below 0x21 or from 0x3B on), or exactly at the two tunnel
// ends (0x1D, 0x3E), which are moved to the other end: returns true ("carry").
inline bool tile_outside_maze_rows(WorkRam ram, unsigned index) {
    const unsigned address = 0x4D09 + 2 * index;
    const uint8_t b = ram[address];
    if (b == 0x1D) {
        ram[address] = 0x3D;
        return true;
    }
    if (b == 0x3E) {
        ram[address] = 0x1E;
        return true;
    }
    return b < 0x21 || b >= 0x3B;
}

// ---- the speed pattern: a 32-bit value kept as two words (high word at `hi`, low word at hi+2); shifted left by one; when a bit falls out the
// figure moves, and the bit is fed back in at the bottom (the pattern repeats).
inline bool advance_speed_pattern(WorkRam ram, unsigned hi) {
    const unsigned lo = hi + 2;
    const uint32_t low = (uint32_t)word_at(ram, lo) << 1;
    set_word(ram, lo, (uint16_t)low);
    const uint32_t high = ((uint32_t)word_at(ram, hi) << 1) | (low >> 16);
    set_word(ram, hi, (uint16_t)high);
    if (!(high >> 16)) return false;
    ram[lo] = (uint8_t)(ram[lo] + 1);
    return true;
}

// ---- Pac-Man
constexpr unsigned kPacmanPixelAddress = 0x4D08, kPacmanStepAddress = 0x4D1C, kPacmanDirectionAddress = 0x4D30;
constexpr unsigned kPacmanWantedStepAddress = 0x4D26, kPacmanWantedDirectionAddress = 0x4D3C, kPacmanInTunnelRowFlagAddress = 0x4DBF;
constexpr unsigned kPacmanDemoTileAddress = 0x4D12; // the demo's own tile word (index 5 of the 1ED0 table)
constexpr unsigned kGameModeAddress = 0x4E00, kLevelStateAddress2 = 0x4E04, kPlayerAddress = 0x4E09, kCocktailAddress = 0x4E72;
constexpr uint8_t kTaskDemoTurn = 0x17, kTaskFruitScoreSprite = 0x1C;
constexpr uint8_t kTimedEventFruitEaten[3] = {0x54, 0x05, 0x00};

struct JoystickPorts {
    uint8_t in0, in1;
}; // what the ROM reads at 5000 and 5040 (a 0 bit = pressed: bit 0 up, 1 left, 2 right, 3 down)

// One update of Pac-Man's movement (ROM 1806).
// T02e: `eat` = false leaves out the last step (19CD, the dots and energizers), for the game, which runs that ROM routine's readable version on its own.
inline void pacman_move(WorkRam ram, Screen screen, JoystickPorts ports, bool eat = true) {
    const VideoRam video{screen.video, screen.colour};
    // -- 1806: is it time to move?
    if (ram[0x4D9D] != 0xFF) {
        ram[0x4D9D] = (uint8_t)(ram[0x4D9D] - 1);
        return;
    } // still waiting after a dot
    const bool moves = ram[0x4DA6] ? advance_speed_pattern(ram, 0x4D4A) : advance_speed_pattern(ram, 0x4D46);
    if (!moves) return;
    ram[0x4D9E] = ram[kDotsEatenAddress];
    const uint8_t second_port = (uint8_t)(ram[kPlayerAddress] & ram[kCocktailAddress]); // player 2 on a cocktail table uses the second stick
    const uint8_t tile_b = ram[0x4D3A];
    const bool in_maze_rows = tile_b >= 0x21 && tile_b < 0x3B;
    if (!in_maze_rows) ram[kPacmanInTunnelRowFlagAddress] = 1;
    const bool demo_walk = ram[kGameModeAddress] == 1 || ram[kLevelStateAddress2] >= 0x10;

    if (demo_walk) {
        // -- 1A19: Pac-Man walks by himself: at a tile middle ask the demo task for a turn, then go on in the current direction
        const bool at_middle = ram[kPacmanStepAddress] != 0 ? (ram[kPacmanPixelAddress] & 7) == 4 : (ram[kPacmanPixelAddress + 1] & 7) == 4;
        if (at_middle) {
            if (!tile_outside_maze_rows(ram, 5)) task_queue_push(ram, kTaskDemoTurn, 0);
            set_word(ram, kPacmanDemoTileAddress, add_step(ram, kPacmanWantedStepAddress, kPacmanDemoTileAddress));
            set_word(ram, kPacmanStepAddress, word_at(ram, kPacmanWantedStepAddress));
            ram[kPacmanDirectionAddress] = ram[kPacmanWantedDirectionAddress];
        }
        set_word(ram, kPacmanPixelAddress, add_step(ram, kPacmanStepAddress, kPacmanPixelAddress));
        goto moved_to; // (1A67: straight to the end, no centring)
    }
    {
        const uint8_t stick = second_port ? ports.in1 : ports.in0;
        if (!in_maze_rows) {
            // -- 1864: in the tunnel rows only left/right count, and the new direction is taken at once
            if (!(stick & 2)) {
                ram[kPacmanDirectionAddress] = 2;
                set_word(ram, kPacmanStepAddress, step_word(2));
            } else if (!(stick & 4)) {
                ram[kPacmanDirectionAddress] = 0;
                set_word(ram, kPacmanStepAddress, step_word(0));
            }
        } else {
            // -- 18AB: the wanted step is the pressed direction (left, right, up, down in that order of priority), or else the current one
            uint8_t b; // 0: a direction was pressed, 1: none
            if (!(stick & 2)) {
                set_word(ram, kPacmanWantedStepAddress, step_word(2));
                ram[kPacmanWantedDirectionAddress] = 2;
                b = 0;
            } else if (!(stick & 4)) {
                set_word(ram, kPacmanWantedStepAddress, step_word(0));
                ram[kPacmanWantedDirectionAddress] = 0;
                b = 0;
            } else if (!(stick & 1)) {
                set_word(ram, kPacmanWantedStepAddress, step_word(3));
                ram[kPacmanWantedDirectionAddress] = 3;
                b = 0;
            } else if (!(stick & 8)) {
                set_word(ram, kPacmanWantedStepAddress, step_word(1));
                ram[kPacmanWantedDirectionAddress] = 1;
                b = 0;
            } else {
                set_word(ram, kPacmanWantedStepAddress, word_at(ram, kPacmanStepAddress));
                b = 1;
            }
            auto wall_ahead = [&](unsigned step_address) {
                const uint16_t next = add_step(ram, step_address, kPacmanTileAddress); // the tile one step on (200F)
                return (screen.read(video_address_of_tile((uint8_t)(next & 255), (uint8_t)(next >> 8))) & 0xC0) == 0xC0;
            };
            // at the middle of its tile Pac-Man stops when blocked (odd directions run along one pixel byte, even ones along the other)
            auto at_tile_middle = [&]() { return (ram[kPacmanDirectionAddress] & 1) ? (ram[kPacmanPixelAddress] & 7) == 4 : (ram[kPacmanPixelAddress + 1] & 7) == 4; };
            bool take_wanted = false, take_direction = false;
            if (wall_ahead(kPacmanWantedStepAddress)) {
                if (b == 1) { // no stick: just keep going until the wall
                    if (at_tile_middle()) return;
                    take_wanted = true;
                    take_direction = true;                   // (the ROM falls into 1940 here, with b counted down once too often)
                } else if (wall_ahead(kPacmanStepAddress)) { // wanted way blocked AND the way he goes blocked
                    if (at_tile_middle()) return;
                } // else: keep the current step
            } else {
                take_wanted = true;
                take_direction = b == 0;
            }
            if (take_wanted) { // 1940
                set_word(ram, kPacmanStepAddress, word_at(ram, kPacmanWantedStepAddress));
                if (take_direction) ram[kPacmanDirectionAddress] = ram[kPacmanWantedDirectionAddress];
            }
        }
        // -- 1950: the move, then the pull to the middle of the tile on the other axis
        uint16_t pixel = add_step(ram, kPacmanStepAddress, kPacmanPixelAddress);
        uint8_t l = (uint8_t)(pixel & 255), h = (uint8_t)(pixel >> 8);
        if (ram[kPacmanDirectionAddress] & 1) {
            const uint8_t m = h & 7;
            if (m < 4) ++h;
            else if (m > 4) --h;
        } else {
            const uint8_t m = l & 7;
            if (m < 4) ++l;
            else if (m > 4) --l;
        }
        set_word(ram, kPacmanPixelAddress, (uint16_t)(l | (h << 8)));
    }
moved_to:
    // -- 1985: the new tile
    set_word(ram, kPacmanTileAddress, tile_of_pixel(word_at(ram, kPacmanPixelAddress)));
    const uint8_t tunnel_row = ram[kPacmanInTunnelRowFlagAddress];
    ram[kPacmanInTunnelRowFlagAddress] = 0;
    if (tunnel_row) return; // no eating on a tunnel-row step
    // -- 199B: Pac-Man on the fruit
    if (ram[kFruitPositionAddress] != 0 && ram[kFruitPointsAddress] != 0 && word_at(ram, kPacmanPixelAddress) == kFruitPosition) {
        const uint8_t points = ram[kFruitPointsAddress];
        task_queue_push(ram, kTaskAddScore, points);
        task_queue_push(ram, kTaskFruitScoreSprite, (uint8_t)(points + 0x15));
        ram[kFruitPositionAddress] = 0;
        ram[kFruitPositionAddress + 1] = 0;
        schedule_timed_event(ram, kTimedEventFruitEaten[0], kTimedEventFruitEaten[1], kTimedEventFruitEaten[2]);
        ram[kFootstepSoundAddress] |= 4;
    }
    if (eat) pacman_eats_on_tile(ram, video); // 19CD, the T01c code
}

// ---- the ghosts: where each one keeps its things (index 0 Blinky, 1 Pinky, 2 Inky, 3 Clyde)
constexpr unsigned kGhostPixelAddress[4] = {0x4D00, 0x4D02, 0x4D04, 0x4D06};    // the ghost's pixel position
constexpr unsigned kGhostMoveTileAddress[4] = {0x4D31, 0x4D33, 0x4D35, 0x4D37}; // the tile it stands on, from its pixel position
constexpr unsigned kGhostStepAddress[4] = {0x4D14, 0x4D16, 0x4D18, 0x4D1A};     // the step it is taking now
constexpr unsigned kGhostActiveAddress[4] = {0x4DA0, 0x4DA1, 0x4DA2, 0x4DA3};   // Blinky: not 0; the others: exactly 1
constexpr unsigned kGhostBlockedAddress[4] = {0x4DAC, 0x4DAD, 0x4DAE, 0x4DAF};  // not 0: does not move
constexpr unsigned kGhostTunnelFlagAddress[4] = {0x4D99, 0x4D9A, 0x4D9B, 0x4D9C};
constexpr unsigned kGhostCurrentDirectionAddress[4] = {0x4D28, 0x4D29, 0x4D2A, 0x4D2B};
constexpr unsigned kSpeedNormalAddress[4] = {0x4D56, 0x4D62, 0x4D6E, 0x4D7A}; // high word of the 32-bit speed pattern (low word 2 bytes on)
constexpr unsigned kSpeedBlueAddress[4] = {0x4D5A, 0x4D66, 0x4D72, 0x4D7E};
constexpr unsigned kSpeedTunnelAddress[4] = {0x4D5E, 0x4D6A, 0x4D76, 0x4D82};
constexpr unsigned kSpeedElroy1Address = 0x4D52, kSpeedElroy2Address = 0x4D4E;
constexpr unsigned kElroy1Address = 0x4DB6, kElroy2Address = 0x4DB7, kElroyDotsLeft1Address = 0x4DBB, kElroyDotsLeft2Address = 0x4DBC;
constexpr unsigned kTurnRoundSettingAddress = 0x4E02;
constexpr uint8_t kColourGhostHouseDoor = 0x1A;
constexpr uint8_t kTaskGhostBlueTurn = 0x0C, kTaskGhostTurn = 0x08; // + ghost index

// ROM 20D7: Blinky becomes Cruise Elroy (faster) when few enough dots are left (the limits 4DBB and 4DBC), once Clyde has left the house.
inline void update_cruise_elroy(WorkRam ram) {
    if (ram[kGhostActiveAddress[3]] == 0) return;
    const uint8_t dots_left = (uint8_t)(0xF4 - ram[kDotsEatenAddress]);
    if (ram[kElroy1Address] == 0) {
        if (ram[kElroyDotsLeft1Address] < dots_left) return;
        ram[kElroy1Address] = 1;
    }
    if (ram[kElroy2Address] != 0) return;
    if (ram[kElroyDotsLeft2Address] < dots_left) return;
    ram[kElroy2Address] = 1;
}

// ROM 1EFE, 1F25, 1F4C, 1F73: a ghost has been told to turn round (reverse flag): the new direction is the opposite of the old one.
inline void apply_turn_round(WorkRam ram, unsigned g) {
    if (ram[kGhostReverseAddress[g]] == 0) return;
    ram[kGhostReverseAddress[g]] = 0;
    const uint8_t direction = (uint8_t)(ram[kGhostCurrentDirectionAddress[g]] ^ 2);
    ram[kGhostDirectionAddress[g]] = direction; // the "wanted" direction 4D2C+g
    const uint16_t step = step_word(direction & 3);
    set_word(ram, kGhostVectorAddress[g], step);
    if (ram[kTurnRoundSettingAddress] != 0x22) return;
    set_word(ram, kGhostStepAddress[g], step);
    ram[kGhostCurrentDirectionAddress[g]] = direction;
}

// One update of one ghost's movement (ROM 1B36 / 1C4B / 1D22 / 1DF9).
inline void ghost_move(WorkRam ram, Screen screen, unsigned g) {
    if (g == 0) {
        if (ram[kGhostActiveAddress[0]] == 0) return;
    } else if (ram[kGhostActiveAddress[g]] != 1) return;
    if (ram[kGhostBlockedAddress[g]] != 0) return;
    if (g == 0) update_cruise_elroy(ram);
    const uint16_t move_tile = word_at(ram, kGhostMoveTileAddress[g]);
    store_tunnel_flag(ram, screen, move_tile, kGhostTunnelFlagAddress[g]);
    // which speed pattern counts (the first that applies): in the tunnel, blue (edible), Cruise Elroy 2 and 1 (Blinky), normal
    unsigned pattern;
    if (ram[kGhostTunnelFlagAddress[g]]) pattern = kSpeedTunnelAddress[g];
    else if (ram[kGhostEdibleAddress[g]]) pattern = kSpeedBlueAddress[g];
    else if (g == 0 && ram[kElroy2Address]) pattern = kSpeedElroy2Address;
    else if (g == 0 && ram[kElroy1Address]) pattern = kSpeedElroy1Address;
    else pattern = kSpeedNormalAddress[g];
    if (!advance_speed_pattern(ram, pattern)) return;
    // 1BD8: in the middle of a tile (on the axis it moves along) the ghost may choose a new direction
    const unsigned pixel = kGhostPixelAddress[g], step = kGhostStepAddress[g];
    const bool at_middle = ram[step] != 0 ? (ram[pixel] & 7) == 4 : (ram[pixel + 1] & 7) == 4;
    if (at_middle) {
        if (!tile_outside_maze_rows(ram, 1 + g)) {
            if (ram[kGhostEdibleAddress[g]]) task_queue_push(ram, (uint8_t)(kTaskGhostBlueTurn + g), 0);
            else if (screen.read(colour_address_of_tile(word_at(ram, kGhostTileAddress[g]))) != kColourGhostHouseDoor)
                task_queue_push(ram, (uint8_t)(kTaskGhostTurn + g), 0);
        }
        apply_turn_round(ram, g);
        set_word(ram, kGhostTileAddress[g], add_step(ram, kGhostVectorAddress[g], kGhostTileAddress[g])); // the tile it will enter next
        set_word(ram, step, word_at(ram, kGhostVectorAddress[g]));
        ram[kGhostCurrentDirectionAddress[g]] = ram[kGhostDirectionAddress[g]];
    }
    set_word(ram, pixel, add_step(ram, step, pixel));
    set_word(ram, kGhostMoveTileAddress[g], tile_of_pixel(word_at(ram, pixel)));
}

} // namespace logic
