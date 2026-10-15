// S29: behaviour tests B: ghosts, scoring, lives. Measured on the C++ machine from RAM and from the program counter and compared with the
// Pac-Man Dossier (https://pacman.holenet.info/, chapters on ghost behaviour, the ghost house, Cruise Elroy and scoring) and with
// docs/notes/table_comparison.md. The game code is not changed. Positions and counters are only ever set by this test harness (one RAM write,
// announced in the output); the ROM code that reacts is never touched.
//
// Usage: test_behaviour_b pacman.zip [evidence.txt]
// Build: g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_behaviour_b.cpp src/board/inputs.cpp src/board/romset.cpp
//          src/board/machine.cpp src/board/bus.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o build/s29/tbb
//
// Frame loop, scripted player and instruction hooks work as in tests/test_behaviour_a.cpp (S28): the loop is the same as Machine::run_frame
// but looks at the program counter before every instruction.
//
// Tile bytes (hardware_facts / ram_map): a tile is a byte pair; first byte E = row + 32 (grows downward), second byte D = 59 - column (grows to the
// LEFT). So "right" is D-1, "left" is D+1, "down" is E+1, "up" is E-1. The ghost target routine 2966 receives the target in DE and the
// ghost's tile in HL; the caller's return address tells which ghost and which rule (2751 Blinky scatter, 2765 Blinky chase, 2787/27A2 Pinky,
// 27C4/27EA Inky, 280C/2834 Clyde).
//
// Items
//  T. Ghost targets: scatter corners, Blinky = Pac-Man's tile, Pinky = 4 ahead (up: also 4 left), Inky = twice the vector from Blinky to the point
//     2 ahead (up: also 2 left), Clyde = Pac-Man if 8 or more tiles away else his corner. Compared on every call in a long scripted game.
//  H. Ghost-house release: personal counters (L1: Pinky 0, Inky 30, Clyde 60), global counter after a death (7, 17, 32), inactivity timer (4 s).
//  E. Cruise Elroy: dots-left thresholds 20 and 10, speeds 80 % and 85 %, ignores scatter, switched off while Clyde is in the house.
//  S. Scoring: dot 10, energizer 50, ghosts 200/400/800/1600 in a row (restarting at 200 with the next energizer), fruit 100 (cherry).
//  X. Extra life at 10,000 (DIP default) and with the other DIP bonus settings, once only.
//  L. Lives: the DIP lives setting (1, 2, 3, 5), one life lost per death, game over after the last.
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "../src/board/inputs.h"
#include "../src/board/machine.h"
#include "../src/board/romset.h"

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

