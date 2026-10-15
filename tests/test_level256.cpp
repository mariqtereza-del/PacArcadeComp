// S30: tests for the debug overlay (src/app/overlay.*) and the level-256 split screen.
//
// Usage: test_level256 pacman.zip [out_dir]      (out_dir default docs/evidence: writes S30_level256.png, S30_overlay.png, S30_level256.txt)
// Build: g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_level256.cpp src/app/overlay.cpp src/video/sprites.cpp src/video/tilemap.cpp
//          src/video/gfx.cpp src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp build/app/miniz.o build/app/miniz_tdef.o
//          build/app/miniz_tinfl.o build/app/miniz_zip.o -I src/third_party/miniz -o build/s30/t256
//
// Part 1  The overlay cannot change the emulation: two machines run the same scripted game for 3,000 frames; one never calls the overlay, the other
//         calls overlay::compose on every frame. Memory hash and picture hash must be equal on every frame; the picture buffer handed to the overlay must be
//         unchanged afterwards; the overlay's output must equal the picture except for the small target markers and the panel.
// Part 2  The overlay's ghost targets equal the targets the ROM itself uses: at every call of the ROM's target routine (2966) the overlay's recomputed
//         target tile is compared with the register the ROM passed.
// Part 3  Level 256: HARNESS ONLY: the level counter 4E13 is set to 254 and the dots-eaten counter to 243; the ROM then finishes level 255 by itself and
//         builds level 256. The frame is saved and hashed; the split screen is measured against the level-1 maze (left half normal, right half not).
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../src/app/overlay.h"
#include "../src/board/inputs.h"
#include "../src/board/machine.h"
#include "../src/board/romset.h"
#include "../src/video/gfx.h"
#include "miniz.h"

void render_tilemap(const Gfx &g, const uint8_t *videoram, const uint8_t *colorram, uint32_t *out);
void render_sprites(const Gfx &g, const uint8_t *lookup, const uint8_t *spriteram, const uint8_t *coords, uint32_t *out, bool mame_rules);

