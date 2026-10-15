// S20 test of src/video/tilemap.cpp. Usage: test_tilemap pacman.zip py_700.rgb py_1500.rgb [out_prefix]
// The .rgb files are the Python prototype's tile layer (tools/analysis/render.py render(mem, gfx, sprites=False), 224x288x3 bytes) at frames
// 700 and 1500 of the exact-frame, mirror-correct prototype board (tools/analysis/oracle_dump.py: MirrorBoard + run_frame_exact). The command
// that makes them is in docs/evidence/S20_tiles.txt. The C++ side runs src/board/machine.cpp for 1500 frames (RAM parity with that board was proved in S18).
// Build: g++ -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_tilemap.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp miniz*.o
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/board/machine.h"
#include "../src/video/gfx.h"

void render_tilemap(const Gfx &g, const uint8_t *videoram, const uint8_t *colorram, uint32_t *out);

static std::vector<uint8_t> read_file(const char *p) {
    std::vector<uint8_t> v;
    FILE *f = std::fopen(p, "rb");
    if (!f) return v;
    uint8_t b[65536];
    size_t n;
    while ((n = std::fread(b, 1, sizeof b, f)) > 0) v.insert(v.end(), b, b + n);
    std::fclose(f);
    return v;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::printf("usage: test_tilemap pacman.zip py_700.rgb py_1500.rgb [out_prefix]\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    static Gfx g;
    decode_gfx(roms, g);
    Machine m(roms.program);
    int failures = 0;
    for (int fr = 1; fr <= 1500; ++fr) {
        m.run_frame();
        if (fr != 700 && fr != 1500) continue;
        static uint32_t img[224 * 288];
        std::memset(img, 0xAB, sizeof img);
        render_tilemap(g, m.bus.videoram, m.bus.colorram, img);
        std::vector<uint8_t> ref = read_file(argv[fr == 700 ? 2 : 3]);
        size_t bad = 0, first = (size_t)-1, nonblack = 0, tiles = 0;
        if (ref.size() != 224 * 288 * 3) {
            std::printf("frame %d: reference has %zu bytes, expected %d\n", fr, ref.size(), 224 * 288 * 3);
            ++failures;
            continue;
        }
        for (size_t i = 0; i < 224 * 288; ++i) {
            uint32_t want = (uint32_t)ref[3 * i] << 16 | (uint32_t)ref[3 * i + 1] << 8 | ref[3 * i + 2];
            if (img[i] != want) {
                if (!bad++) first = i;
            }
            if (img[i] != 0) ++nonblack;
        }
        for (int i = 0; i < 0x400; ++i)
            if (m.bus.videoram[i] != 0x40) ++tiles; // 0x40 = blank tile
        std::printf("frame %d: %zu of %d pixels differ%s; %zu non-black pixels, %zu non-blank tiles\n", fr, bad, 224 * 288,
                    bad ? (" (first at x=" + std::to_string(first % 224) + ", y=" + std::to_string(first / 224) + ")").c_str() : "", nonblack, tiles);
        if (bad || nonblack < 500) ++failures;
        if (argc > 4) {
            std::string fn = std::string(argv[4]) + std::to_string(fr) + ".ppm";
            FILE *o = std::fopen(fn.c_str(), "wb");
            if (o) {
                std::fprintf(o, "P6\n224 288\n255\n");
                for (size_t i = 0; i < 224 * 288; ++i) {
                    uint8_t px[3] = {(uint8_t)(img[i] >> 16), (uint8_t)(img[i] >> 8), (uint8_t)img[i]};
                    std::fwrite(px, 1, 3, o);
                }
                std::fclose(o);
            }
        }
    }
    std::printf("%s test_tilemap\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