struct Player { // coin at 400, start at 420, random joystick (hold 10-41 frames) from 430
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

struct Tgt {
    int ghost, ret, de, hl, phase, state, pill, pe, pd, vec, bl, elroy, frame;
};
struct Rel {
    int ghost, frame, dots, c0, c1, c2, lim0, lim1, lim2, global, died, level, path, timer, timer_limit;
}; // path 0 counter, 1 global, 2 timer
struct Score {
    int frame, delta, tile, before;
};
struct FrameLog {
    int f, state, sub, pill, dying, c1806, bl_y, bl_x, elroy1, elroy2, dots, c88, e88;
};

struct Run {
    Machine m;
    Inputs in;
    Player player;
    int f = 0;
    unsigned prev_pc = 0;
    int c1806 = 0;
    std::vector<Tgt> tgts;
    std::vector<Rel> rels;
    std::vector<Score> scores;
    std::vector<FrameLog> log;
    std::vector<int> extra_frames;
    int last_tile = 0, score_before = 0;
    bool force_energizer = false;
    int forced = 0;
    int timer_hit = -1, timer_hit_limit = -1, sum_before = 0, dots_counted = 0, dots_count_ok = 0, house_open = 0;
    int pokes_inky = 0, pokes_clyde = 0;
    int c88 = 0, e88 = 0;
    bool in88 = false;
    explicit Run(const std::vector<uint8_t> &prog, uint32_t seed) : m(prog), player(seed) {}
    int rd(int a) { return m.bus.read((uint16_t)a); }
    int rd16(int a) { return rd(a) | rd(a + 1) << 8; }
    void wr(int a, int v) { m.bus.write((uint16_t)a, (uint8_t)v); }
    int score() {
        auto b = [&](int a) {
            int v = rd(a);
            return (v >> 4) * 10 + (v & 15);
        };
        return b(0x4E80) + 100 * b(0x4E81) + 10000 * b(0x4E82);
    }
    void set_score(int s) {
        auto bcd = [](int v) { return ((v / 10) % 10) << 4 | (v % 10); };
        wr(0x4E80, bcd(s % 100));
        wr(0x4E81, bcd((s / 100) % 100));
        wr(0x4E82, bcd((s / 10000) % 100));
    }
    void release(int g, unsigned pc) {
        Rel r{};
        r.ghost = g;
        r.frame = f + 1;
        r.dots = rd(0x4E0E);
        r.c0 = rd(0x4E0F);
        r.c1 = rd(0x4E10);
        r.c2 = rd(0x4E11);
        r.lim0 = rd(0x4DB8);
        r.lim1 = rd(0x4DB9);
        r.lim2 = rd(0x4DBA);
        r.global = rd(0x4D9F);
        r.died = rd(0x4E12);
        r.level = rd(0x4E13);
        r.timer = timer_hit;
        r.timer_limit = timer_hit_limit;
        timer_hit = -1;
        bool timer = (g == 0 && prev_pc == 0x1408) || (g == 1 && prev_pc == 0x1412) || (g == 2 && prev_pc == 0x141B);
        bool global = (g == 0 && prev_pc == 0x207B) || (g == 1 && prev_pc == 0x209E);
        r.path = timer ? 2 : global ? 1 : 0;
        (void)pc;
        rels.push_back(r);
    }
    void hook(unsigned pc) {
        switch (pc) {
        case 0x1806: ++c1806; break;
        case 0x1B36: in88 = false; break; // Blinky's mover starts
        case 0x1B88:
            ++c88;
            in88 = true;
            break; // Blinky's speed tick reached the Elroy/normal branch (not tunnel, not frightened)
        case 0x1BD8:
            if (in88) ++e88;
            break; // the speed pattern fired: one step
        case 0x2966: {
            unsigned sp = m.on_get_sp();
            int ret = rd(sp) | rd(sp + 1) << 8;
            int g = -1;
            if (ret == 0x2751 || ret == 0x2765) g = 0;
            else if (ret == 0x2787 || ret == 0x27A2) g = 1;
            else if (ret == 0x27C4 || ret == 0x27EA) g = 2;
            else if (ret == 0x280C || ret == 0x2834) g = 3;
            if (g >= 0)
                tgts.push_back({g, ret, (int)m.on_get_de(), (int)m.on_get_hl(), rd(0x4DC1), rd(0x4E04), rd(0x4DA6), rd(0x4D39), rd(0x4D3A), rd16(0x4D1C), rd16(0x4D0A),
                                rd(0x4DB6), f + 1});
            break;
        }
        case 0x13FD:
            timer_hit = rd16(0x4D97);
            timer_hit_limit = rd16(0x4D95);
            break; // inactivity timer reached its limit
        case 0x1B08:
            sum_before = rd(0x4E0F) + rd(0x4E10) + rd(0x4E11) + rd(0x4D9F);
            house_open = (rd(0x4E12) ? 1 : (rd(0x4DA3) == 0));
            break; // a dot is being counted for the ghost house
        case 0x1A03: {
            int d = rd(0x4E0F) + rd(0x4E10) + rd(0x4E11) + rd(0x4D9F) - sum_before;
            ++dots_counted;
            if (house_open ? d == 1 : d == 0) ++dots_count_ok;
            break;
        }
        case 0x2086: release(0, pc); break;
        case 0x20A9: release(1, pc); break;
        case 0x20D1: release(2, pc); break;
        case 0x19D8:
            if (force_energizer) {
                m.bus.write((uint16_t)m.on_get_hl(), 0x14);
                force_energizer = false;
                ++forced;
            }
            break; // harness: an energizer is under Pac-Man
        case 0x19E0: last_tile = m.on_get_a(); break;
        case 0x2A5A: score_before = score(); break;
        case 0x2A86:
            scores.push_back({f + 1, score() - score_before, last_tile, score_before});
            last_tile = 0;
            break;
        case 0x2B33: extra_frames.push_back(f + 1); break;
        default: break;
        }
        prev_pc = pc;
    }
    void frame() {
        player.apply(in, f);
        in.apply(m.bus);
        c1806 = 0;
        c88 = e88 = 0;
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
        log.push_back({f, rd(0x4E00), rd(0x4E04), rd(0x4DA6), rd(0x4DA5), c1806, rd(0x4D00), rd(0x4D01), rd(0x4DB6), rd(0x4DB7), rd(0x4E0E), c88, e88});
    }
    bool playing() { return rd(0x4E00) == 3 && rd(0x4E04) == 3 && rd(0x4DA5) == 0; }
    bool run_until_playing() {
        while (f < 900) frame();
        return playing();
    }
};

static std::vector<uint8_t> g_prog;
static int tcol(int d) { return 59 - d; }
static int trow(int e) { return e - 32; }

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_behaviour_b pacman.zip [evidence.txt]\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    g_prog = roms.program;
    if (argc >= 3) g_out = std::fopen(argv[2], "w");
    say("S29 behaviour tests B (ghosts, scoring, lives). The game code is not changed. Values are measured on the running ROM; every place where the test\n"
        "writes a RAM value to set up a situation is marked 'harness'.\n");

    // ================================================================= survey run for T
    say("\nSurvey T: scripted games (seed 1, 2, 3 ...), harness: once the maze runs, the three ghosts in the house are released (4DA1 = 2, 4DA2 = 3, 4DA3 = 3, the "
        "values the\n"
        "  ROM's own release routines store) and 60 frames into each life's scatter phase the mode counter is moved to 419 (time jump) so the chase phase follows, until "
        "one game shows every target rule, Pac-Man facing up for Pinky and Inky, and Clyde both near and far\n");
    Run *R = nullptr;
    uint32_t seed_used = 0;
    for (uint32_t seed = 1; seed <= 80; ++seed) {
        delete R;
        R = new Run(g_prog, seed);
        for (int i = 0; i < 9000; ++i) {
            if (R->playing() && R->f > 700) {
                if (!R->rd(0x4DA1)) R->wr(0x4DA1, 2);
                if (!R->rd(0x4DA2)) R->wr(0x4DA2, 3);
                if (!R->rd(0x4DA3)) R->wr(0x4DA3, 3);
                if (R->rd(0x4DC1) == 0 && R->rd16(0x4DC2) == 60) {
                    R->wr(0x4DC2, 419 & 255);
                    R->wr(0x4DC3, 419 >> 8);
                }
            } // harness time jump: 60 frames of scatter, then the counter is moved to 419 so chase starts next frame
            R->frame();
            if (R->rd(0x4E00) == 1 && R->f > 600) break;
            if (i > 1500 && i % 500 == 0) {
                int up_p = 0, up_i = 0, far_c = 0, near_c = 0;
                for (auto &t : R->tgts) {
                    bool chase = t.ret == 0x27A2 || t.ret == 0x27EA || t.ret == 0x2834;
                    int v = t.vec;
                    if (!chase) continue;
                    if (v == 0x00FF) {
                        if (t.ghost == 1) ++up_p;
                        if (t.ghost == 2) ++up_i;
                    }
                    if (t.ghost == 3) {
                        int dd = tcol(t.hl >> 8) - tcol(t.pd), de = trow(t.hl & 255) - trow(t.pe);
                        if (dd * dd + de * de >= 64) ++far_c;
                        else ++near_c;
                    }
                }
                if (up_p >= 3 && up_i >= 3 && far_c >= 3 && near_c >= 3) {
                    seed_used = seed;
                    break;
                }
            }
        }
        if (seed_used) break;
    }
    if (!seed_used) {
        say("  no scripted game in 80 seeds showed everything; using the last one and reporting what it showed\n");
        seed_used = 80;
    }
    say("  seed %u: %d frames, %zu target calls, %zu score events\n", seed_used, R->f, R->tgts.size(), R->scores.size());

