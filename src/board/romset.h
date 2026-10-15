// S16: load the player's own Pac-Man ROM zip into memory and verify it. The ROM is never written anywhere.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct RomSet {
    std::vector<uint8_t> program;  // pacman.6e + 6f + 6h + 6j = 16384 bytes at CPU address 0000
    std::vector<uint8_t> tiles;    // pacman.5e, 4096 bytes
    std::vector<uint8_t> sprites;  // pacman.5f, 4096 bytes
    std::vector<uint8_t> colours;  // 82s123.7f, 32 bytes (colour PROM)
    std::vector<uint8_t> palettes; // 82s126.4a, 256 bytes (palette look-up PROM)
    std::vector<uint8_t> sound1;   // 82s126.1m, 256 bytes (waveforms)
    std::vector<uint8_t> sound3;   // 82s126.3m, 256 bytes (timing PROM, not used by the sound hardware model)
};

// Returns true and fills `out` when all 10 files are present with the expected size and CRC32 (other files in the zip are ignored).
// Otherwise returns false, leaves `out` empty and puts one plain-language line per problem into `error`.
bool load_romset_from_memory(const uint8_t *zip, size_t zip_size, RomSet &out, std::string &error);
bool load_romset_from_file(const std::string &path, RomSet &out, std::string &error); // only reads the file
