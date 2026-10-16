// T01b: scoring, the extra life and the high score, written as readable C++.
//
// The ROM routine at 2A5A ("add_score") is called with the number of the points in register B. In order it:
//   1. does nothing at all while the demo is playing (game state 4E00 = 1);
//   2. looks up the points for B in a table of 14 two-byte numbers at 2B17 (10, 50, 200, 400, 800, 1600, 100, 300, 500, 700, 1000, 2000, 3000, 5000);
//   3. adds them to the score of the player whose turn it is (4E09: 0 = player 1, 1 = player 2);
//   4. gives an extra life when the score has reached the limit the DIP switches chose (4E71), once per game and player;
//   5. redraws the score (video RAM; left to the ROM here: this file is rules only);
//   6. copies the score to the high score when it is higher, and redraws that too (not done here either).
//
// How a score is stored: three bytes, lowest first, each byte holds TWO DECIMAL DIGITS (binary coded decimal, "BCD": the byte 0x47 means 47).
// Player 1: 4E80 4E81 4E82, then a flag byte 4E83 whose bit 0 says "the extra life was already given". Player 2: 4E84-4E86 and 4E87.
// The high score: 4E88-4E8A. Because the printed score always ends in 0 for the dots, the last two stored digits are the tens and units: 10 points is 0x10 in byte 4E80.
#pragma once
#include <cstdint>
#include "work_ram.h"