    // ================================================================= survey run for H
    say("\nSurvey H: scripted games until one shows the ghost-house rules on the first life and after a death. Harness (first life only): once Pinky is out the counter "
        "4E10 is set\n"
        "  to 28 (and 4E11 to 58 once Inky is out), so the next two dots, counted by the ROM, reach the limits 30 and 60 without waiting for a perfect run\n");
    Run *H = nullptr;
    uint32_t hseed = 0;
    for (uint32_t seed = 1; seed <= 200 && !hseed; ++seed) {
        delete H;
        H = new Run(g_prog, seed);
        bool pi = false, pc = false;
        for (int i = 0; i < 14000; ++i) {
            if (H->playing() && H->rd(0x4E12) == 0 && H->rd(0x4E13) == 0) {
                if (!pi && H->rd(0x4DA1) != 0 && H->rd(0x4DA2) == 0) {
                    H->wr(0x4E10, 28);
                    pi = true;
                }
                if (!pc && H->rd(0x4DA2) != 0 && H->rd(0x4DA3) == 0) {
                    H->wr(0x4E11, 58);
                    pc = true;
                }
            }
            H->frame();
            if (H->rd(0x4E00) == 1 && H->f > 600) break;
        }
        bool okc[3] = {false, false, false};
        int gl[2] = {0, 0};
        for (auto &r : H->rels) {
            if (!r.died && r.path == 0 && r.level == 0) okc[r.ghost] = true;
            if (r.path == 1 && r.ghost < 2) ++gl[r.ghost];
        }
        if (okc[0] && okc[1] && okc[2] && gl[0] >= 1 && gl[1] >= 1) hseed = seed;
    }
    if (!hseed) say("  (no game in 200 seeds showed every release type; reporting the last one)\n");
    say("  seed %u: %d frames, %zu releases from the ghost house\n", hseed ? hseed : 200, H->f, H->rels.size());