static int failures = 0, checks = 0;
static FILE *g_out = nullptr;
static void say(const char *f, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *f, ...) {
    char b[1200];
    va_list a;
    va_start(a, f);
    std::vsnprintf(b, sizeof b, f, a);
    va_end(a);
    std::fputs(b, stdout);
    if (g_out) std::fputs(b, g_out);
}
static void expect(bool ok, const char *what) {
    ++checks;
    if (!ok) ++failures;
    say("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
}

static bool save_png(const std::string &path, const uint32_t *img, int w, int h) {
    std::vector<uint8_t> rgb((size_t)w * h * 3);
    for (int i = 0; i < w * h; ++i) {
        rgb[3 * i] = (uint8_t)(img[i] >> 16);
        rgb[3 * i + 1] = (uint8_t)(img[i] >> 8);
        rgb[3 * i + 2] = (uint8_t)img[i];
    }
    size_t n = 0;
    void *png = tdefl_write_image_to_png_file_in_memory(rgb.data(), w, h, 3, &n);
    if (!png) return false;
    FILE *f = std::fopen(path.c_str(), "wb");
    bool ok = f && std::fwrite(png, 1, n, f) == n;
    if (f) std::fclose(f);
    std::free(png);
    return ok;
}
static std::string pic_hash(const uint32_t *img, int w, int h) {
    std::vector<uint8_t> rgb((size_t)w * h * 3);
    for (int i = 0; i < w * h; ++i) {
        rgb[3 * i] = (uint8_t)(img[i] >> 16);
        rgb[3 * i + 1] = (uint8_t)(img[i] >> 8);
        rgb[3 * i + 2] = (uint8_t)img[i];
    }
    return sha1_hex(rgb.data(), rgb.size());
}

struct Player {
    uint32_t rng;
    int hold = 0;
    Control dir = Control::P1Left;
    explicit Player(uint32_t s) : rng(s * 2654435761u + 12345u) {}
    void apply(Inputs &in, int f) {
        in.set(Control::Coin1, f >= 400 && f < 405);
        in.set(Control::Start1, f >= 420 && f < 424);
        const Control dirs[4] = {Control::P1Up, Control::P1Left, Control::P1Right, Control::P1Down};
        for (Control c : dirs) in.set(c, false);
        if (f >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                dir = dirs[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
            in.set(dir, true);
        }
    }
};

struct Rig {
    Machine m;
    Inputs in;
    Player p;
    int f = 0;
    const Gfx &gfx;
    const RomSet &roms;
    // target-call comparison (part 2)
    bool compare_targets = false;
    long calls = 0, same = 0, skipped = 0;
    std::string first_bad;
    Rig(const RomSet &r, const Gfx &g, uint32_t seed) : m(r.program), p(seed), gfx(g), roms(r) {}
    int rd(int a) { return m.bus.read((uint16_t)a); }
    int rd16(int a) { return rd(a) | rd(a + 1) << 8; }
    void wr(int a, int v) { m.bus.write((uint16_t)a, (uint8_t)v); }
    void picture(uint32_t *fb) {
        render_tilemap(gfx, m.bus.videoram, m.bus.colorram, fb);
        render_sprites(gfx, roms.palettes.data(), m.bus.spriteram, m.bus.sprite_coords, fb, true);
    }
    std::string mem_hash() {
        uint8_t ram[0x1000];
        std::memcpy(ram, m.bus.videoram, 0x400);
        std::memcpy(ram + 0x400, m.bus.colorram, 0x400);
        std::memset(ram + 0x800, 0, 0x400);
        std::memcpy(ram + 0xC00, m.bus.workram, 0x3F0);
        std::memcpy(ram + 0xFF0, m.bus.spriteram, 0x10);
        return sha1_hex(ram, sizeof ram);
    }
    void hook(unsigned pc) {
        if (!compare_targets || pc != 0x2966) return;
        unsigned sp = m.on_get_sp();
        int ret = rd(sp) | rd(sp + 1) << 8;
        int g = -1;
        if (ret == 0x2751 || ret == 0x2765) g = 0;
        else if (ret == 0x2787 || ret == 0x27A2) g = 1;
        else if (ret == 0x27C4 || ret == 0x27EA) g = 2;
        else if (ret == 0x280C || ret == 0x2834) g = 3;
        if (g < 0) return;
        overlay::Info i = overlay::read_info(m.bus);
        int de = (int)m.on_get_de();
        int tcol = 59 - (de >> 8), trow = (de & 255) - 32;
        ++calls;
        if (i.g[g].target_kind == 2) {
            ++skipped;
            return;
        }
        if (i.g[g].tcol == tcol && i.g[g].trow == trow) ++same;
        else if (first_bad.empty()) {
            char b[160];
            std::snprintf(b, sizeof b, "ghost %d frame %d: ROM target %d,%d overlay %d,%d", g, f, tcol, trow, i.g[g].tcol, i.g[g].trow);
            first_bad = b;
        }
    }
    void frame() {
        p.apply(in, f);
        in.apply(m.bus);
        uint32_t t0 = m.ticks_in_frame();
        for (;;) {
            hook(m.on_get_pc());
            m.on_step();
            if (m.ticks_in_frame() - t0 >= Machine::cycles_per_frame) break;
        }
        ++m.frame;
        m.bus.set_frame(m.frame);
        m.deliver_vblank_interrupt();
        ++f;
    }
    bool playing() { return rd(0x4E00) == 3 && rd(0x4E04) == 3 && rd(0x4DA5) == 0; }
};

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_level256 pacman.zip [out_dir]\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    static Gfx gfx;
    if (!decode_gfx(roms, gfx)) {
        std::printf("graphics decode failed\n");
        return 2;
    }
    std::string dir = argc >= 3 ? argv[2] : "docs/evidence";
    g_out = std::fopen((dir + "/S30_level256.txt").c_str(), "w");
    say("S30 overlay and level-256 tests. The game code is not changed; the overlay only reads memory.\n");

    static uint32_t picA[224 * 288], picB[224 * 288], keep[224 * 288], over[overlay::kOutW * overlay::kOutH];

    // ------------------------------------------------------------------ part 1
    say("\n1. The overlay does not affect the emulation (3,000 frames, scripted game; machine A never calls the overlay, machine B calls it every frame)\n");
    {
        Rig A(roms, gfx, 5), B(roms, gfx, 5);
        int bad_mem = 0, bad_pic = 0, bad_keep = 0, bad_out = 0, first_bad = -1, drawn = 0;
        long marker_px_max = 0;
        bool panel_has_text = true;
        for (int i = 0; i < 3000; ++i) {
            A.frame();
            B.frame();
            A.picture(picA);
            B.picture(picB);
            std::memcpy(keep, picB, sizeof keep);
            overlay::compose(gfx, B.m.bus, picB, over);
            ++drawn;
            if (std::memcmp(keep, picB, sizeof keep) != 0) ++bad_keep;
            if (A.mem_hash() != B.mem_hash()) {
                if (!bad_mem++) first_bad = i + 1;
            }
            if (std::memcmp(picA, picB, sizeof picA) != 0) ++bad_pic;
            long diff = 0;
            for (int y = 0; y < 288; ++y)
                for (int x = 0; x < 224; ++x)
                    if (over[y * overlay::kOutW + x] != picB[y * 224 + x]) ++diff;
            if (diff > marker_px_max) marker_px_max = diff;
            if (diff > 4 * 40) ++bad_out;
            if (i == 2999) {
                int lit = 0;
                for (int y = 0; y < 288; ++y)
                    for (int x = 224; x < overlay::kOutW; ++x)
                        if (over[y * overlay::kOutW + x] != 0x101018) ++lit;
                panel_has_text = lit > 300;
            }
        }
        say("  frames compared: %d; memory hash differences %d (first at frame %d); picture hash differences %d; picture buffer changed by the overlay in %d frames\n",
            drawn, bad_mem, first_bad, bad_pic, bad_keep);
        say("  overlay output differs from the picture inside the picture area by at most %ld pixels (target markers only; 4 ghosts x 40 pixels allowed)\n",
            marker_px_max);
        expect(bad_mem == 0 && bad_pic == 0, "memory and picture are identical on every frame with and without the overlay");
        expect(bad_keep == 0, "the overlay never writes into the picture it is given");
        expect(bad_out == 0 && panel_has_text, "the overlay changes only target markers inside the picture area, and draws its text on the panel beside it");
        say("  (the overlay takes only 'const Bus&' and 'const uint32_t*': it has no way to write into the machine)\n");
    }

    // ------------------------------------------------------------------ part 2
    say("\n2. The overlay's ghost targets equal the targets the ROM uses\n");
    {
        Rig R(roms, gfx, 7);
        R.compare_targets = true;
        bool saved = false;
        int chase_since = -1;
        for (int i = 0; i < 5000; ++i) {
            // harness (as in S29): release the ghosts that wait in the house, and move the mode counter to 419 sixty frames into the scatter phase so chase follows
            if (R.playing() && R.f > 700) {
                for (int a = 0; a < 3; ++a)
                    if (!R.rd(0x4DA1 + a)) R.wr(0x4DA1 + a, a == 0 ? 2 : 3);
                if (R.rd(0x4DC1) == 0 && R.rd16(0x4DC2) == 60) {
                    R.wr(0x4DC2, 419 & 255);
                    R.wr(0x4DC3, 419 >> 8);
                }
            }
            R.frame();
            if (R.playing() && R.rd(0x4DC1) == 1 && R.rd(0x4DA6) == 0) {
                if (chase_since < 0) chase_since = R.f;
            } else if (R.rd(0x4DC1) != 1) chase_since = -1;
            if (!saved && chase_since >= 0 && R.f - chase_since >= 120 && R.playing()) {
                R.picture(picA);
                overlay::compose(gfx, R.m.bus, picA, over);
                saved = save_png(dir + "/S30_overlay.png", over, overlay::kOutW, overlay::kOutH);
                overlay::Info in = overlay::read_info(R.m.bus);
                say("  overlay picture saved (frame %d, chase phase, 2 seconds in): level %d phase %d counter %d next %d dots %d Pac-Man tile %d,%d\n", R.f, in.level,
                    in.phase, in.counter, in.limit, in.dots, in.pac_col, in.pac_row);
                const char *gn[4] = {"Blinky", "Pinky", "Inky", "Clyde"};
                for (int g = 0; g < 4; ++g)
                    say("    %-6s at %2d,%2d  %s target %2d,%2d\n", gn[g], in.g[g].col, in.g[g].row,
                        in.g[g].target_kind == 0   ? "corner"
                        : in.g[g].target_kind == 1 ? "chase "
                                                   : "other ",
                        in.g[g].tcol, in.g[g].trow);
            }
        }
        say("  ROM target calls %ld, shown by the overlay as frightened (no target) %ld, overlay target equal to the ROM's register in %ld of %ld\n", R.calls, R.skipped,
            R.same, R.calls - R.skipped);
        if (!R.first_bad.empty()) say("  first difference: %s\n", R.first_bad.c_str());
        expect(R.calls - R.skipped >= 200 && R.same == R.calls - R.skipped,
               "the overlay's target tile equals the ROM's own target on every call (scatter corners and all four chase rules)");
        expect(saved, "an overlay picture (S30_overlay.png) was saved");
    }

    // ------------------------------------------------------------------ part 3
    say("\n3. Level 256 (harness: 4E13 = 254 and dots eaten = 243; the ROM finishes level 255 and builds level 256)\n");
    {
        // reference: the normal level-1 maze right after it is drawn
        Rig L1(roms, gfx, 9);
        while (!L1.playing() || L1.f < 800) {
            L1.frame();
            if (L1.f > 3000) break;
        }
        std::vector<uint8_t> ref(L1.m.bus.videoram, L1.m.bus.videoram + 0x400);
        int ref_level = L1.rd(0x4E13);

        bool done = false;
        Rig *X = nullptr;
        for (uint32_t seed = 9; seed <= 29 && !done; ++seed) {
            delete X;
            X = new Rig(roms, gfx, seed);
            while (!X->playing() || X->f < 800) {
                X->frame();
                if (X->f > 3000) break;
            }
            if (!X->playing()) continue;
            X->wr(0x4E13, 254);
            X->wr(0x4E0E, 243); // harness
            int k = 0;
            while (X->rd(0x4E13) != 255 && k++ < 4000) {
                X->frame();
                if (X->rd(0x4E00) != 3) break;
            }
            if (X->rd(0x4E13) != 255) continue;
            int k2 = 0;
            while (!(X->rd(0x4E04) == 3 && X->rd(0x4E13) == 255 && X->rd(0x4DA5) == 0 && k2 > 400) && k2++ < 3000) {
                X->frame();
                if (X->rd(0x4E00) != 3) break;
            }
            if (X->rd(0x4E00) != 3) continue;
            done = true;
        }
        if (!done) {
            say("  could not reach level 256 in 20 seeds\n");
            expect(false, "level 256 reached");
        } else {
            say("  level counter 4E13 = %d (the screen shows level %d); main state %d, level state %d, frame %d\n", X->rd(0x4E13), X->rd(0x4E13) + 1, X->rd(0x4E00),
                X->rd(0x4E04), X->f);
            X->picture(picA);
            bool ok = save_png(dir + "/S30_level256.png", picA, 224, 288);
            std::string h = pic_hash(picA, 224, 288);
            say("  frame saved to S30_level256.png (%s); SHA-1 of its RGB bytes: %s\n", ok ? "written" : "NOT written", h.c_str());
            // split-screen measurement over the maze rows (screen rows 3 to 33)
            int ld = 0, rd_ = 0, lt = 0, rt = 0;
            // use the renderer's own picture instead of the tile RAM layout: compare level-256 picture with the level-1 picture in the maze area
            static uint32_t pic1[224 * 288];
            L1.picture(pic1);
            for (int y = 24; y < 264; ++y)
                for (int x = 0; x < 224; ++x) {
                    bool d = picA[y * 224 + x] != pic1[y * 224 + x];
                    if (x < 112) {
                        ++lt;
                        if (d) ++ld;
                    } else {
                        ++rt;
                        if (d) ++rd_;
                    }
                }
            say("  maze area pixels that differ from the normal level-1 maze: left half %d of %d (%.1f %%), right half %d of %d (%.1f %%)\n", ld, lt, 100.0 * ld / lt,
                rd_, rt, 100.0 * rd_ / rt);
            say("  (level 1 reference: level counter %d; both frames are taken once the maze is running; sprites and the player's moves make small differences on both "
                "sides)\n",
                ref_level);
            // keep running for 600 frames: does the game survive?
            int alive = 0;
            for (int i = 0; i < 600; ++i) {
                X->frame();
                if (X->rd(0x4E00) == 3) ++alive;
            }
            say("  the ROM kept running for %d of 600 further frames\n", alive);
            expect(ok, "the level-256 frame was saved");
            expect(X->rd(0x4E13) == 255 && alive == 600, "the game reaches level counter 255 (level 256) and keeps running");
            expect(rd_ > 3 * ld && 100.0 * rd_ / rt > 20.0, "split screen: the right half of the maze is mostly different from a normal maze, the left half is not");
            // overlay on that frame
            overlay::compose(gfx, X->m.bus, picA, over);
            save_png(dir + "/S30_level256_overlay.png", over, overlay::kOutW, overlay::kOutH);
        }
        delete X;
    }

    say("\n%d checks, %d failed. %s\n", checks, failures, failures ? "FAIL" : "PASS");
    if (g_out) std::fclose(g_out);
    return failures ? 1 : 0;
}
