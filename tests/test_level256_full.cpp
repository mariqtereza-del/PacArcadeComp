// S30A: level 1 to level 256 by the ROM's own code, the level-256 screen, the rack-test switch, and the table values at every level.
//
// Usage: test_level256_full pacman.zip [out_dir] [work_dir]
//          out_dir  default docs/evidence   (writes S30A_level256_full.txt)
//          work_dir default build/s30a      (writes ours_levels.txt, ours_events.txt, ours_vram300.bin and ours_picture.rgb for the MAME comparison, see tests/test_level256_full.sh)
// Build: see tests/test_level256_full.sh (g++ -std=c++17 -O2 ... src/board/*.cpp, no graphics needed).
//
// WHAT IS A SHORTCUT AND WHAT IS NOT (the evidence file repeats this):
//   NATURAL: the ROM itself advances the level counter 4E13 from 0 to 255 (code at 0A7C), builds every maze, plays the intermissions and builds level 256
//            (including the garbage in its right half). Nothing in the game or in the machine knows about level 256.
//   HARNESS SHORTCUTS (test code only, never in the game program), each applied in the natural run:
//            1. The dots-eaten counter 4E0E is set to 243 once per level while playing, so one more dot ends the level (otherwise 255 mazes
//               would take hours of play). The ROM then checks 244 itself (08DE-08E8) and ends the level by itself.
//            2. Lives are topped up to 3 when fewer than 2 are left (the random player dies a lot). No other score or life value is touched.
//            3. The player is a fixed pseudo-random joystick script (the same one tools/analysis/mame_level256.lua uses).
//   POKED (diagnostic only, NOT proof of natural play): part 2 starts a game, writes 4E13 = 254 and 4E0E = 243 once, and compares the screen with the
//            natural level 256.
//
// Parts: 1 natural run to level 256 (tables at all 256 level starts, intermissions, counter behaviour, the screen, 60,000 frames of play)
//        2 diagnostic poked run versus the natural screen
//        3 rack-test switch on level 256 (what the counter and the parameter tables do afterwards)
// The same player and shortcuts are scripted in tools/analysis/mame_level256.lua so that MAME can play the identical sequence; the two logs are
// compared by tools/analysis/compare_mame_level256.py.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "../src/board/inputs.h"
#include "../src/board/machine.h"
#include "../src/board/romset.h"
#include "../src/video/gfx.h"

void render_tilemap(const Gfx &g, const uint8_t *videoram, const uint8_t *colorram, uint32_t *out);
void render_sprites(const Gfx &g, const uint8_t *lookup, const uint8_t *spriteram, const uint8_t *coords, uint32_t *out, bool mame_rules);

