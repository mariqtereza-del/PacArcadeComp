// S19 test of src/video/gfx.cpp. Usage: test_gfx PATH_TO_pacman.zip PATH_TO_gfx_dump.bin
// The dump is made from the Python prototype's decoders (tools/analysis/render.py: tile_pixels, sprite_pixels, palette) with
//   python3 -c "import sys;sys.path.insert(0,'tools/analysis');from harness import load_roms;from render import *;p,g=load_roms();o=bytearray()
//   (continued, one line) ;[o.extend(v for r in tile_pixels(g['pacman.5e'],c) for v in r) for c in range(256)]
//   (continued) ;[o.extend(v for r in sprite_pixels(g['pacman.5f'],c) for v in r) for c in range(64)]
//   (continued) ;[o.extend(rgb) for pal in palette(g) for rgb in pal];open('build/s19/gfx_dump.bin','wb').write(o)"
// Layout of the dump: 256 tiles x 64 bytes (row by row, values 0-3), 64 sprites x 256 bytes, 64 palettes x 4 entries x 3 bytes (R,G,B) = 33,536 bytes.
// The dump holds ROM graphics, so it is kept in build/ (never in a checkpoint or release), not in tests/data.
// Build: g++ -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_gfx.cpp src/video/gfx.cpp src/board/romset.cpp miniz*.o
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/video/gfx.h"

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

// Independent decoder written from the strip description (Lomont guide p6-8), not from the MAME offset tables:
// a strip is 8 bytes = 8 rows of 4 pixels; in each byte the high nibble holds the high bit and the low nibble the low bit of the 4 pixels, leftmost pixel = bit 3 / bit 7.
static int strip_px(const uint8_t *s8, int y, int k) { return ((s8[y] >> (7 - k)) & 1) * 2 + ((s8[y] >> (3 - k)) & 1); }

