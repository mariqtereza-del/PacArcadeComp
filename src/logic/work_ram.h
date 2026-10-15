// T01b: a tiny view of the machine's work RAM (addresses 4C00-4FEF) so that the readable rules can use the same addresses as the original program
// and the docs/notes/ram_map.md table. The caller owns the bytes (the real machine's, or a copy for a test).
#pragma once
#include <cstdint>

namespace logic {

constexpr unsigned kWorkRamStart = 0x4C00;
constexpr unsigned kWorkRamSize = 0x3F0;

struct WorkRam {
    uint8_t *bytes; // kWorkRamSize bytes, the first one is the byte at address 4C00
    uint8_t &operator[](unsigned address) const { return bytes[address - kWorkRamStart]; }
};

// The video RAM (4000-43FF): one byte per character tile of the screen, which is also where the game keeps the map of dots still to be eaten.
constexpr unsigned kVideoRamStart = 0x4000;
constexpr unsigned kVideoRamSize = 0x400;

struct VideoRam {
    uint8_t *bytes;            // kVideoRamSize bytes, the first one is the byte at address 4000
    uint8_t *colour = nullptr; // optional (T01e): the colour RAM, 0x400 bytes at 4400-47FF, which a tile address can run into
    // Reading outside 4000-43FF gives the colour RAM if it was handed in (4400-47FF), 0xBF for 4800-4BFF (nothing there), else 0; writing there does nothing.
    uint8_t &operator[](unsigned address) const {
        static uint8_t dummy;
        if (address >= kVideoRamStart && address < kVideoRamStart + kVideoRamSize) return bytes[address - kVideoRamStart];
        if (colour && address >= 0x4400 && address < 0x4800) return colour[address - 0x4400];
        dummy = (address >= 0x4800 && address < 0x4C00) ? 0xBF : 0;
        return dummy;
    }
};

} // namespace logic