namespace logic {

// ---- where things live (docs/notes/ram_map.md)
constexpr unsigned kGameStateAddress = 0x4E00;      // 1 = demo (attract mode), 3 = a game is being played
constexpr unsigned kCurrentPlayerAddress = 0x4E09;  // 0 = player 1, 1 = player 2
constexpr unsigned kLivesAddress = 0x4E14;          // lives left
constexpr unsigned kLivesShownAddress = 0x4E15;     // lives shown on the screen (the extra life raises both)
constexpr unsigned kBonusLifeLimitAddress = 0x4E71; // the score in thousands at which the extra life comes, BCD: 0x10, 0x15, 0x20, or 0xFF = never
constexpr unsigned kHighScoreAddress = 0x4E88;
constexpr unsigned kSoundRequestAddress = 0x4E9C; // bit 0 asks the sound chip for the extra life jingle
constexpr unsigned kScoreAddress[2] = {0x4E80, 0x4E84};
constexpr unsigned kScoreBytes = 3;
constexpr unsigned kExtraLifeGivenBit = 0x01; // in the byte right after each score

constexpr uint8_t kStateDemo = 1;

// ---- what each kind of score is worth (the numbers B stands for in the ROM)
enum PointsIndex {
    kDot = 0,
    kEnergizer,
    kGhost1,
    kGhost2,
    kGhost3,
    kGhost4, // 10, 50, then the four ghosts of one energizer: 200, 400, 800, 1600
    kCherry,
    kStrawberry,
    kOrange,
    kApple,
    kMelon,
    kGalaxian,
    kBell,
    kKey, // 100, 300, 500, 700, 1000, 2000, 3000, 5000
    kPointsKinds
};
constexpr unsigned kPointsValue[kPointsKinds] = {10, 50, 200, 400, 800, 1600, 100, 300, 500, 700, 1000, 2000, 3000, 5000};

// ---- BCD helpers
// Two decimal digits (0..99) as one BCD byte.
inline uint8_t to_bcd(unsigned two_digits) { return (uint8_t)(((two_digits / 10) << 4) | (two_digits % 10)); }

// Adds two BCD bytes and a carry; `carry` is updated (the Z80's ADD/ADC followed by DAA, for valid BCD digits).
inline uint8_t add_bcd_byte(uint8_t a, uint8_t b, bool &carry) {
    unsigned low = (a & 0x0F) + (b & 0x0F) + (carry ? 1 : 0);
    unsigned high = (a >> 4) + (b >> 4);
    if (low > 9) {
        low -= 10;
        high += 1;
    }
    carry = high > 9;
    if (carry) high -= 10;
    return (uint8_t)((high << 4) | low);
}

// The extra life (ROM 2B33). `flags` is the byte after the player's score. Returns true when a life was actually given now.
inline bool give_extra_life_once(WorkRam ram, unsigned flags_address) {
    if (ram[flags_address] & kExtraLifeGivenBit) return false; // already given in this game
    ram[flags_address] |= kExtraLifeGivenBit;
    ram[kSoundRequestAddress] |= 0x01; // the jingle
    ram[kLivesAddress] = (uint8_t)(ram[kLivesAddress] + 1);
    ram[kLivesShownAddress] = (uint8_t)(ram[kLivesShownAddress] + 1);
    // (the ROM also redraws the little Pac-Man pictures at the bottom of the screen: video RAM, not done here)
    return true;
}

// What the points did besides raising the score (T02b: the game needs to know, to carry on with the ROM's drawing).
enum ExtraLife { kNoExtraLife, kExtraLifeAlreadyGiven, kExtraLifeGivenNow };

// Steps 1 and 2 of the routine: the points are added to the current player's score and the extra life is given when the limit is reached.
// (The demo check is the caller's.) Returns what happened with the extra life.
inline ExtraLife add_points(WorkRam ram, unsigned points_index) {
    const unsigned score = kScoreAddress[ram[kCurrentPlayerAddress] != 0];

    // 1. add the points: the table value as BCD, low byte first, carried into the third byte
    const unsigned points = kPointsValue[points_index];
    const uint8_t add_low = to_bcd(points % 100), add_mid = to_bcd((points / 100) % 100);
    bool carry = false;
    ram[score + 0] = add_bcd_byte(ram[score + 0], add_low, carry);
    ram[score + 1] = add_bcd_byte(ram[score + 1], add_mid, carry);
    ram[score + 2] = add_bcd_byte(ram[score + 2], 0x00, carry); // (a carry out of the third byte, one million points, is lost)

    // 2. extra life: the score has reached the limit when its ten-thousands and thousands digits (the 4th and 5th digit from the right,
    //    as one BCD byte) are at least the limit. The ROM tests "limit - 1 < digits" with ordinary byte arithmetic, which is the same thing
    //    and also makes the limit 0xFF ("never") unreachable, because no BCD pair is 0xFF.
    const uint8_t ten_thousands_and_thousands = (uint8_t)((ram[score + 2] << 4) | (ram[score + 1] >> 4));
    if ((uint8_t)(ram[kBonusLifeLimitAddress] - 1) < ten_thousands_and_thousands)
        return give_extra_life_once(ram, score + kScoreBytes) ? kExtraLifeGivenNow : kExtraLifeAlreadyGiven;
    return kNoExtraLife;
}

// Step 3: the high score. The score of the player whose turn it is is compared with the high score from the top byte down; equal scores do nothing.
// `bytes_equal` (if given) is how many top bytes were equal before the bytes that decided (0, 1 or 2; 3 = the scores are equal).
// Returns true when the high score was raised (copied).
inline bool update_high_score(WorkRam ram, unsigned *bytes_equal = nullptr) {
    const unsigned score = kScoreAddress[ram[kCurrentPlayerAddress] != 0];
    for (int i = (int)kScoreBytes - 1; i >= 0; --i) {
        const uint8_t mine = ram[score + i], best = ram[kHighScoreAddress + i];
        if (bytes_equal) *bytes_equal = (unsigned)((int)kScoreBytes - 1 - i);
        if (mine < best) return false;
        if (mine > best) {
            for (unsigned k = 0; k < kScoreBytes; ++k) ram[kHighScoreAddress + k] = ram[score + k];
            return true;
        }
    }
    if (bytes_equal) *bytes_equal = kScoreBytes;
    return false;
}

// The whole routine for one score event.
inline void add_score(WorkRam ram, unsigned points_index) {
    if (ram[kGameStateAddress] == kStateDemo) return; // nobody scores in the demo
    add_points(ram, points_index);
    update_high_score(ram);
}

} // namespace logic