int main(int argc, char **argv) {
    if (argc < 3) {
        std::printf("usage: test_gfx pacman.zip gfx_dump.bin\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    static Gfx g;
    expect(decode_gfx(roms, g), "decode_gfx succeeds on the verified set");

    // ---- 1. byte compare with the prototype's decoders
    FILE *f = std::fopen(argv[2], "rb");
    std::vector<uint8_t> d;
    if (f) {
        uint8_t b[65536];
        size_t n;
        while ((n = std::fread(b, 1, sizeof b, f)) > 0) d.insert(d.end(), b, b + n);
        std::fclose(f);
    }
    expect(d.size() == 256 * 64 + 64 * 256 + 64 * 4 * 3, "dump has 33,536 bytes");
    if (d.size() == 33536) {
        size_t bad_t = 0, bad_s = 0, bad_p = 0;
        int first_t = -1, first_s = -1, first_p = -1;
        for (int c = 0; c < 256; ++c)
            for (int y = 0; y < 8; ++y)
                for (int x = 0; x < 8; ++x)
                    if (g.tiles[c][y][x] != d[c * 64 + y * 8 + x]) {
                        if (!bad_t++) first_t = c;
                    }
        for (int c = 0; c < 64; ++c)
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    if (g.sprites[c][y][x] != d[16384 + c * 256 + y * 16 + x]) {
                        if (!bad_s++) first_s = c;
                    }
        for (int p = 0; p < 64; ++p)
            for (int i = 0; i < 4; ++i)
                for (int k = 0; k < 3; ++k)
                    if (g.palettes[p][i][k] != d[16384 + 16384 + (p * 4 + i) * 3 + k]) {
                        if (!bad_p++) first_p = p;
                    }
        std::printf("tiles: %zu differing pixels (of 16384, first bad tile %d); sprites: %zu (of 16384, first bad sprite %d); palettes: %zu bytes (of 768, first bad "
                    "palette %d)\n",
                    bad_t, first_t, bad_s, first_s, bad_p, first_p);
        expect(bad_t == 0, "256 tiles identical to the prototype");
        expect(bad_s == 0, "64 sprites identical to the prototype");
        expect(bad_p == 0, "64 palettes x 4 colours identical to the prototype");
    }

    // ---- 2. independent check of the Lomont strip order 5,1,6,2,7,3,4,0 (p8)
    // Reading: the eight 8-byte strips of a sprite sit in the order bottom,top for each 4-pixel-wide column from the left.
    static const int L[8] = {5, 1, 6, 2, 7, 3, 4, 0};
    int bad = 0, nonblank = 0;
    for (int c = 0; c < 64; ++c) {
        const uint8_t *s = &roms.sprites[c * 64];
        bool any = false;
        for (int col = 0; col < 4; ++col)
            for (int y = 0; y < 16; ++y)
                for (int k = 0; k < 4; ++k) {
                    int strip = y < 8 ? L[2 * col + 1] : L[2 * col]; // top row of strips = odd entries, bottom row = even entries
                    int v = strip_px(s + strip * 8, y & 7, k);
                    if (v) any = true;
                    if (v != g.sprites[c][y][col * 4 + k]) ++bad;
                }
        if (any) ++nonblank;
    }
    std::printf("Lomont strip order: %d differing pixels over 64 sprites (%d sprites have pixels)\n", bad, nonblank);
    expect(bad == 0, "sprites rebuilt from strips in Lomont's order equal the decoded sprites");
    expect(nonblank >= 40, "most sprites are not blank, so the comparison is meaningful");
    // the check has teeth: other orders must fail
    static const int WRONG[2][8] = {{0, 1, 2, 3, 4, 5, 6, 7}, {1, 5, 2, 6, 3, 7, 0, 4}};
    for (int w = 0; w < 2; ++w) {
        int wb = 0;
        for (int c = 0; c < 64; ++c) {
            const uint8_t *s = &roms.sprites[c * 64];
            for (int col = 0; col < 4; ++col)
                for (int y = 0; y < 16; ++y)
                    for (int k = 0; k < 4; ++k) {
                        int strip = y < 8 ? WRONG[w][2 * col + 1] : WRONG[w][2 * col];
                        if (strip_px(s + strip * 8, y & 7, k) != g.sprites[c][y][col * 4 + k]) ++wb;
                    }
        }
        std::printf("  control: strip order %d,%d,%d,%d,%d,%d,%d,%d gives %d differing pixels\n", WRONG[w][0], WRONG[w][1], WRONG[w][2], WRONG[w][3], WRONG[w][4],
                    WRONG[w][5], WRONG[w][6], WRONG[w][7], wb);
        expect(wb > 1000, "a wrong strip order is detected");
    }
    // tiles are two strips: x 0-3 from bytes 8-15, x 4-7 from bytes 0-7
    int tb = 0;
    for (int c = 0; c < 256; ++c)
        for (int y = 0; y < 8; ++y)
            for (int x = 0; x < 8; ++x) {
                const uint8_t *t = &roms.tiles[c * 16];
                int v = x < 4 ? strip_px(t + 8, y, x) : strip_px(t, y, x - 4);
                if (v != g.tiles[c][y][x]) ++tb;
            }
    std::printf("tiles rebuilt from two strips: %d differing pixels\n", tb);
    expect(tb == 0, "tiles equal the two-strip construction");

    // ---- 3. sanity of the colour tables
    expect(g.palettes[0][0][0] == 0 && g.palettes[0][0][1] == 0 && g.palettes[0][0][2] == 0, "palette 0 is black");
    int distinct = 0;
    for (int i = 0; i < 32; ++i) {
        bool dup = false;
        for (int j = 0; j < i; ++j)
            if (!std::memcmp(g.colours[i], g.colours[j], 3)) dup = true;
        if (!dup) ++distinct;
    }
    std::printf("colour PROM: %d distinct colours of 32\n", distinct);
    expect(distinct >= 8, "colour PROM decodes to several colours");

    std::printf("%s: %d checks, %d failures\n", failures ? "FAIL" : "PASS test_gfx", checks, failures);
    return failures ? 1 : 0;
}