    // ================================================================= T
    say("\nT. Ghost targets (calls of the survey T game and of the survey H game together)\n");
    std::vector<Tgt> allT = allT;
    allT.insert(allT.end(), H->tgts.begin(), H->tgts.end());
    {
        const int corner[4] = {0x221D, 0x391D, 0x2040, 0x3B40};
        const char *nm[4] = {"Blinky", "Pinky", "Inky", "Clyde"};
        int n_sc[4] = {0}, ok_sc[4] = {0}, n_ch[4] = {0}, ok_ch[4] = {0}, path_bad = 0;
        int up_n[4] = {0}, up_ok[4] = {0};
        int nd[4][4] = {{0}}, nok[4][4] = {{0}}; // [ghost][dir 0 right 1 down 2 left 3 up]
        int clyde_far = 0, clyde_far_ok = 0, clyde_near = 0, clyde_near_ok = 0, clyde_exact64 = 0;
        int blinky_elroy_scatter = 0, blinky_elroy_scatter_ok = 0;
        std::string firstbad;
        for (auto &t : allT) {
            bool scatter_ret = t.ret == 0x2751 || t.ret == 0x2787 || t.ret == 0x27C4 || (t.ret == 0x280C);
            bool scatter_phase = (t.phase & 1) == 0 && t.state == 3 && (t.ghost != 0 || t.elroy == 0);
            int pe = t.pe, pd = t.pd, v = t.vec;
            int dir = v == 0xFF00 ? 0 : v == 0x0001 ? 1 : v == 0x0100 ? 2 : v == 0x00FF ? 3 : -1;
            int exp = -1;
            if (t.ghost != 3) {
                if (scatter_ret != scatter_phase) {
                    ++path_bad;
                    continue;
                }
            }
            if (scatter_ret && t.ghost != 3) {
                exp = corner[t.ghost];
                ++n_sc[t.ghost];
                if (t.de == exp) ++ok_sc[t.ghost];
                else if (firstbad.empty()) firstbad = std::string(nm[t.ghost]) + " scatter";
                continue;
            }
            if (t.ghost == 0) {
                exp = pd << 8 | pe;
                ++n_ch[0];
                if (t.de == exp) ++ok_ch[0];
                else if (firstbad.empty()) firstbad = "Blinky chase";
                if (t.elroy && (t.phase & 1) == 0 && t.state == 3) {
                    ++blinky_elroy_scatter;
                    if (t.de == exp) ++blinky_elroy_scatter_ok;
                }
                continue;
            }
            if (t.ghost == 1 || t.ghost == 2) {
                if (dir < 0) continue;
                int ahead = t.ghost == 1 ? 4 : 2;
                int E = pe, D = pd; // the Dossier's rule in tile space
                if (dir == 0) D -= ahead;
                else if (dir == 2) D += ahead;
                else if (dir == 1) E += ahead;
                else {
                    E -= ahead;
                    D += ahead;
                } // up: also 'left' (overflow bug)
                if (t.ghost == 2) {
                    int be = t.bl & 255, bd = t.bl >> 8;
                    E = E + (E - be);
                    D = D + (D - bd);
                }
                exp = (D & 255) << 8 | (E & 255);
                ++n_ch[t.ghost];
                ++nd[t.ghost][dir];
                if (t.de == exp) {
                    ++ok_ch[t.ghost];
                    ++nok[t.ghost][dir];
                } else if (firstbad.empty()) firstbad = std::string(nm[t.ghost]) + " chase dir " + std::to_string(dir);
                continue;
            }
            if (t.ghost == 3) {
                int gd = tcol(t.hl >> 8), ge = trow(t.hl & 255);
                int dd = gd - tcol(pd), de = ge - trow(pe);
                int d2 = dd * dd + de * de;
                bool sc = scatter_phase || d2 < 64;
                exp = sc ? corner[3] : (pd << 8 | pe);
                if (d2 == 64) ++clyde_exact64;
                if (scatter_phase) {
                    ++n_sc[3];
                    if (t.de == exp) ++ok_sc[3];
                    continue;
                }
                if (d2 >= 64) {
                    ++clyde_far;
                    if (t.de == exp) ++clyde_far_ok;
                } else {
                    ++clyde_near;
                    if (t.de == exp) ++clyde_near_ok;
                }
                ++n_ch[3];
                if (t.de == exp) ++ok_ch[3];
                else if (firstbad.empty()) firstbad = "Clyde chase";
            }
        }
        (void)up_n;
        (void)up_ok;
        say("  scatter corners (ROM): Blinky %04X, Pinky %04X, Inky %04X, Clyde %04X as (D,E) = (59 - column, row + 32): Blinky column %d row %d, Pinky column %d row "
            "%d, Inky column %d row %d, Clyde column %d row %d\n",
            corner[0], corner[1], corner[2], corner[3], tcol(corner[0] >> 8), trow(corner[0] & 255), tcol(corner[1] >> 8), trow(corner[1] & 255), tcol(corner[2] >> 8),
            trow(corner[2] & 255), tcol(corner[3] >> 8), trow(corner[3] & 255));
        int minE = 255, maxE = 0;
        for (auto &t : allT) {
            if (t.pe < minE) minE = t.pe;
            if (t.pe > maxE) maxE = t.pe;
        }
        say("  Pac-Man's tile rows seen: %d to %d (row + 32)\n", minE, maxE);
        expect((corner[0] & 255) < minE && (corner[1] & 255) < minE && (corner[2] & 255) > maxE && (corner[3] & 255) > maxE,
               "all four corners lie outside the maze rows: Blinky and Pinky above, Inky and Clyde below (Dossier: dead space above/below the maze)");
        expect((corner[0] >> 8) < (corner[1] >> 8) && (corner[2] >> 8) < (corner[3] >> 8) && tcol(corner[0] >> 8) > 13 && tcol(corner[1] >> 8) < 14 &&
                   tcol(corner[2] >> 8) > 13 && tcol(corner[3] >> 8) < 14,
               "Blinky and Inky aim at the right half, Pinky and Clyde at the left half");
        for (int g = 0; g < 4; ++g)
            say("  %-6s scatter calls %d (target = corner in %d), chase calls %d (target as the rule in %d)\n", nm[g], n_sc[g], ok_sc[g], n_ch[g], ok_ch[g]);
        say("  scatter/chase path chosen wrongly (phase even and state 3 means corner; Blinky also needs Elroy off): %d calls\n", path_bad);
        expect(path_bad == 0, "every ghost uses its corner exactly in the scatter phases (and Blinky only while Elroy is off)");
        for (int g = 0; g < 4; ++g) {
            char b[160];
            std::snprintf(b, sizeof b, "%s's scatter target is always its corner (%d calls)", nm[g], n_sc[g]);
            expect(n_sc[g] >= 3 && ok_sc[g] == n_sc[g], b);
        }
        expect(n_ch[0] >= 50 && ok_ch[0] == n_ch[0], "Blinky chases Pac-Man's own tile on every call");
        say("  Blinky calls in a scatter phase with Elroy on (Dossier: Elroy ignores scatter): %d, all aimed at Pac-Man: %d\n", blinky_elroy_scatter,
            blinky_elroy_scatter_ok);
        const char *dn[4] = {"right", "down", "left", "up"};
        for (int g = 1; g <= 2; ++g) {
            say("  %s chase calls by Pac-Man's direction (calls / correct):", nm[g]);
            for (int d = 0; d < 4; ++d) say(" %s %d/%d", dn[d], nd[g][d], nok[g][d]);
            say("\n");
        }
        expect(n_ch[1] >= 50 && ok_ch[1] == n_ch[1], "Pinky aims 4 tiles ahead of Pac-Man (facing up: 4 up AND 4 left, the Dossier's overflow bug)");
        expect(nd[1][3] >= 3 && nok[1][3] == nd[1][3], "Pinky's facing-up case was seen at least 3 times and matches the bug");
        expect(n_ch[2] >= 20 && ok_ch[2] == n_ch[2], "Inky aims at twice the vector from Blinky to the point 2 ahead (facing up: 2 up AND 2 left)");
        expect(nd[2][3] >= 3 && nok[2][3] == nd[2][3], "Inky's facing-up case was seen at least 3 times and matches the bug");
        say("  Clyde chase calls: 8 or more tiles away %d (target Pac-Man in %d), nearer than 8 tiles %d (target his corner in %d), exactly 8 tiles (squared 64): %d\n",
            clyde_far, clyde_far_ok, clyde_near, clyde_near_ok, clyde_exact64);
        expect(clyde_far >= 3 && clyde_far_ok == clyde_far && clyde_near >= 3 && clyde_near_ok == clyde_near,
               "Clyde chases Pac-Man from 8 or more tiles away and goes to his corner when closer (Dossier)");
        if (!firstbad.empty()) say("  first mismatch: %s\n", firstbad.c_str());
    }

