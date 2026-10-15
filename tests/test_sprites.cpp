// S21 test of src/video/sprites.cpp. Usage: test_sprites pacman.zip py_700.rgb py_1500.rgb py_2000.rgb [out_prefix]
// Part 1: frames 700, 1500 (attract) and 2000 (demo game): tile layer (S20) + sprites in prototype-compatible mode must equal the prototype's
//   render(mem, gfx, sprites=True, variant='A') pixel for pixel. The .rgb files come from the exact-frame, mirror-correct prototype board (see docs/evidence/S21_sprites.txt).
// Part 2: the same frames in MAME mode (hack, clip, wrap-around, look-up PROM transparency): the pictures are saved and the difference to part 1 is counted.
// Part 3: synthetic sprite registers: flip X, flip Y, both, priority, transparency, xoffsethack, clip, wrap-around.
// Build: g++ -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_sprites.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp miniz*.o
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "../src/board/machine.h"
#include "../src/video/gfx.h"

void render_tilemap(const Gfx &g, const uint8_t *videoram, const uint8_t *colorram, uint32_t *out);
void render_sprites(const Gfx &g, const uint8_t *lookup, const uint8_t *spriteram, const uint8_t *coords, uint32_t *out, bool mame_rules);

static int failures = 0, checks = 0;
static void expect(bool ok, const std::string &what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}
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
static void save_ppm(const std::string &fn, const uint32_t *img) {
    FILE *o = std::fopen(fn.c_str(), "wb");
    if (!o) return;
    std::fprintf(o, "P6\n224 288\n255\n");
    for (size_t i = 0; i < 224 * 288; ++i) {
        uint8_t px[3] = {(uint8_t)(img[i] >> 16), (uint8_t)(img[i] >> 8), (uint8_t)img[i]};
        std::fwrite(px, 1, 3, o);
    }
    std::fclose(o);
}
static uint32_t rgb(const Gfx &g, int pal, int v) {
    const uint8_t *c = g.palettes[pal][v];
    return (uint32_t)c[0] << 16 | (uint32_t)c[1] << 8 | c[2];
}
static uint32_t &npx(uint32_t *img, int X, int Y) { return img[X * 224 + (223 - Y)]; } // native pixel (X,Y) of the 288x224 picture

struct Regs {
    uint8_t spriteram[16], coords[16];
    Regs() {
        std::memset(spriteram, 0, 16);
        std::memset(coords, 0, 16);
        for (int n = 0; n < 8; ++n) coords[2 * n] = 0;
    }                                                            // y byte 0 -> sy = -31: fully above the picture
    void put(int n, int code, int flip, int pal, int X, int Y) { // X, Y = wanted native top-left (before the hack)
        spriteram[2 * n] = (uint8_t)(code << 2 | flip);
        spriteram[2 * n + 1] = (uint8_t)pal;
        coords[2 * n + 1] = (uint8_t)(272 - X);
        coords[2 * n] = (uint8_t)(Y + 31);
    }
};

