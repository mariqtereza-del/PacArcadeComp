// 2.1.0 unit test: FRAME RATE (sprites drawn between two machine ticks, render_sprites_lerp in src/video/sprites.cpp), and the new setting
// (menu.cpp: FRAME RATE). Usage: test_fps roms/pacman.zip
#include <SDL.h>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include "../src/board/romset.h"
#include "../src/video/gfx.h"
#include "../src/app/controls.cpp"
#include "../src/app/overlay.cpp"
#include "../src/app/menu.cpp"

void render_sprites(const Gfx &g, const uint8_t *lookup, const uint8_t *spriteram, const uint8_t *coords, uint32_t *out, bool mame_rules);
void render_sprites_lerp(const Gfx &g, const uint8_t *lookup, const uint8_t *ram_prev, const uint8_t *crd_prev, const uint8_t *ram_cur, const uint8_t *crd_cur, float alpha,
                         uint32_t *out, bool mame_rules);

static int fails = 0, n = 0;
#define CHECK(c, m)                                       \
    do {                                                  \
        ++n;                                              \
        if (!(c)) {                                       \
            ++fails;                                      \
            std::printf("FAIL: %s\n", m);                 \
        }                                                 \
    } while (0)

int main(int argc, char **argv) {
    if (argc < 2) { std::printf("usage: test_fps pacman.zip\n"); return 2; }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) { std::printf("ROM load failed: %s\n", err.c_str()); return 2; }
    static Gfx g;
    if (!decode_gfx(roms, g)) { std::printf("decode failed\n"); return 2; }
    const uint8_t *prom = roms.palettes.data();
    std::mt19937 rng(2100);
    static uint32_t bg[224 * 288], a[224 * 288], b[224 * 288];
    for (int i = 0; i < 224 * 288; ++i) bg[i] = 0x101010 + (uint32_t)(i % 7) * 0x010203;

    // 1. alpha = 1 equals render_sprites of the current tick, pixel for pixel; alpha = 0 equals render_sprites of the tick before when every sprite is the same picture and moved <= 8
    int bad1 = 0, bad0 = 0, bad_mid = 0, cases = 0;
    for (int mame = 0; mame < 2; ++mame)
        for (int t = 0; t < 400; ++t) {
            uint8_t rp[16], cp[16], rc[16], cc[16];
            for (int i = 0; i < 16; ++i) { rp[i] = rng(); cp[i] = (uint8_t)(20 + rng() % 216); }
            std::memcpy(rc, rp, 16);
            std::memcpy(cc, cp, 16);
            for (int s = 0; s < 8; ++s) {            // a small move for each sprite (same picture)
                cc[2 * s] = (uint8_t)(cp[2 * s] + (int)(rng() % 17) - 8);
                cc[2 * s + 1] = (uint8_t)(cp[2 * s + 1] + (int)(rng() % 17) - 8);
                rp[2 * s + 1] &= 0x1F;
                rc[2 * s + 1] = rp[2 * s + 1];
            }
            if (t % 2) for (int s = 0; s < 8; ++s) rc[2 * s] = (uint8_t)(rp[2 * s] ^ (4 << (rng() % 4))); // half of the cases: the pictures differ, then the current place is used at every alpha
            std::memcpy(a, bg, sizeof a);
            std::memcpy(b, bg, sizeof b);
            render_sprites_lerp(g, prom, rp, cp, rc, cc, 1.0f, a, mame);
            render_sprites(g, prom, rc, cc, b, mame);
            if (std::memcmp(a, b, sizeof a)) ++bad1;
            std::memcpy(a, bg, sizeof a);
            std::memcpy(b, bg, sizeof b);
            render_sprites_lerp(g, prom, rp, cp, rc, cc, 0.0f, a, mame);
            if (t % 2) render_sprites(g, prom, rc, cc, b, mame);   // the pictures differ: current place
            else render_sprites(g, prom, rp, cp, b, mame);
            if (std::memcmp(a, b, sizeof a)) ++bad0;
            // a move of an even number of pixels at alpha 0.5 lands on a whole pixel: equal to render_sprites at that place
            if (t % 2 == 0) {
                uint8_t cm[16], cc2[16];
                std::memcpy(cc2, cp, 16);
                std::memcpy(cm, cp, 16);
                for (int s = 0; s < 8; ++s) {
                    const int dy = 2 * ((int)(rng() % 5) - 2), dx = 2 * ((int)(rng() % 5) - 2);
                    cc2[2 * s] = (uint8_t)(cp[2 * s] + dy);
                    cc2[2 * s + 1] = (uint8_t)(cp[2 * s + 1] + dx);
                    cm[2 * s] = (uint8_t)(cp[2 * s] + dy / 2);
                    cm[2 * s + 1] = (uint8_t)(cp[2 * s + 1] + dx / 2);
                }
                std::memcpy(a, bg, sizeof a);
                std::memcpy(b, bg, sizeof b);
                render_sprites_lerp(g, prom, rp, cp, rp, cc2, 0.5f, a, mame);
                render_sprites(g, prom, rp, cm, b, mame);
                if (std::memcmp(a, b, sizeof a)) ++bad_mid;
            }
            ++cases;
        }
    std::printf("  %d random sprite situations (both rule sets): alpha 1 differs in %d, alpha 0 differs in %d, whole-pixel middle differs in %d\n", cases, bad1, bad0, bad_mid);
    CHECK(bad1 == 0, "alpha 1 is exactly the current tick's picture");
    CHECK(bad0 == 0, "alpha 0 is exactly the previous tick's picture (or the current one when the sprite changed)");
    CHECK(bad_mid == 0, "a move of two pixels at alpha 0.5 is exactly the picture one pixel along");

    // 2. smooth motion: one solid sprite moving 1 pixel per tick: its centre moves monotonically and evenly with alpha; a jump of more than 8 pixels is not smoothed
    {
        uint8_t ram[16] = {}, c0[16] = {}, c1[16] = {};
        // find a sprite number with many opaque pixels
        int best = 0, bestn = 0;
        for (int code = 0; code < 64; ++code) {
            int cnt = 0;
            for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) cnt += g.sprites[code][y][x] != 0;
            if (cnt > bestn) { bestn = cnt; best = code; }
        }
        ram[0] = (uint8_t)(best << 2);
        ram[1] = 1;
        for (int s = 1; s < 8; ++s) c0[2 * s + 1] = c1[2 * s + 1] = 0;   // the others are off the visible area (x register 0 = native 272: clipped)
        c0[0] = 100; c0[1] = 150; c1[0] = 104; c1[1] = 146;              // moves 4 along y and 4 along x
        double prev_cx = -1, prev_cy = -1, step_x = -1;
        bool monotone = true, even = true;
        for (int k = 0; k <= 8; ++k) {
            std::vector<uint32_t> img(224 * 288, 0);
            render_sprites_lerp(g, prom, ram, c0, ram, c1, k / 8.0f, img.data(), true);
            double sw = 0, sx = 0, sy = 0;
            for (int X = 0; X < 288; ++X)
                for (int col = 0; col < 224; ++col) {
                    const uint32_t p = img[X * 224 + col];
                    if (!p) continue;
                    const double w = ((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255);
                    sw += w; sx += w * X; sy += w * (223 - col);
                }
            const double cx = sx / sw, cy = sy / sw;
            if (k > 0) {
                if (!(cx > prev_cx + 1e-9 && cy > prev_cy + 1e-9)) monotone = false;   // x register falls (sx = 272 - x) so sx rises; y rises
                if (k == 1) step_x = cx - prev_cx;
                else if (std::fabs((cx - prev_cx) - step_x) > 0.02) even = false;
            }
            prev_cx = cx; prev_cy = cy;
        }
        CHECK(monotone, "a moving sprite's centre moves steadily from the old place to the new one");
        CHECK(even && step_x > 0.4 && step_x < 0.6, "equal alpha steps give equal steps of the centre (half a pixel per 1/8 of a 4-pixel move)");
        // a jump of 40 pixels (the side tunnel) is not smoothed
        c1[1] = 110;
        std::vector<uint32_t> p1(224 * 288, 0), p2(224 * 288, 0);
        render_sprites_lerp(g, prom, ram, c0, ram, c1, 0.5f, p1.data(), true);
        render_sprites(g, prom, ram, c1, p2.data(), true);
        CHECK(p1 == p2, "a jump of more than 8 pixels is drawn at the new place (no smearing across the tunnel)");
    }

    // 3. settings and menu
    {
        using namespace menu;
        CHECK(fps_value(0) == 30 && fps_value(1) == 60 && fps_value(2) == 75 && fps_value(3) == 100 && fps_value(4) == 120 && fps_value(5) == 144 && fps_value(6) == 0, "the seven frame rates");
        Model m;
        CHECK(value_text(Cmd::FrameRate, m) == "60", "default frame rate reads 60");
        m.fps = 5;
        CHECK(value_text(Cmd::FrameRate, m) == "144", "144");
        m.fps = 6;
        CHECK(value_text(Cmd::FrameRate, m) == "UNLOCKED", "unlocked");
        m.purist = true;
        CHECK(value_text(Cmd::FrameRate, m) == "60", "Purist: frame rate 60");
        CHECK(locked(Cmd::FrameRate, m), "Purist locks FRAME RATE");
        m.purist = false;
        CHECK(!locked(Cmd::FrameRate, m), "it is free otherwise");
        Settings s;
        CHECK(s.fps == 1, "default: 60");
        s.fps = 4;
        const std::string path = "build/x211/t_settings.cfg";
        std::string e;
        if (std::system("mkdir -p build/x211")) return 2;
        CHECK(save_settings(path, s, &e), "settings saved");
        Settings t;
        std::string probs;
        CHECK(load_settings(path, t, &probs) && t.fps == 4 && probs.empty(), "frame rate 120 comes back");
        FILE *f = std::fopen(path.c_str(), "ab");
        std::fputs("overlay on\nhdr on\nhdr_intensity 9\nframe_rate unlocked\n", f);
        std::fclose(f);
        Settings u;
        probs.clear();
        load_settings(path, u, &probs);
        CHECK(probs.empty(), "an old 'overlay' line is ignored without a message");
        CHECK(u.fps == 6, "unlocked; old hdr lines are ignored without a message");
        std::string txt;
        { FILE *r = std::fopen(path.c_str(), "rb"); char buf[4096]; size_t k = std::fread(buf, 1, sizeof buf, r); txt.assign(buf, k); std::fclose(r); }
        CHECK(txt.find("overlay") == std::string::npos || txt.find("overlay on") == txt.rfind("overlay on"), "the saved file has no overlay line of its own");
    }
    // 4. the DISPLAY page with its 15 rows: every line fits, the box ends inside the 288-line picture, FRAME RATE is the first row
    {
        using namespace menu;
        Menu dm;
        Model m;
        dm.open();
        for (int i = 0; i < 5; ++i) dm.key(Key::Down, m);
        dm.key(Key::Enter, m);
        CHECK(dm.page() == 3, "the DISPLAY page opens");
        const std::vector<std::string> ln = dm.visible_lines(m);
        CHECK(ln.size() == 15, "the DISPLAY page has 15 rows");
        bool fit = true;
        for (const std::string &l : ln) if (l.size() > 24) fit = false;
        CHECK(fit, "every DISPLAY line fits the box (24 characters)");
        CHECK(ln[0].rfind("FRAME RATE", 0) == 0, "FRAME RATE is the first row");
        static uint32_t buf[overlay::kOutW * 320];
        std::memset(buf, 0, sizeof buf);
        dm.draw(g, buf, m);
        bool below = false;
        for (int y = 288; y < 320; ++y) for (int x = 0; x < overlay::kOutW; ++x) if (buf[y * overlay::kOutW + x]) below = true;
        CHECK(!below, "nothing is drawn below the 288 lines of the picture");
    }
    std::printf("%d checks, %d failed\nRESULT: %s\n", n, fails, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