    // ================================================================= H
    say("\nH. Ghost-house release\n");
    {
        const char *nm[3] = {"Pinky", "Inky", "Clyde"};
        say("  release events in the survey game (ghost, frame, level, died-this-level flag, how it was released, counter vs limit, global counter, dots eaten):\n");
        for (auto &r : H->rels) {
            const char *how = r.path == 0 ? "personal counter" : r.path == 1 ? "global counter" : "inactivity timer";
            int c = r.ghost == 0 ? r.c0 : r.ghost == 1 ? r.c1 : r.c2, l = r.ghost == 0 ? r.lim0 : r.ghost == 1 ? r.lim1 : r.lim2;
            say("    %-5s frame %5d level %d died %d  %-16s counter %2d limit %2d  global %2d  dots %3d", nm[r.ghost], r.frame, r.level + 1, r.died, how, c, l, r.global,
                r.dots);
            if (r.path == 2) say("  timer %d of limit %d", r.timer, r.timer_limit);
            say("\n");
        }
        say("  ROM personal limits on level 1 (4DB8, 4DB9, 4DBA): ");
        {
            Run P(g_prog, 1);
            while (P.f < 620) P.frame();
            say("%d %d %d (Dossier: 0, 30, 60)\n", P.rd(0x4DB8), P.rd(0x4DB9), P.rd(0x4DBA));
            expect(P.rd(0x4DB8) == 0 && P.rd(0x4DB9) == 30 && P.rd(0x4DBA) == 60, "level-1 personal dot limits are 0, 30 and 60");
            say("  inactivity limit 4D95 on level 1: %d frames = %.2f s (Dossier: 4 s)\n", P.rd16(0x4D95), P.rd16(0x4D95) / 60.0);
            expect(P.rd16(0x4D95) == 240, "inactivity timer limit is 240 frames = 4 s");
        }
        int firstL1[3] = {0, 0, 0}, ok_first[3] = {0, 0, 0}, timer_n = 0, timer_ok = 0, g_ok = 0, g_n = 0, odd = 0;
        for (auto &r : H->rels) {
            int c = r.ghost == 0 ? r.c0 : r.ghost == 1 ? r.c1 : r.c2, l = r.ghost == 0 ? r.lim0 : r.ghost == 1 ? r.lim1 : r.lim2;
            if (r.path == 2) {
                ++timer_n;
                if (r.timer == r.timer_limit) ++timer_ok;
                continue;
            }
            if (r.died == 0 && r.level == 0 && r.path == 0) {
                ++firstL1[r.ghost];
                if (c == l) ++ok_first[r.ghost];
            } else if (r.path == 1) {
                ++g_n;
                int want = r.ghost == 0 ? 7 : 17;
                if (r.global == want) ++g_ok;
            } else ++odd;
        }
        say("  first life, level 1, released by the personal counter: Pinky %d (counter = limit in %d), Inky %d (in %d), Clyde %d (in %d)\n", firstL1[0], ok_first[0],
            firstL1[1], ok_first[1], firstL1[2], ok_first[2]);
        expect(firstL1[0] >= 1 && ok_first[0] == firstL1[0], "Pinky leaves as soon as the level starts (limit 0)");
        expect(firstL1[1] >= 1 && ok_first[1] == firstL1[1], "Inky leaves when 30 dots have been eaten since Pinky left (counter = limit = 30)");
        if (firstL1[2] >= 1) expect(ok_first[2] == firstL1[2], "Clyde leaves when 60 further dots have been eaten (counter = limit = 60)");
        else say("  Clyde was not released by his counter on the first life of level 1 in this game (not observed, no value to compare)\n");
        say("  after a death, released by the global counter: %d times, counter at release equal to 7 (Pinky) / 17 (Inky) in %d\n", g_n, g_ok);
        expect(g_n >= 2 && g_ok == g_n, "after a death Pinky leaves at global count 7 and Inky at 17 (Dossier)");
        say("  released by the inactivity timer: %d times, timer equalled its limit in %d (Dossier: forced out after 4 s without eating a dot)\n", timer_n, timer_ok);
        expect(timer_ok == timer_n, "every inactivity release happens exactly when the timer reaches its limit");
        say("  other releases (later levels or other rules): %d\n", odd);
        say("  ghost-house dot counting (1B08, once per dot): %d dots, counters moved by exactly one while a ghost waited (or the global counter ran) and not at all "
            "otherwise: %d\n",
            H->dots_counted, H->dots_count_ok);
        expect(H->dots_counted > 50 && H->dots_count_ok == H->dots_counted,
               "every dot adds exactly 1 to the counter of the first waiting ghost (or to the global counter after a death)");
    }