static int failures = 0, checks = 0;
static FILE *g_out = nullptr;
static void say(const char *f, ...) __attribute__((format(printf, 1, 2)));
static void say(const char *f, ...) {
    char b[1500];
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

struct Player {
    uint32_t rng = 7u * 2654435761u + 12345u;
    int hold = 0;
    Control dir = Control::P1Left;
    void apply(Inputs &in, int f, bool rack) {
        in.set(Control::Coin1, f >= 400 && f < 405);
        in.set(Control::Start1, f >= 420 && f < 424);
        const Control d[4] = {Control::P1Up, Control::P1Left, Control::P1Right, Control::P1Down};
        for (Control c : d) in.set(c, false);
        if (f >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
            in.set(dir, true);
        }
        in.set(Control::RackTest, rack);
    }
};

struct Rig {
    Machine m;
    Inputs in;
    Player p;
    int f = 0;
    bool rack = false;
    explicit Rig(const RomSet &r) : m(r.program) {}
    int rd(int a) { return m.bus.read((uint16_t)a); }
    int rd16(int a) { return rd(a) | rd(a + 1) << 8; }
    void wr(int a, int v) { m.bus.write((uint16_t)a, (uint8_t)v); }
    void frame() {
        p.apply(in, f, rack);
        in.apply(m.bus);
        m.run_frame();
        ++f;
    }
    bool state3() { return rd(0x4E00) == 3; }
    bool playing() { return rd(0x4E00) == 3 && rd(0x4E04) == 3 && rd(0x4DA5) == 0; }
    int speed(int a) {
        unsigned v = (unsigned)rd(a) | (unsigned)rd(a + 1) << 8 | (unsigned)rd(a + 2) << 16 | (unsigned)rd(a + 3) << 24;
        return __builtin_popcount(v) * 5;
    }
    void table_row(int n, int got[14]) {
        got[0] = n;
        got[1] = speed(0x4D46);
        got[2] = speed(0x4D4A);
        got[3] = speed(0x4D4E);
        got[4] = speed(0x4D52);
        got[5] = speed(0x4D56);
        got[6] = speed(0x4D5A);
        got[7] = speed(0x4D5E);
        got[8] = rd16(0x4DBD);
        got[9] = rd(0x4DB8);
        got[10] = rd(0x4DB9);
        got[11] = rd(0x4DBA);
        got[12] = rd(0x4DBB);
        got[13] = rd(0x4DBC);
    }
    void tiles(int &dots, int &en) {
        dots = en = 0;
        for (int i = 0; i < 0x400; ++i) {
            if (m.bus.videoram[i] == 0x10) ++dots;
            if (m.bus.videoram[i] == 0x14) ++en;
        }
    }
};

// the picture the port draws from the machine state (tiles then sprites, MAME's sprite rules as in the window app); raw RGB, 224 x 288, for the pixel comparison
static bool save_picture(const std::string &path, const Gfx &gfx, const RomSet &roms, const Machine &m) {
    static uint32_t fb[224 * 288];
    render_tilemap(gfx, m.bus.videoram, m.bus.colorram, fb);
    render_sprites(gfx, roms.palettes.data(), m.bus.spriteram, m.bus.sprite_coords, fb, true);
    FILE *o = std::fopen(path.c_str(), "wb");
    if (!o) return false;
    for (int i = 0; i < 224 * 288; ++i) {
        uint8_t px[3] = {(uint8_t)(fb[i] >> 16), (uint8_t)(fb[i] >> 8), (uint8_t)fb[i]};
        std::fwrite(px, 1, 3, o);
    }
    std::fclose(o);
    return true;
}

static bool save_bin(const std::string &path, const Machine &m) {
    FILE *o = std::fopen(path.c_str(), "wb");
    if (!o) return false;
    std::fwrite(m.bus.videoram, 1, 0x400, o);
    std::fwrite(m.bus.colorram, 1, 0x400, o);
    std::fclose(o);
    return true;
}

// expected table, tests/data/level_expect.txt (made from the ROM-decoded tables of S12 by tools/analysis/make_level_expect.py)
static int E[22][14];
static bool row_ok(int idx, const int got[14]) {
    for (int j = 1; j < 14; ++j) {
        int e = E[idx][j];
        if ((j == 2 || j == 6) && e == -1) continue;   // frightened speeds are not used when there is no frightened time
        if (j == 8 && e == 0 && got[j] <= 1) continue; // frightened time 0: the ROM stores 1 (checked below to be what MAME also stores)
        if (got[j] != e) return false;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_level256_full pacman.zip [out_dir] [work_dir]\n");
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
    std::string dir = argc >= 3 ? argv[2] : "docs/evidence", work = argc >= 4 ? argv[3] : "build/s30a";
    g_out = std::fopen((dir + "/S30A_level256_full.txt").c_str(), "w");
    FILE *ex = std::fopen("tests/data/level_expect.txt", "r");
    if (!ex) {
        std::printf("tests/data/level_expect.txt missing (run tools/analysis/make_level_expect.py)\n");
        return 2;
    }
    for (int i = 1; i <= 21; ++i)
        for (int j = 0; j < 14; ++j)
            if (std::fscanf(ex, "%d", &E[i][j]) != 1) {
                std::printf("bad level_expect.txt\n");
                return 2;
            }
    std::fclose(ex);
    FILE *lvf = std::fopen((work + "/ours_levels.txt").c_str(), "w"), *evf = std::fopen((work + "/ours_events.txt").c_str(), "w");
    if (!lvf || !evf) {
        std::printf("cannot write into %s (create the folder first)\n", work.c_str());
        return 2;
    }

    say("S30A level 256 tests. The game code is not changed. Shortcuts: dots-eaten counter set to 243 once per level, lives topped up, scripted random player.\n");

    // ======================================================================================= part 1 (natural) + part 3 (rack test) in one run
    say("\n1. Natural run: the ROM itself plays level 1 to level 256 (about 155,000 frames)\n");
    Rig R(roms);
    int last = -1, poked = -1, pend = -1, pend_f = 0, l256 = -1, phase = 0, wrapf = -1, rackf = -1, maxeaten = 0, bad = 0, checked = 0;
    int counter_jumps = 0, started = -1, frames_l256_state3 = 0, left255 = 0;
    std::set<int> i32;
    std::vector<int> seq;
    int poked_after = -1;
    int rack_counter = -1, rack_dots = -1, rack_en = -1, rack_eaten_at = 0;
    int l256_dots = -1, l256_en = -1, l256_eaten = -1;
    int rowbad_example[14] = {0};
    int bad_n = -1;
    std::map<int, int> i32_frame;
    int r_i32_counter1 = -1;
    int rack_row[14] = {0};
    bool rack_row_done = false;
    int post_wrap_counter_changes = 0, final_counter = -1;
    for (;;) {
        R.frame();
        int lvl = R.rd(0x4E13);
        bool s3 = R.state3();
        int ikey = lvl + (phase >= 4 ? 1000 : 0); // after the wrap the same counter values come again: keep the two periods apart
        if (s3 && R.rd(0x4E04) == 32 && !i32.count(ikey)) {
            i32.insert(ikey);
            std::fprintf(evf, "I %d %d\n", lvl, R.f);
            i32_frame[ikey] = R.f;
            if (phase >= 4 && lvl == 1) r_i32_counter1 = R.f;
        }
        if (s3 && lvl != last) {
            std::fprintf(evf, "L %d %d\n", lvl, R.f);
            if (last >= 0 && lvl != last + 1 && !(phase >= 3)) ++counter_jumps;
            if (last == 255 && phase >= 3) {
                wrapf = R.f;
                ++post_wrap_counter_changes;
                R.rack = false;
                phase = 4;
            } else if (phase >= 4) ++post_wrap_counter_changes;
            if (lvl == 0 && started < 0 && R.f > 500) started = R.f;
            last = lvl;
            pend = lvl;
            pend_f = R.f + 300;
            seq.push_back(lvl);
        }
        if (pend >= 0 && R.f == pend_f && s3) {
            int n = pend + 1, got[14];
            R.table_row(n, got);
            for (int j = 0; j < 14; ++j) std::fprintf(lvf, "%d%c", got[j], j == 13 ? '\n' : ' ');
            if (phase < 3) {
                int idx = n > 21 ? 21 : n;
                ++checked;
                if (!row_ok(idx, got)) {
                    ++bad;
                    if (bad_n < 0) {
                        bad_n = n;
                        std::memcpy(rowbad_example, got, sizeof got);
                    }
                }
            }
            pend = -1;
        }
        if (phase == 0 && last == 255) {
            l256 = R.f;
            phase = 1;
        }
        if (phase >= 1 && phase <= 2) {
            if (R.rd(0x4E0E) > maxeaten) maxeaten = R.rd(0x4E0E);
            if (R.state3()) ++frames_l256_state3;
            if (R.rd(0x4E13) != 255) ++left255;
            if (R.f == l256 + 300) {
                R.tiles(l256_dots, l256_en);
                l256_eaten = R.rd(0x4E0E);
                if (!save_bin(work + "/ours_vram300.bin", R.m) || !save_picture(work + "/ours_picture.rgb", gfx, roms, R.m)) {
                    std::printf("cannot write the dumps\n");
                    return 2;
                }
                std::fprintf(evf, "V %d %d %d\n", l256_dots, l256_en, l256_eaten);
            }
            if (R.f == l256 + 60000) {
                std::fprintf(evf, "X %d\n", maxeaten);
                phase = 2;
            }
        }
        if (phase == 2 && R.f >= l256 + 60000 && R.playing()) {
            R.rack = true;
            rackf = R.f;
            rack_counter = R.rd(0x4E13);
            std::fprintf(evf, "R %d %d\n", rackf, rack_counter);
            phase = 3;
        }
        if (phase == 4 && wrapf >= 0 && R.f == wrapf + 300 && s3 && !rack_row_done) {
            R.table_row(R.rd(0x4E13) + 1, rack_row);
            R.tiles(rack_dots, rack_en);
            rack_eaten_at = R.rd(0x4E0E);
            rack_row_done = true;
            std::fprintf(evf, "W %d %d %d\n", rack_dots, rack_en, rack_eaten_at);
            for (int j = 0; j < 14; ++j) std::fprintf(evf, "%d%c", rack_row[j], j == 13 ? '\n' : ' ');
        }
        // harness shortcuts
        bool pl = R.playing();
        if (pl && R.rd(0x4E13) < 255 && phase < 3 && poked != R.rd(0x4E13) && R.rd(0x4E0E) < 200) {
            R.wr(0x4E0E, 243);
            poked = R.rd(0x4E13);
        }
        if (phase == 4 && rack_row_done && poked_after != R.rd(0x4E13) && pl && R.rd(0x4E0E) < 200) {
            R.wr(0x4E0E, 243);
            poked_after = R.rd(0x4E13);
        }
        if (pl && R.rd(0x4E14) < 2) {
            R.wr(0x4E14, 3);
            R.wr(0x4E15, 2);
        }
        if (phase == 4 && R.f >= wrapf + 3000) break;
        if (R.f > 400000) break; // runaway guard
        if (R.rd(0x4E00) == 1 && R.f > 600 && last >= 0 && phase < 3) {
            say("  game over before level 256 (frame %d, counter %d)\n", R.f, last);
            break;
        }
    }
    final_counter = last;
    std::fclose(lvf);
    std::fclose(evf);
    char b[700];
    expect(started > 0, "a game was started by the script (counter 0 seen after the attract demo)");
    std::snprintf(b, sizeof b, "the level counter 4E13 reached 255 by the ROM's own code (frame %d, game started at frame %d)", l256, started);
    expect(l256 > 0, b);
    // sequence from the game start: 0,1,2 ... 255 with no jump
    bool seq_ok = true;
    {
        size_t i0 = 0;
        while (i0 < seq.size() && seq[i0] != 0) ++i0;
        if (i0 + 256 > seq.size()) seq_ok = false;
        for (int k = 0; seq_ok && k < 256; ++k)
            if (seq[i0 + k] != k) seq_ok = false;
    }
    expect(seq_ok && counter_jumps <= 1,
           "the counter went 0,1,2,...,255 one at a time (no skipped or repeated level; the single earlier jump is the attract demo's counter 9 -> 0 at game start)");
    std::snprintf(b, sizeof b, "%d level starts checked against the ROM tables decoded in S12 (levels 21 and above must use the level-21 entry): %d mismatches", checked,
                  bad);
    expect(checked >= 256 && bad == 0, b);
    if (bad)
        say("     first mismatch: level %d got %d %d %d %d %d %d %d %d %d %d %d %d %d\n", bad_n, rowbad_example[1], rowbad_example[2], rowbad_example[3],
            rowbad_example[4], rowbad_example[5], rowbad_example[6], rowbad_example[7], rowbad_example[8], rowbad_example[9], rowbad_example[10], rowbad_example[11],
            rowbad_example[12], rowbad_example[13]);
    {
        std::set<int> want = {1, 4, 8, 12, 16};
        std::set<int> before;
        for (int c : i32)
            if (c < 1000) before.insert(c);
        std::string s;
        for (int c : before) { s += std::to_string(c) + " "; }
        std::snprintf(b, sizeof b,
                      "intermissions (level state 32) happened at counter values { %s} = after levels 2, 5, 9, 13 and 17, and nowhere else (Dossier: Act I after level "
                      "2, II after 5, III after 9, 13, 17)",
                      s.c_str());
        expect(before == want, b);
    }
    std::snprintf(
        b, sizeof b,
        "level 256: the counter stayed 255 for 60,000 frames (frames with another value: %d) and the ROM kept running (main state 3 on %d frames of the 60,001)", left255,
        frames_l256_state3);
    expect(left255 == 0 && frames_l256_state3 >= 60000, b);
    std::snprintf(b, sizeof b, "level 256 cannot be completed: the most dots eaten at once was %d of the 244 needed (the Dossier: only 168 can be harvested)", maxeaten);
    expect(maxeaten < 244 && maxeaten >= 100, b);
    std::snprintf(b, sizeof b,
                  "level 256 screen 300 frames in: %d dot tiles + %d energizer tiles + %d dots already eaten = %d (a full maze has 244 + the Dossier's right-half "
                  "remnants: 114 left + 9 right = 123)",
                  l256_dots, l256_en, l256_eaten, l256_dots + l256_en + l256_eaten);
    expect(l256_dots + l256_en + l256_eaten == 123, b);
    say("     (the line above: 123 = 114 dots on the left half + 9 on the right half, the Dossier's count of what survives on the kill-screen maze)\n");

    // ======================================================================================= part 2 (diagnostic)
    say("\n2. Diagnostic only (POKED, not natural play): start a game, write 4E13 = 254 and 4E0E = 243 once; compare with the natural level 256\n");
    {
        Rig P(roms);
        int pl256 = -1;
        bool did = false;
        uint8_t pv[0x800] = {0};
        int prow[14] = {0};
        for (int guard = 0; guard < 20000; ++guard) {
            P.frame();
            if (!did && P.f >= 900 && P.playing() && P.rd(0x4E0E) < 200) {
                P.wr(0x4E13, 254);
                P.wr(0x4E0E, 243);
                did = true;
            }
            if (P.playing() && P.rd(0x4E14) < 2) {
                P.wr(0x4E14, 3);
                P.wr(0x4E15, 2);
            }
            if (did && pl256 < 0 && P.state3() && P.rd(0x4E13) == 255) pl256 = P.f;
            if (pl256 >= 0 && P.f == pl256 + 300) {
                std::memcpy(pv, P.m.bus.videoram, 0x400);
                std::memcpy(pv + 0x400, P.m.bus.colorram, 0x400);
                P.table_row(256, prow);
                break;
            }
        }
        uint8_t nv[0x800];
        FILE *o = std::fopen((work + "/ours_vram300.bin").c_str(), "rb");
        bool ok = o && std::fread(nv, 1, 0x800, o) == 0x800;
        if (o) std::fclose(o);
        expect(pl256 > 0 && ok, "the poked run reached level 256 and the natural dump was read back");
        int garb = 0, other = 0;
        std::string cells;
        if (pl256 > 0 && ok)
            for (int i = 0; i < 0x800; ++i)
                if (pv[i] != nv[i]) {
                    int a = i & 0x3FF;
                    if (a >= 0x004 && a <= 0x20F) {
                        ++garb;
                        if (cells.size() < 120) {
                            char c[40];
                            std::snprintf(c, sizeof c, "%s%03X(%02X/%02X) ", i >= 0x400 ? "c" : "v", a, pv[i], nv[i]);
                            cells += c;
                        }
                    } else {
                        ++other;
                    }
                }
        std::snprintf(b, sizeof b, "garbage area (cells 004-20F of video and colour RAM): %d of 1,036 cells differ between the poked and the natural run; first: %s",
                      garb, cells.c_str());
        expect(garb <= 8, b);
        say("     (cells that differ are dots and energizers eaten by the two different random walks; other cells differing, e.g. score and lives: %d)\n", other);
        say("     poked run, parameter table 300 frames in: pac %d, ghost %d, fright word %d, mode table first entry per RAM; natural level 256 has pac %d, ghost %d, "
            "fright word 1\n",
            prow[1], prow[5], prow[8], E[21][1], E[21][5]);
        std::snprintf(b, sizeof b,
                      "NOTE (why a poked level is only a diagnostic): the poked run's speeds and times are those of level 2 (pac %d %%, ghost %d %%, fright word %d), "
                      "not level 21's (pac %d %%, ghost %d %%), because the ROM's level-record pointer 4E0A moves one record per finished level and the poke does not "
                      "move it; the maze and the garbage depend on 4E13 only",
                      prow[1], prow[5], prow[8], E[21][1], E[21][5]);
        expect(pl256 > 0 && row_ok(2, prow) && !row_ok(21, prow), b);
    }

    // ======================================================================================= part 3 (rack test)
    say("\n3. Rack-test switch (IN0 bit 4) held after 60,000 frames on level 256 (the ROM code at 08CD ends the level when the switch is on)\n");
    std::snprintf(b, sizeof b, "the switch was pressed at frame %d with counter %d", rackf, rack_counter);
    expect(rackf > 0 && rack_counter == 255, b);
    std::snprintf(b, sizeof b, "the level ended and the counter wrapped 255 -> 0 at frame %d (the single byte at 4E13 overflows; the ROM does not stop it)", wrapf);
    expect(wrapf > 0 && final_counter >= 0, b);
    if (rack_row_done) {
        say("     table 300 frames after the wrap: pac %d, pac-fright %d, elroy2 %d, elroy1 %d, ghost %d, ghost-fright %d, tunnel %d, fright time %d, house %d/%d/%d, "
            "elroy dots %d/%d\n",
            rack_row[1], rack_row[2], rack_row[3], rack_row[4], rack_row[5], rack_row[6], rack_row[7], rack_row[8], rack_row[9], rack_row[10], rack_row[11], rack_row[12],
            rack_row[13]);
        bool same21 = row_ok(21, rack_row);
        expect(same21, "after the wrap the speeds and times are still the level-21 values (the level-record pointer 4E0A stays at the end of its table; the Dossier: "
                       "'ghost behaviour as at level 21')");
        std::snprintf(b, sizeof b,
                      "the maze after the wrap is a normal one: %d dot tiles + %d energizer tiles + %d eaten (a full maze has 240 dots + 4 energizers = 244)", rack_dots,
                      rack_en, rack_eaten_at);
        expect(rack_dots + rack_en + rack_eaten_at == 244, b);
    } else expect(false, "table after the wrap was read");
    std::snprintf(b, sizeof b,
                  "playing on at counter 0 and finishing that maze: counter went to 1 and the first intermission (state 32) came again at frame %d (level index 1 uses "
                  "the first cutscene)",
                  r_i32_counter1);
    expect(r_i32_counter1 > 0, b);
    say("     the score is kept (the ROM never clears it between levels): not asserted here, see the evidence of S29.\n");

    say("\nS30A part 1-3: %d checks, %d failed\n", checks, failures);
    if (g_out) std::fclose(g_out);
    return failures ? 1 : 0;
}
