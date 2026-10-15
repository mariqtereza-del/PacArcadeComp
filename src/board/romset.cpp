// romset.cpp: loads and checks the player's own ROM zip (see romset.h). The ROM is never written anywhere.
#include "romset.h"
#include <cstdio>
#include <cstring>
#include "../third_party/miniz/miniz.h"

namespace {
struct Expect {
    const char *name;
    size_t size;
    uint32_t crc;
};
// Same table as tools/check_rom.py (MAME set "pacman").
const Expect TABLE[] = {
    {"pacman.6e", 4096, 0xc1e6ab10}, {"pacman.6f", 4096, 0x1a6fb2d4}, {"pacman.6h", 4096, 0xbcdd1beb}, {"pacman.6j", 4096, 0x817d94e3}, {"pacman.5e", 4096, 0x0c944964},
    {"pacman.5f", 4096, 0x958fedf9}, {"82s123.7f", 32, 0x2fc650bd},   {"82s126.4a", 256, 0x3eb3a8e4},  {"82s126.1m", 256, 0xa9cc86bf},  {"82s126.3m", 256, 0x77245b66},
};
std::string hex8(uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "%08x", (unsigned)v);
    return b;
}
} // namespace

bool load_romset_from_memory(const uint8_t *zip, size_t zip_size, RomSet &out, std::string &error) {
    out = RomSet();
    error.clear();
    mz_zip_archive za;
    std::memset(&za, 0, sizeof za);
    if (!zip || !mz_zip_reader_init_mem(&za, zip, zip_size, 0)) {
        error = "This is not a zip file (or it is damaged), so no ROMs can be read from it.";
        return false;
    }
    std::vector<uint8_t> data[10];
    std::string problems;
    for (int i = 0; i < 10; ++i) {
        const Expect &e = TABLE[i];
        int idx = mz_zip_reader_locate_file(&za, e.name, nullptr, 0);
        if (idx < 0) {
            problems += std::string("The file ") + e.name + " is missing from the zip.\n";
            continue;
        }
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&za, (mz_uint)idx, &st)) {
            problems += std::string("The file ") + e.name + " cannot be read from the zip.\n";
            continue;
        }
        if (st.m_uncomp_size != e.size) {
            problems += std::string("The file ") + e.name + " has the wrong size: " + std::to_string((unsigned long long)st.m_uncomp_size) + " bytes, expected " +
                        std::to_string(e.size) + ". This is not the standard Midway \"pacman\" set.\n";
            continue;
        }
        std::vector<uint8_t> buf(e.size);
        if (!mz_zip_reader_extract_to_mem(&za, (mz_uint)idx, buf.data(), buf.size(), 0)) {
            problems += std::string("The file ") + e.name + " is damaged inside the zip (it cannot be unpacked).\n";
            continue;
        }
        uint32_t crc = (uint32_t)mz_crc32(MZ_CRC32_INIT, buf.data(), buf.size());
        if (crc != e.crc) {
            problems += std::string("The file ") + e.name + " has the wrong checksum (CRC32 " + hex8(crc) + ", expected " + hex8(e.crc) +
                        "). It is damaged or from a different version of the game.\n";
            continue;
        }
        data[i] = std::move(buf);
    }
    mz_zip_reader_end(&za);
    if (!problems.empty()) {
        error = problems;
        return false;
    }
    for (int i = 0; i < 4; ++i) out.program.insert(out.program.end(), data[i].begin(), data[i].end());
    out.tiles = std::move(data[4]);
    out.sprites = std::move(data[5]);
    out.colours = std::move(data[6]);
    out.palettes = std::move(data[7]);
    out.sound1 = std::move(data[8]);
    out.sound3 = std::move(data[9]);
    return true;
}

bool load_romset_from_file(const std::string &path, RomSet &out, std::string &error) {
    out = RomSet();
    FILE *f = std::fopen(path.c_str(), "rb"); // read only
    if (!f) {
        error = "Cannot open the ROM zip \"" + path + "\". Put your own pacman.zip there.";
        return false;
    }
    std::vector<uint8_t> buf;
    uint8_t chunk[65536];
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0) {
        buf.insert(buf.end(), chunk, chunk + n);
        if (buf.size() > (64u << 20)) {
            std::fclose(f);
            error = "The file \"" + path + "\" is far too large to be the ROM zip.";
            return false;
        }
    }
    std::fclose(f);
    return load_romset_from_memory(buf.data(), buf.size(), out, error);
}