    // ================================================================= S
    say("\nS. Scoring\n");
    {
        int nd = 0, ok_d = 0, ne = 0, ok_e = 0;
        for (auto &s : H->scores) {
            if (s.tile == 0x10) {
                ++nd;
                if (s.delta == 10) ++ok_d;
            } else if (s.tile == 0x14) {
                ++ne;
                if (s.delta == 50) ++ok_e;
            }
        }
        say("  survey game: dots eaten %d (10 points each in %d), energizers %d (50 points each in %d)\n", nd, ok_d, ne, ok_e);
        expect(nd >= 50 && ok_d == nd, "a dot scores 10");
        (void)ne;
        (void)ok_e;
    }
    {
        say("  ghosts in a row (harness: an energizer tile is written under Pac-Man once so the ROM's own eating code runs; each ghost's tile and position are set to "
            "Pac-Man's)\n");
        bool done = false;
        for (uint32_t seed = 1; seed <= 40 && !done; ++seed) {
            Run G(g_prog, seed);
            if (!G.run_until_playing() || G.rd(0x4DA6) != 0) continue;
            G.force_energizer = true;
            int k = 0;
            while (G.rd(0x4DA6) == 0 && k++ < 60) G.frame();
            if (G.rd(0x4DA6) == 0 || !G.playing()) continue;
            size_t e0 = G.scores.size();
            int e_delta = -1, e_tile = -1;
            for (auto &sc : G.scores)
                if (sc.tile == 0x14) {
                    e_delta = sc.delta;
                    e_tile = sc.tile;
                }
            say("  energizer eaten by the ROM's own code (harness wrote the tile): score +%d (tile code %02X)\n", e_delta, e_tile);
            expect(e_delta == 50, "an energizer scores 50");
            std::vector<int> seq;
            std::vector<int> cnt;
            bool good = true;
            for (int gi = 0; gi < 4 && good; ++gi) {
                if (G.rd(0x4DA7 + gi) != 1) {
                    good = false;
                    break;
                }
                size_t before = G.scores.size();
                G.wr(0x4D0A + 2 * gi, G.rd(0x4D39));
                G.wr(0x4D0B + 2 * gi, G.rd(0x4D3A));
                G.wr(0x4D00 + 2 * gi, G.rd(0x4D08));
                G.wr(0x4D01 + 2 * gi, G.rd(0x4D09));
                for (int j = 0; j < 40 && G.scores.size() == before; ++j) G.frame();
                if (G.scores.size() == before) {
                    good = false;
                    break;
                }
                seq.push_back(G.scores.back().delta);
                cnt.push_back(G.rd(0x4DD0));
                for (int j = 0; j < 150; ++j) G.frame();
                if (G.rd(0x4DA6) == 0 && gi < 3) {
                    good = false;
                    break;
                }
            }
            if (!good || seq.size() != 4) continue;
            (void)e0;
            say("  seed %u: energizer eaten (score event 50), then ghosts eaten in order Blinky, Pinky, Inky, Clyde scored %d, %d, %d, %d; kill counter 4DD0 after each: "
                "%d %d %d %d\n",
                seed, seq[0], seq[1], seq[2], seq[3], cnt[0], cnt[1], cnt[2], cnt[3]);
            expect(seq[0] == 200 && seq[1] == 400 && seq[2] == 800 && seq[3] == 1600, "ghosts score 200, 400, 800, 1600 in a row (Dossier)");
            // next energizer: back to 200
            int k2 = 0;
            while (G.rd(0x4DA6) != 0 && k2++ < 800) G.frame();
            if (G.rd(0x4DA6) == 0 && G.playing()) {
                G.force_energizer = true;
                k2 = 0;
                while (G.rd(0x4DA6) == 0 && k2++ < 60) G.frame();
                int d = -1, who = -1;
                for (int gi = 0; gi < 4 && d < 0 && G.rd(0x4DA6); ++gi) {
                    if (G.rd(0x4DA7 + gi) != 1) continue;
                    size_t before = G.scores.size();
                    for (int j = 0; j < 12 && G.scores.size() == before; ++j) {
                        G.wr(0x4D0A + 2 * gi, G.rd(0x4D39));
                        G.wr(0x4D0B + 2 * gi, G.rd(0x4D3A));
                        G.wr(0x4D00 + 2 * gi, G.rd(0x4D08));
                        G.wr(0x4D01 + 2 * gi, G.rd(0x4D09));
                        G.frame();
                    }
                    if (G.scores.size() > before) {
                        d = G.scores.back().delta;
                        who = gi;
                    }
                }
                if (G.rd(0x4DA6) || d >= 0) {
                    say("  the next energizer: first ghost eaten (ghost %d) scored %d (Dossier: the sequence restarts at 200)\n", who, d);
                    expect(d == 200, "a new energizer restarts the ghost score at 200");
                } else say("  (second energizer: no edible ghost available in this game, not measured)\n");
            } else say("  (the pill did not run out in time for a second energizer, not measured)\n");
            done = true;
        }
        expect(done, "ghost-eating run completed");
    }
    {
        bool done = false;
        for (uint32_t seed = 1; seed <= 60 && !done; ++seed) {
            Run F(g_prog, seed);
            if (!F.run_until_playing()) continue;
            F.wr(0x4E0E, 68); // harness: dots eaten set to 68
            int k = 0;
            while (F.rd(0x4E0E) < 70 && k++ < 1200 && F.rd(0x4E00) == 3) F.frame();
            if (F.rd(0x4E0E) != 70) continue;
            for (int j = 0; j < 3; ++j) F.frame();
            if (!(F.rd(0x4DD2) == 0x94 && F.rd(0x4DD3) == 0x80 && F.rd(0x4DD4) != 0) || !F.playing()) continue;
            size_t before = F.scores.size();
            int s0 = F.score();
            F.wr(0x4D08, 0x94);
            F.wr(0x4D09, 0x80); // harness: Pac-Man put on the fruit's pixel position 8094
            for (int j = 0; j < 20 && F.scores.size() == before; ++j) F.frame();
            if (F.scores.size() == before) continue;
            int d = F.scores.back().delta;
            say("  fruit: level 1 cherry on screen at 8094; Pac-Man put on its position (harness); score went %d -> %d (+%d); fruit position afterwards %04X\n", s0,
                F.score(), d, F.rd16(0x4DD2));
            expect(d == 100 && F.rd16(0x4DD2) == 0, "the level-1 cherry scores 100 and disappears (Dossier / ROM table 2B17, S12)");
            done = true;
        }
        expect(done, "fruit-eating run completed");
    }

