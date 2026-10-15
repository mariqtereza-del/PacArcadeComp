// T01a: the arcade game's random number generator, written as readable C++.
//
// What the ROM does (routine at 2A23, called from only one place, 2926, to choose a random direction for a frightened ghost):
//     HL = the 16-bit seed stored in RAM at 4DC9 (low byte) and 4DCA (high byte)
//     HL = HL * 5 + 1                      (the ROM computes it as HL + HL + HL + HL + HL, then adds 1)
//     keep only the lower 13 bits of HL    (the ROM clears the top three bits of H, so HL is an address inside the 8 KB 0000-1FFF)
//     the answer is the byte of the program ROM at address HL; HL is stored back as the new seed
//
// So the "random" number is a byte read from the game's own program ROM at a position that jumps around in a fixed pattern. A seed of
// x * 5 + 1 modulo 8192 visits all 8192 positions before it repeats, so the sequence has a period of 8192 steps; the program starts a
// game with whatever seed is left in RAM, so a player cannot predict it but a replay can reproduce it.
#pragma once
#include <cstdint>

namespace logic {

constexpr unsigned kRandomSeedAddress = 0x4DC9; // RAM: low byte at 4DC9, high byte at 4DCA
constexpr unsigned kRandomSeedMask = 0x1FFF;    // the seed always stays inside 0000-1FFF
constexpr unsigned kRandomMultiplier = 5;
constexpr unsigned kRandomIncrement = 1;

// The step: the new seed, from the old one.
inline uint16_t next_random_seed(uint16_t seed) { return (uint16_t)((seed * kRandomMultiplier + kRandomIncrement) & kRandomSeedMask); }

// The whole routine. `seed` is updated in place (it is the two RAM bytes at 4DC9/4DCA); `program_rom` points to the first of the 16,384
// bytes of the program ROM (the caller supplies it). Returns the random byte.
inline uint8_t get_random_byte(uint16_t &seed, const uint8_t *program_rom) {
    seed = next_random_seed(seed);
    return program_rom[seed];
}

} // namespace logic