int main(int argc, char **argv) {
    if (argc < 5) {
        std::printf("usage: test_sprites pacman.zip py_700.rgb py_1500.rgb py_2000.rgb [out_prefix]\n");
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
    const uint8_t *lookup = roms.palettes.data();

    // ---------------- Parts 1 and 2: real frames
    Machine m(roms.program);
    for (int fr = 1; fr <= 2000; ++fr) {
        m.run_frame();
        if (fr != 700 && fr != 1500 && fr != 2000) continue;
        static uint32_t compat[224 * 288], mame[224 * 288];
        render_tilemap(g, m.bus.videoram, m.bus.colorram, compat);
        std::memcpy(mame, compat, sizeof compat);
        render_sprites(g, lookup, m.bus.spriteram, m.bus.sprite_coords, compat, false);
        render_sprites(g, lookup, m.bus.spriteram, m.bus.sprite_coords, mame, true);
        std::vector<uint8_t> ref = read_file(argv[fr == 700 ? 2 : fr == 1500 ? 3 : 4]);
        size_t bad = 0, first = (size_t)-1, diff_mame = 0, sprite_pixels = 0;
        if (ref.size() != 224 * 288 * 3) {
            std::printf("frame %d: bad reference size %zu\n", fr, ref.size());
            ++failures;
            continue;
        }
        static uint32_t tiles_only[224 * 288];
        render_tilemap(g, m.bus.videoram, m.bus.colorram, tiles_only);
        for (size_t i = 0; i < 224 * 288; ++i) {
            uint32_t want = (uint32_t)ref[3 * i] << 16 | (uint32_t)ref[3 * i + 1] << 8 | ref[3 * i + 2];
            if (compat[i] != want) {
                if (!bad++) first = i;
            }
            if (compat[i] != mame[i]) ++diff_mame;
            if (compat[i] != tiles_only[i]) ++sprite_pixels;
        }
        bool pal_ok = true;
        for (int n = 0; n < 8; ++n)
            if (m.bus.spriteram[2 * n + 1] >= 0x20) pal_ok = false;
        std::printf("frame %d: prototype-compatible render: %zu of %d pixels differ%s; sprites change %zu pixels; MAME-rules render differs from it in %zu pixels\n", fr,
                    bad, 224 * 288, bad ? (" (first at x=" + std::to_string(first % 224) + ", y=" + std::to_string(first / 224) + ")").c_str() : "", sprite_pixels,
                    diff_mame);
        expect(bad == 0, "frame " + std::to_string(fr) + " equals the prototype");
        if (fr != 700)
            expect(sprite_pixels > 0,
                   "frame " + std::to_string(fr) + ": sprites are visible"); // the frame-700 attract screen shows its ghosts as tiles; no sprite is on screen
        expect(pal_ok, "frame " + std::to_string(fr) + ": sprite palette bytes below 0x20 (prototype mask 0x3F equals hardware mask 0x1F)");
        if (argc > 5) {
            save_ppm(std::string(argv[5]) + "compat_" + std::to_string(fr) + ".ppm", compat);
            save_ppm(std::string(argv[5]) + "mame_" + std::to_string(fr) + ".ppm", mame);
        }
    }

    // ---------------- Part 3: synthetic registers
    // an asymmetric sprite, and palettes whose entries 1-3 are all non-zero in the look-up PROM
    int code = -1;
    for (int c = 0; c < 64 && code < 0; ++c) {
        bool sx = false, sy = false;
        int left = 0, right = 0, bottom = 0, edge = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                if (g.sprites[c][y][x] != g.sprites[c][y][15 - x]) sx = true;
                if (g.sprites[c][y][x] != g.sprites[c][15 - y][x]) sy = true;
                if (g.sprites[c][y][x]) {
                    if (x < 2) ++edge;
                    if (x < 8) ++left;
                    if (x >= 8) ++right;
                    if (y >= 8) ++bottom;
                }
            }
        if (sx && sy && left > 30 && right > 30 && bottom > 30 && edge > 6) code = c;
    } // asymmetric, with plenty of pixels in the left, right and bottom parts
    expect(code >= 0, "found an asymmetric sprite for the flip tests");
    int palA = -1, palB = -1;
    for (int p = 1; p < 32; ++p) {
        bool ok = true;
        for (int v = 1; v < 4; ++v)
            if ((lookup[p * 4 + v] & 15) == 0) ok = false;
        if (!ok) continue;
        if (palA < 0) palA = p;
        else if (palB < 0 && std::memcmp(g.palettes[p][1], g.palettes[palA][1], 3)) palB = p;
    }
    expect(palA > 0 && palB > 0, "found two palettes with different colours and no zero entries");
    static uint32_t img[224 * 288];
    const uint32_t BG = 0x123456;
    auto clear = [&]() {
        for (auto &p : img) p = BG;
    };

    for (int flip = 0; flip < 4; ++flip) { // flip bit 0 = X, bit 1 = Y
        Regs r;
        r.put(3, code, flip, palA, 100, 100);
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, true);
        int bad = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                int v = g.sprites[code][(flip & 2) ? 15 - y : y][(flip & 1) ? 15 - x : x];
                uint32_t want = v ? rgb(g, palA, v) : BG;
                if (npx(img, 100 + x, 100 + y) != want) ++bad;
            }
        expect(bad == 0, std::string("flip ") + (flip & 1 ? "X " : "") + (flip & 2 ? "Y " : "") + (flip ? "" : "none ") + "draws the mirrored sprite exactly");
    }
    { // flips really differ from each other
        Regs a, b;
        a.put(3, code, 0, palA, 100, 100);
        b.put(3, code, 1, palA, 100, 100);
        static uint32_t i1[224 * 288], i2[224 * 288];
        for (auto &p : i1) p = BG;
        for (auto &p : i2) p = BG;
        render_sprites(g, lookup, a.spriteram, a.coords, i1, true);
        render_sprites(g, lookup, b.spriteram, b.coords, i2, true);
        expect(std::memcmp(i1, i2, sizeof i1) != 0, "flip X changes the picture");
    }
    { // priority: lower sprite number on top
        Regs r;
        r.put(5, code, 0, palB, 100, 100);
        r.put(3, code, 0, palA, 100, 100);
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, true);
        int bad = 0, overlap = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                int v = g.sprites[code][y][x];
                if (!v) continue;
                ++overlap;
                if (npx(img, 100 + x, 100 + y) != rgb(g, palA, v)) ++bad;
            }
        expect(bad == 0 && overlap > 50, "sprite 3 is drawn over sprite 5 (lower number on top)");
        Regs s;
        s.put(3, code, 0, palA, 100, 100);
        s.put(5, code, 0, palB, 100, 100);
        clear();
        render_sprites(g, lookup, s.spriteram, s.coords, img, true);
        int bad2 = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                int v = g.sprites[code][y][x];
                if (v && npx(img, 100 + x, 100 + y) != rgb(g, palA, v)) ++bad2;
            }
        expect(bad2 == 0, "register order does not matter, only the sprite number");
    }
    { // transparency by the look-up PROM: find a palette where a non-zero pixel value maps to colour 0
        int tp = -1, tv = -1;
        for (int p = 0; p < 32 && tp < 0; ++p)
            for (int v = 1; v < 4; ++v)
                if ((lookup[p * 4 + v] & 15) == 0 && tp < 0) {
                    tp = p;
                    tv = v;
                }
        expect(tp >= 0, "a palette with a transparent non-zero pixel value exists (palette 14 in this ROM)");
        if (tp >= 0) {
            int c2 = -1;
            for (int c = 0; c < 64 && c2 < 0; ++c)
                for (int y = 0; y < 16; ++y)
                    for (int x = 0; x < 16; ++x)
                        if (g.sprites[c][y][x] == tv) c2 = c;
            Regs r;
            r.put(3, c2, 0, tp, 100, 100);
            clear();
            render_sprites(g, lookup, r.spriteram, r.coords, img, true);
            int hid = 0, seen = 0;
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    if (g.sprites[c2][y][x] == tv) {
                        ++seen;
                        if (npx(img, 100 + x, 100 + y) == BG) ++hid;
                    }
            expect(seen > 0 && hid == seen, "MAME rules: pixels whose PROM colour is 0 are transparent");
            clear();
            render_sprites(g, lookup, r.spriteram, r.coords, img, false);
            int shown = 0;
            for (int y = 0; y < 16; ++y)
                for (int x = 0; x < 16; ++x)
                    if (g.sprites[c2][y][x] == tv && npx(img, 100 + x, 100 + y) != BG) ++shown;
            expect(shown == seen, "prototype-compatible rules draw them (only pixel value 0 is transparent)");
        }
    }
    { // xoffsethack: sprites 0-2 one pixel further along native y
        Regs r;
        r.put(0, code, 0, palA, 100, 100);
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, true);
        Regs q;
        q.put(3, code, 0, palA, 100, 101);
        static uint32_t ref2[224 * 288];
        for (auto &p : ref2) p = BG;
        render_sprites(g, lookup, q.spriteram, q.coords, ref2, true);
        expect(std::memcmp(img, ref2, sizeof img) == 0, "sprite 0 is drawn exactly like sprite 3 placed one pixel further in native y");
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, false);
        Regs q2;
        q2.put(3, code, 0, palA, 100, 100);
        for (auto &p : ref2) p = BG;
        render_sprites(g, lookup, q2.spriteram, q2.coords, ref2, false);
        expect(std::memcmp(img, ref2, sizeof img) == 0, "prototype-compatible rules have no offset");
    }
    { // clip: native x 16..271, y 0..223
        // The x register byte is 272 - sx, so sx is at least 17: a primary copy never reaches x < 16; the clip shows on the wrap-around copy (sx - 256).
        Regs r;
        r.put(3, code, 0, palA, 270, 100);
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, true);
        int clipped = 0, bad = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 2; ++x) {
                if (g.sprites[code][y][x]) ++clipped;
                if (npx(img, 270 - 256 + x, 100 + y) != BG) ++bad;
            }
        expect(bad == 0 && clipped > 4, "left clip: the wrap-around copy draws nothing at native x < 16 (x = 14 and 15)");
        Regs t;
        t.put(3, code, 0, palA, 265, 100);
        clear();
        render_sprites(g, lookup, t.spriteram, t.coords, img, true);
        int bad3 = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                int v = g.sprites[code][y][x];
                int X = 265 + x;
                uint32_t want = (v && X <= 271) ? rgb(g, palA, v) : BG;
                if (X < 288 && npx(img, X, 100 + y) != want) ++bad3;
            }
        expect(bad3 == 0, "right clip: nothing is drawn at native x > 271");
        clear();
        render_sprites(g, lookup, t.spriteram, t.coords, img, false);
        int shown = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 7; x < 16; ++x)
                if (g.sprites[code][y][x] && npx(img, 265 + x, 100 + y) != BG) ++shown;
        expect(shown > 0, "prototype-compatible rules draw beyond x = 271");
        Regs u;
        u.put(3, code, 0, palA, 100, 215);
        clear();
        render_sprites(g, lookup, u.spriteram, u.coords, img, true); // rows 215..230: bottom rows cut at 223 without crashing
        int bad4 = 0;
        for (int y = 0; y < 9; ++y)
            for (int x = 0; x < 16; ++x) {
                int v = g.sprites[code][y][x];
                if (npx(img, 100 + x, 215 + y) != (v ? rgb(g, palA, v) : BG)) ++bad4;
            }
        expect(bad4 == 0, "bottom edge: rows beyond y = 223 are cut off");
    }
    { // wrap-around: second copy at sx - 256
        Regs r;
        r.put(3, code, 0, palA, 270, 100);
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, true);
        int bad = 0, seen = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 2; x < 16; ++x) {
                int v = g.sprites[code][y][x];
                int X = 270 - 256 + x;
                if (v) ++seen;
                if (npx(img, X, 100 + y) != (v ? rgb(g, palA, v) : BG)) ++bad;
            }
        expect(bad == 0 && seen > 30, "a sprite at native x 270 also shows its right part at x 16..27 (wrap-around copy)");
        clear();
        render_sprites(g, lookup, r.spriteram, r.coords, img, false);
        int any = 0;
        for (int y = 0; y < 16; ++y)
            for (int x = 16; x < 28; ++x)
                if (npx(img, x, 100 + y) != BG) ++any;
        expect(any == 0, "prototype-compatible rules have no copy");
    }

    std::printf("%s test_sprites: %d checks, %d failures\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