    // ================================================================= E
    say("\nE. Cruise Elroy (Blinky's speed-up), level 1\n");
    {
        Run Q(g_prog, 2);
        Q.run_until_playing();
        say("  ROM thresholds (4DBB, 4DBC): %d and %d dots left; speeds from the S12 table: 80 %% and 85 %% (Dossier: 20 and 10 dots left, 80 %% and 85 %%)\n",
            Q.rd(0x4DBB), Q.rd(0x4DBC));
        expect(Q.rd(0x4DBB) == 20 && Q.rd(0x4DBC) == 10, "Elroy 1 at 20 dots left, Elroy 2 at 10 dots left");
        // thresholds, with Clyde out (harness: 4DA3 = 3) and the dots-eaten counter held (harness: 4E0E written before every frame)
        struct Pt {
            int eaten;
        } pts[6] = {{222}, {223}, {224}, {233}, {234}, {235}};
        say("  harness: Clyde set 'out' (4DA3 = 3) and the dots-eaten counter held at each value; flags 4DB6 (Elroy 1) / 4DB7 (Elroy 2):\n");
        bool thr_ok = true;
        const int want1[6] = {0, 0, 1, 1, 1, 1}, want2[6] = {0, 0, 0, 0, 1, 1};
        for (int i = 0; i < 6; ++i) {
            for (int k = 0; k < 6; ++k) {
                Q.wr(0x4DA3, 3);
                Q.wr(0x4E0E, pts[i].eaten);
                Q.frame();
            }
            int a = Q.rd(0x4DB6), b = Q.rd(0x4DB7);
            say("    eaten %d (%d left): Elroy 1 = %d, Elroy 2 = %d\n", pts[i].eaten, 244 - pts[i].eaten, a, b);
            if (a != want1[i] || b != want2[i]) thr_ok = false;
        }
        expect(thr_ok, "Elroy 1 switches on at 20 dots left and Elroy 2 at 10 dots left, not before");
    }
    {
        // Clyde in the house: no Elroy, even with 8 dots left (fresh game, harness)
        Run Q(g_prog, 2);
        Q.run_until_playing();
        bool inhouse_ok = true;
        int a = 0, b = 0;
        for (int k = 0; k < 8; ++k) {
            Q.wr(0x4DA3, 0);
            Q.wr(0x4E0E, 236);
            Q.frame();
            a = Q.rd(0x4DB6);
            b = Q.rd(0x4DB7);
            if (a || b) inhouse_ok = false;
        }
        say("  harness: Clyde in the house (4DA3 = 0), 8 dots left: Elroy flags %d / %d; then Clyde out: ", a, b);
        for (int k = 0; k < 8; ++k) {
            Q.wr(0x4DA3, 3);
            Q.wr(0x4E0E, 236);
            Q.frame();
        }
        say("%d / %d\n", Q.rd(0x4DB6), Q.rd(0x4DB7));
        expect(inhouse_ok && Q.rd(0x4DB6) == 1 && Q.rd(0x4DB7) == 1,
               "Elroy stays off while Clyde is in the house and starts as soon as he is out (Dossier: suspended after a death until Clyde leaves)");
    }
    {
        // Elroy ignores scatter: harness holds Elroy 1 (dots 226, Clyde out), puts the mode timer in a scatter phase (phase 2, counter 1700), reads Blinky's target calls
        bool done = false;
        for (uint32_t seed = 1; seed <= 20 && !done; ++seed) {
            Run Q(g_prog, seed);
            Q.run_until_playing();
            Q.wr(0x4DC1, 2);
            Q.wr(0x4DC2, 1700 & 255);
            Q.wr(0x4DC3, 1700 >> 8);
            int n = 0, ok = 0, ph_ok = 0;
            for (int k = 0; k < 400 && Q.rd(0x4E00) == 3; ++k) {
                Q.wr(0x4DA3, 3);
                Q.wr(0x4E0E, 226);
                size_t t0 = Q.tgts.size();
                Q.frame();
                for (size_t t = t0; t < Q.tgts.size(); ++t) {
                    auto &g = Q.tgts[t];
                    if (g.ghost != 0 || (g.phase & 1) || g.state != 3 || !g.elroy) continue;
                    ++n;
                    if (g.ret == 0x2765) ++ph_ok;
                    if (g.de == (g.pd << 8 | g.pe)) ++ok;
                }
            }
            if (n < 5) continue;
            say("  Elroy 1 on, mode timer in a scatter phase (harness): Blinky made %d target calls, %d used the chase rule (return address 2765), %d aimed at Pac-Man's "
                "tile (Dossier: Elroy ignores scatter)\n",
                n, ph_ok, ok);
            expect(ph_ok == n && ok == n, "Blinky with Elroy on keeps chasing Pac-Man during scatter phases");
            done = true;
        }
        if (!done) expect(false, "Elroy scatter run completed");
    }
    {
        // speeds: dots held so Elroy 1 / Elroy 2 / no Elroy stay on; Blinky's speed pattern is read at the moment it is used (the ROM's own tick), and the pixels
        // moved are recorded as a cross-check
        double ps[3] = {0, 0, 0}, px[3] = {0, 0, 0};
        long cn[3] = {0, 0, 0};
        const int held[3] = {226, 236, 100};
        for (int mode = 0; mode < 3; ++mode) {
            Run Q(g_prog, 2);
            Q.run_until_playing();
            for (int k = 0; k < 3000; ++k) {
                Q.wr(0x4DA3, 3);
                Q.wr(0x4E0E, held[mode]);
                Q.frame();
                if (Q.rd(0x4E00) != 3) break;
            }
            long c = 0, e = 0;
            double sum = 0;
            long n = 0;
            for (size_t i = 1; i < Q.log.size(); ++i) {
                auto &l = Q.log[i], &p = Q.log[i - 1];
                if (l.f < 900 || l.sub != 3 || l.state != 3 || l.dying || p.dying || l.pill || p.pill || l.c1806 != 2) continue;
                bool flags_ok = mode == 0 ? (l.elroy1 && !l.elroy2) : mode == 1 ? (l.elroy2 != 0) : (!l.elroy1 && !l.elroy2);
                if (!flags_ok) continue;
                c += l.c88;
                e += l.e88;
                int dy = std::abs(l.bl_y - p.bl_y), dx = std::abs(l.bl_x - p.bl_x);
                if (dy + dx <= 2) {
                    sum += dx + dy;
                    ++n;
                }
            }
            ps[mode] = c ? 2.0 * e / c : 0;
            cn[mode] = c;
            px[mode] = n ? sum / n : 0;
        }
        const char *mn[3] = {"Elroy 1 (dots held at 226 eaten = 18 left)", "Elroy 2 (held at 236 = 8 left)", "no Elroy (held at 100)"};
        const double wantv[3] = {1.0, 1.0625, 0.9375};
        const int wantp[3] = {80, 85, 75};
        for (int m = 0; m < 3; ++m) {
            say("  Blinky, %s: speed pattern fires %.4f steps per frame = %.1f %% (Dossier %d %% = %.4f), measured over %ld ticks; pixels per frame in the same frames "
                "%.4f (includes tunnel slow-down and corners)\n",
                mn[m], ps[m], ps[m] / 1.25 * 100, wantp[m], wantv[m], cn[m], px[m]);
            char b[160];
            std::snprintf(b, sizeof b, "Blinky's speed with %s is %d %%", m == 0 ? "Elroy 1" : m == 1 ? "Elroy 2" : "no Elroy", wantp[m]);
            expect(cn[m] > 400 && std::fabs(ps[m] - wantv[m]) < 0.02, b);
        }
    }

    // ================================================================= X
    say("\nX. Extra life\n");
    {
        struct Cfg {
            DipSettings::Bonus b;
            int want71;
            int limit;
            const char *name;
        } cfgs[4] = {{DipSettings::At10000, 0x10, 10000, "10,000 (default)"},
                     {DipSettings::At15000, 0x15, 15000, "15,000"},
                     {DipSettings::At20000, 0x20, 20000, "20,000"},
                     {DipSettings::NoBonus, 0xFF, 0, "none"}};
        for (auto &c : cfgs) {
            bool done = false;
            for (uint32_t seed = 1; seed <= 30 && !done; ++seed) {
                Run X(g_prog, seed);
                DipSettings d;
                d.bonus = c.b;
                X.in.set_dip(d);
                if (!X.run_until_playing()) continue;
                int lives0 = X.rd(0x4E14), disp0 = X.rd(0x4E15);
                size_t e0 = X.extra_frames.size();
                if (c.limit) {
                    X.set_score(c.limit - 20);
                } // harness: score set 20 points under the limit
                else
                    X.set_score(9980);
                int k = 0;
                while (X.scores.empty() || X.score() < (c.limit ? c.limit : 10000)) {
                    X.frame();
                    if (++k > 3000 || X.rd(0x4E00) != 3) break;
                }
                if (X.rd(0x4E00) != 3 || X.rd(0x4E14) < lives0) continue; // died on the way: try another seed
                for (int j = 0; j < 5; ++j) X.frame();
                size_t ev = X.extra_frames.size() - e0;
                int lives1 = X.rd(0x4E14), disp1 = X.rd(0x4E15);
                // second crossing: push the score on to 2x the limit, no further life
                X.set_score(c.limit ? 2 * c.limit - 20 : 99980);
                int kk = 0;
                size_t sc0 = X.scores.size();
                while (X.scores.size() < sc0 + 2 && kk++ < 3000 && X.rd(0x4E00) == 3) X.frame();
                size_t ev2 = X.extra_frames.size() - e0;
                int lives2 = X.rd(0x4E14);
                say("  DIP bonus %s: ROM limit 4E71 = %02X; lives %d -> %d (displayed %d -> %d) after crossing the limit; extra-life routine (2B33) entered %zu times "
                    "after the first crossing (it returns at once once the once-only flag is set); after a second crossing: lives %d, entered %zu times in all\n",
                    c.name, X.rd(0x4E71), lives0, lives1, disp0, disp1, ev, lives2, ev2);
                bool ok = X.rd(0x4E71) == c.want71 && (c.limit ? (ev >= 1 && lives1 == lives0 + 1 && disp1 == disp0 + 1 && lives2 == lives1)
                                                               : (ev == 0 && lives1 == lives0 && ev2 == 0 && lives2 == lives0));
                char b[160];
                std::snprintf(b, sizeof b, c.limit ? "extra life at %s, once only" : "no extra life with bonus %s", c.name);
                expect(ok, b);
                done = true;
            }
            if (!done) expect(false, "extra-life run completed");
        }
    }

    // ================================================================= L
    say("\nL. Lives\n");
    {
        const DipSettings::Lives opts[4] = {DipSettings::One, DipSettings::Two, DipSettings::Three, DipSettings::Five};
        const int nl[4] = {1, 2, 3, 5};
        for (int o = 0; o < 4; ++o) {
            bool done = false;
            for (uint32_t seed = 1; seed <= 30 && !done; ++seed) {
                Run L(g_prog, seed);
                DipSettings d;
                d.lives = opts[o];
                L.in.set_dip(d);
                bool started = false;
                int lives = -1, start_lives = -1, disp0 = -1, deaths = 0, bad_step = 0;
                int start_state = 0;
                bool over = false;
                int over_frame = 0;
                for (int i = 0; i < 40000; ++i) {
                    L.frame();
                    if (!started && L.rd(0x4E00) == 3 && L.rd(0x4E04) == 3 && L.f > 700) {
                        started = true;
                        lives = start_lives = L.rd(0x4E14);
                        disp0 = L.rd(0x4E15);
                        start_state = 3;
                    }
                    if (started) {
                        int v = L.rd(0x4E14);
                        if (v < lives) {
                            if (lives - v != 1) ++bad_step;
                            ++deaths;
                            lives = v;
                        }
                        if (v > lives) lives = v;
                        if (L.rd(0x4E00) != 3) {
                            over = true;
                            over_frame = L.f;
                            break;
                        }
                    }
                }
                if (!started || !over) continue;
                size_t extras = L.extra_frames.size();
                say("  DIP lives %d: game starts with 4E14 = %d (icons shown %d); deaths counted %d, extra lives won %zu, game over (state %d) at frame %d\n", nl[o],
                    start_lives, disp0, deaths, extras, L.rd(0x4E00), over_frame);
                (void)start_state;
                char b[200];
                std::snprintf(b, sizeof b, "with %d lives: start value %d, one life lost per death (%d deaths + %zu extra), game over after the last", nl[o], nl[o],
                              deaths, extras);
                expect(start_lives == nl[o] && disp0 == nl[o] - 1 && bad_step == 0 && deaths == nl[o] + (int)extras, b);
                done = true;
            }
            if (!done) expect(false, "lives run completed");
        }
    }

    say("\n%d checks, %d failed. %s\n", checks, failures, failures ? "FAIL" : "PASS");
    if (g_out) std::fclose(g_out);
    return failures ? 1 : 0;
}
