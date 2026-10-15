// S30A: behaviour checks on high levels (5, 21 and 256), in the style of S28/S29. Measured on the C++ machine from RAM and from the program counter;
// expected values come from tests/data/level_expect.txt and level_modes.txt (the ROM-decoded tables of S12, made by tools/analysis/make_level_expect.py;
// the speeds and times in them agree with the Dossier, see docs/notes/table_comparison.md) and from the Dossier's ghost rules.
//
// Usage: test_high_levels pacman.zip [out_dir]       (writes S30A_high_levels.txt in out_dir, default docs/evidence)
// Build: see tests/test_level256_full.sh.
//
// HOW EACH LEVEL IS REACHED (natural progression with one shortcut): a scripted game is started and the ROM itself ends every level from 1 up to L-1; the harness
// only sets the dots-eaten counter 4E0E = 243 once per level so that one more dot ends it (otherwise this would take hours) and tops up lives. Poking the level
// counter 4E13 alone is NOT enough: the speed tables follow a pointer (4E0A) that moves one record per finished level (see the note in reach()). Other harness writes are marked 'harness': lives topped up, ghosts released (4DA1-3), the mode counter moved
// forward once (time jump), the dots-eaten counter held (Elroy thresholds), an energizer put under Pac-Man (as in S29).
//
// Measured per level:
//   A  the parameter table in RAM (speeds, fright time, house limits, Elroy dots) equals the expected row; the mode timer table at 4D86 equals the expected one
//   B  the first scatter-to-chase change comes exactly when the table says (no time jump)
//   C  Pac-Man's speed (no power pill; with a power pill where the level has one) and Blinky's speed without Elroy
//   D  power pill length against the fright time; on levels without fright time the pill ends at once
//   E  Cruise Elroy switches on at the level's dot counts (not before)
//   F  ghost targets on every call: corners in scatter, Blinky = Pac-Man, Pinky 4 ahead, Inky via Blinky, Clyde 8-tile rule, with the up-direction overflow bug
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
    uint32_t rng;
    int hold = 0;
    Control dir = Control::P1Left;
    explicit Player(uint32_t s) : rng(s * 2654435761u + 12345u) {}
    void apply(Inputs &in, int f) {
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
    }
};

struct Tgt {
    int ghost, ret, de, hl, phase, state, pe, pd, vec, bl, elroy;
};
struct Fl {
    int f, state, sub, pill, dying, c1806, c1806_free, c1843, c88, e88, phase, counter, dots, bl_y, bl_x, elroy1, elroy2;
};

struct Run {
    Machine m;
    Inputs in;
    Player player;
    int f = 0;
    int c1806 = 0, c1806_free = 0, c1843 = 0, c88 = 0, e88 = 0, inc_since = 0;
    bool in88 = false;
    bool force_energizer = false;
    int forced = 0;
    std::vector<Tgt> tgts;
    std::vector<Fl> log;
    std::vector<int> change_inc, change_counter, change_frames;
    explicit Run(const std::vector<uint8_t> &prog, uint32_t seed) : m(prog), player(seed) {}
    int rd(int a) { return m.bus.read((uint16_t)a); }
    int rd16(int a) { return rd(a) | rd(a + 1) << 8; }
    void wr(int a, int v) { m.bus.write((uint16_t)a, (uint8_t)v); }
    void hook(unsigned pc) {
        switch (pc) {
        case 0x0E42:
            if (rd16(0x4DC2) == 0) inc_since = 0;
            ++inc_since;
            break; // the mode counter is about to be incremented
        case 0x0E5C:
            change_inc.push_back(inc_since);
            change_counter.push_back(rd16(0x4DC2));
            change_frames.push_back(f + 1);
            inc_since = 0;
            break; // phase moves on
        case 0x1806:
            ++c1806;
            if (rd(0x4D9D) == 0xFF) ++c1806_free;
            break;
        case 0x1843: ++c1843; break;
        case 0x1B36: in88 = false; break;
        case 0x1B88:
            ++c88;
            in88 = true;
            break;
        case 0x1BD8:
            if (in88) ++e88;
            break;
        case 0x19D8:
            if (force_energizer) {
                m.bus.write((uint16_t)m.on_get_hl(), 0x14);
                force_energizer = false;
                ++forced;
            }
            break;
        case 0x2966: {
            unsigned sp = m.on_get_sp();
            int ret = rd(sp) | rd(sp + 1) << 8;
            int g = -1;
            if (ret == 0x2751 || ret == 0x2765) g = 0;
            else if (ret == 0x2787 || ret == 0x27A2) g = 1;
            else if (ret == 0x27C4 || ret == 0x27EA) g = 2;
            else if (ret == 0x280C || ret == 0x2834) g = 3;
            if (g >= 0)
                tgts.push_back({g, ret, (int)m.on_get_de(), (int)m.on_get_hl(), rd(0x4DC1), rd(0x4E04), rd(0x4D39), rd(0x4D3A), rd16(0x4D1C), rd16(0x4D0A), rd(0x4DB6)});
            break;
        }
        default: break;
        }
    }
    void frame(bool instrumented = true) {
        player.apply(in, f);
        in.apply(m.bus);
        c1806 = c1806_free = c1843 = c88 = e88 = 0;
        if (!instrumented) {
            m.run_frame();
            ++f;
            return;
        } // same machine step as run_frame; no hooks and no log (fast climb to the level)
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
        log.push_back({f, rd(0x4E00), rd(0x4E04), rd(0x4DA6), rd(0x4DA5), c1806, c1806_free, c1843, c88, e88, rd(0x4DC1), rd16(0x4DC2), rd(0x4E0E), rd(0x4D00),
                       rd(0x4D01), rd(0x4DB6), rd(0x4DB7)});
    }
    bool playing() { return rd(0x4E00) == 3 && rd(0x4E04) == 3 && rd(0x4DA5) == 0; }
    void topup() {
        if (playing() && rd(0x4E14) < 2) {
            wr(0x4E14, 3);
            wr(0x4E15, 2);
        }
    } // harness: lives
    // NATURAL progression with the dots-eaten shortcut: the ROM itself ends every level from 1 to L-1 (the counter 4E0E is set to 243 once per level while
    // playing) and builds level L with its own code. Writing only the level counter 4E13 would NOT do: the speed and time tables follow a pointer (4E0A)
    // that moves one record per finished level, so a poked counter gives level-2 tables on a level-256 maze (found in this step).
    bool reach(int L) {
        while (f < 900 || !playing()) {
            frame(false);
            if (f > 4000) return false;
        }
        int poked = -1, guard = 0;
        while (!(rd(0x4E00) == 3 && rd(0x4E13) == L - 1 && playing())) {
            topup();
            if (playing() && rd(0x4E13) < L - 1 && poked != rd(0x4E13) && rd(0x4E0E) < 200) {
                wr(0x4E0E, 243);
                poked = rd(0x4E13);
            }
            frame(false);
            if (++guard > 400000) return false;
        }
        return true;
    }
};

static std::vector<uint8_t> g_prog;
static int E[22][14], M[22][8];
static int tcol(int d) { return 59 - d; }
static int trow(int e) { return e - 32; }

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_high_levels pacman.zip [out_dir]\n");
        return 2;
    }
    RomSet roms;
    std::string err;
    if (!load_romset_from_file(argv[1], roms, err)) {
        std::printf("ROM load failed: %s\n", err.c_str());
        return 2;
    }
    g_prog = roms.program;
    std::string dir = argc >= 3 ? argv[2] : "docs/evidence";
    g_out = std::fopen((dir + "/S30A_high_levels.txt").c_str(), "w");
    FILE *ex = std::fopen("tests/data/level_expect.txt", "r"), *mo = std::fopen("tests/data/level_modes.txt", "r");
    if (!ex || !mo) {
        std::printf("tests/data/level_expect.txt or level_modes.txt missing (run tools/analysis/make_level_expect.py)\n");
        return 2;
    }
    for (int i = 1; i <= 21; ++i) {
        for (int j = 0; j < 14; ++j)
            if (std::fscanf(ex, "%d", &E[i][j]) != 1) return 2;
        for (int j = 0; j < 8; ++j)
            if (std::fscanf(mo, "%d", &M[i][j]) != 1) return 2;
    }
    std::fclose(ex);
    std::fclose(mo);
    say("S30A high-level behaviour tests (levels 5, 21 and 256). The game code is not changed. Each level is reached by the ROM's own progression (one dots-eaten "
        "shortcut per level); other writes are marked 'harness'.\n");
    const int levels[3] = {5, 21, 256};
    char b[400];
    for (int li = 0; li < 3; ++li) {
        int L = levels[li], idx = L > 21 ? 21 : L;
        const int *e = E[idx];
        say("\n================ level %d (counter %d, expected table = level %d of the ROM table) ================\n", L, L - 1, idx);
        Run R(g_prog, 1); // one scripted game per level: reach the level naturally, then run the sections one after another on it
        bool reached = R.reach(L);
        expect(reached, "the ROM reached this level by itself (dots-eaten shortcut once per level)");
        if (!reached) continue;
        const int cnt = L - 1;
        auto stay = [&]() { return R.rd(0x4E13) == cnt; }; // the level must not change while measuring

        // ---------------------------------------------------------------- A: tables
        say("\nA. Parameter tables\n");
        for (int k = 0; k < 10; ++k) {
            R.topup();
            R.frame();
        }
        {
            int got[14] = {L, 0};
            auto speed = [&](int a) {
                unsigned v = (unsigned)R.rd(a) | (unsigned)R.rd(a + 1) << 8 | (unsigned)R.rd(a + 2) << 16 | (unsigned)R.rd(a + 3) << 24;
                return __builtin_popcount(v) * 5;
            };
            got[1] = speed(0x4D46);
            got[2] = speed(0x4D4A);
            got[3] = speed(0x4D4E);
            got[4] = speed(0x4D52);
            got[5] = speed(0x4D56);
            got[6] = speed(0x4D5A);
            got[7] = speed(0x4D5E);
            got[8] = R.rd16(0x4DBD);
            got[9] = R.rd(0x4DB8);
            got[10] = R.rd(0x4DB9);
            got[11] = R.rd(0x4DBA);
            got[12] = R.rd(0x4DBB);
            got[13] = R.rd(0x4DBC);
            bool ok = true;
            for (int j = 1; j < 14; ++j) {
                if ((j == 2 || j == 6) && e[j] == -1) continue;
                if (j == 8 && e[j] == 0 && got[j] <= 1) continue;
                if (got[j] != e[j]) ok = false;
            }
            say("  pac %d, pac-fright %d, elroy2 %d, elroy1 %d, ghost %d, ghost-fright %d, tunnel %d %%, fright word %d (= %d s), house %d/%d/%d, Elroy dots %d/%d\n",
                got[1], got[2], got[3], got[4], got[5], got[6], got[7], got[8], got[8] / 120, got[9], got[10], got[11], got[12], got[13]);
            expect(ok, "speeds, fright time, ghost-house limits and Elroy dot counts equal the ROM table for this level");
            int tbl[7];
            for (int k = 0; k < 7; ++k) tbl[k] = R.rd16(0x4D86 + 2 * k);
            bool tok = true;
            for (int k = 0; k < 7; ++k)
                if (tbl[k] != M[idx][k + 1]) tok = false;
            say("  mode timer table 4D86: %d %d %d %d %d %d %d (cumulative frames; 65534/65535 = chase for ever)\n", tbl[0], tbl[1], tbl[2], tbl[3], tbl[4], tbl[5],
                tbl[6]);
            expect(tok, "the scatter/chase schedule in RAM equals the table of this level's speed record (Dossier: 5 s scatter, 20 s chase on levels 5 and up)");
            // B: the first scatter phase in real time (no time jump): exactly tbl[0] ticks of the mode counter, one tick per frame
            R.change_inc.clear();
            R.change_counter.clear();
            R.change_frames.clear();
            int g = 0;
            while (R.change_inc.empty() && g++ < tbl[0] + 3000 && stay()) {
                R.topup();
                R.frame();
            }
            if (!R.change_inc.empty()) {
                say("  first phase change: mode counter %d, after %d ticks (table says %d)\n", R.change_counter[0], R.change_inc[0], tbl[0]);
                expect(R.change_counter[0] == tbl[0] && R.change_inc[0] == tbl[0],
                       "the first scatter phase ends after exactly the tabled number of frames (no time jump)");
            } else expect(false, "first phase change seen");
        }

        // ---------------------------------------------------------------- C and D: speeds, power pill
        say("\nC/D. Speeds and power pill (harness: ghosts released, energizers put under Pac-Man)\n");
        {
            size_t log0 = R.log.size();
            int pill_windows = 0, pill_frames_max = 0, cur = 0, pills_forced = 0;
            bool prev_pill = false;
            std::vector<int> pill_lens;
            for (int k = 0; k < 7000 && stay(); ++k) {
                R.topup();
                if (R.playing()) {
                    if (!R.rd(0x4DA1)) R.wr(0x4DA1, 2);
                    if (!R.rd(0x4DA2)) R.wr(0x4DA2, 3);
                    if (!R.rd(0x4DA3)) R.wr(0x4DA3, 3);
                }
                if (R.playing() && !R.force_energizer && k % 600 == 100 && R.rd(0x4E0E) < 150) {
                    R.force_energizer = true;
                    ++pills_forced;
                }
                R.frame();
                bool pill = R.rd(0x4DA6) != 0;
                if (pill) ++cur;
                else {
                    if (prev_pill) {
                        ++pill_windows;
                        pill_lens.push_back(cur);
                        if (cur > pill_frames_max) pill_frames_max = cur;
                    }
                    cur = 0;
                }
                prev_pill = pill;
                if (R.rd(0x4E00) == 1) break;
            }
            expect(stay(), "the level did not change during the measurement");
            long ev_n = 0, calls_n = 0, ev_p = 0, calls_p = 0, c88 = 0, e88 = 0;
            for (size_t i = log0 + 1; i < R.log.size(); ++i) {
                const Fl &l = R.log[i], &p = R.log[i - 1];
                if (l.sub != 3 || l.state != 3 || p.sub != 3 || l.dying || p.dying) continue;
                if (l.pill) {
                    ev_p += l.c1843;
                    calls_p += l.c1806_free;
                } else {
                    ev_n += l.c1843;
                    calls_n += l.c1806_free;
                }
                if (!l.pill && !p.pill && l.dots < 100 && !l.elroy1 && !l.elroy2 && l.c1806 == 2) {
                    c88 += l.c88;
                    e88 += l.e88;
                }
            }
            double pm = calls_n ? 2.0 * ev_n / calls_n : 0, pp = calls_p ? 2.0 * ev_p / calls_p : 0, bl = c88 ? 2.0 * e88 / c88 : 0;
            double wp = e[1] * 0.0125, wb = e[5] * 0.0125;
            std::snprintf(b, sizeof b, "Pac-Man without a power pill: %.4f steps per frame = %.1f %% over %ld ticks (table %d %% = %.4f)", pm, pm / 1.25 * 100, calls_n,
                          e[1], wp);
            say("  %s\n", b);
            expect(calls_n > 1000 && std::fabs(pm - wp) < 0.03, b);
            std::snprintf(b, sizeof b, "Blinky without Elroy and without a power pill: %.4f steps per frame = %.1f %% over %ld ticks (table %d %% = %.4f)", bl,
                          bl / 1.25 * 100, c88, e[5], wb);
            say("  %s\n", b);
            expect(c88 > 400 && std::fabs(bl - wb) < 0.03, b);
            say("  power pills forced (harness) %d, pill windows seen %d, longest %d frames, fright word %d = %d frames\n", pills_forced, pill_windows, pill_frames_max,
                e[8], e[8] / 2);
            if (e[8] > 1) {
                double wpp = e[2] * 0.0125;
                std::snprintf(b, sizeof b, "Pac-Man with a power pill: %.4f steps per frame = %.1f %% over %ld ticks (table %d %% = %.4f)", pp, pp / 1.25 * 100, calls_p,
                              e[2], wpp);
                say("  %s\n", b);
                expect(calls_p > 100 && std::fabs(pp - wpp) < 0.04, b);
                int clean = 0;
                std::string lens;
                for (int v : pill_lens) {
                    lens += std::to_string(v) + " ";
                    if (std::abs(v - e[8] / 2) <= 2) ++clean;
                }
                say("  pill windows (frames): %s(a window longer than the fright time = a second energizer eaten before the first ended; shorter = Pac-Man died)\n",
                    lens.c_str());
                std::snprintf(b, sizeof b, "at least one power pill ran its full tabled fright time (%d frames, +/- 2): %d of %d windows did", e[8] / 2, clean,
                              pill_windows);
                expect(clean >= 1, b);
            } else {
                std::snprintf(b, sizeof b, "no fright time on this level: the power-pill flag lasts at most 2 frames (longest %d, %d windows, %d energizers forced)",
                              pill_frames_max, pill_windows, pills_forced);
                expect(pills_forced >= 3 && pill_frames_max <= 2, b);
            }
        }

        // ---------------------------------------------------------------- F: ghost targets
        say("\nF. Ghost targets (harness: ghosts released; 100 frames before the end of each scatter phase the mode counter is moved to the end of it)\n");
        {
            const int corner[4] = {0x221D, 0x391D, 0x2040, 0x3B40};
            size_t t0n = R.tgts.size();
            for (int i = 0; i < 6000 && stay(); ++i) {
                R.topup();
                if (R.playing()) {
                    if (!R.rd(0x4DA1)) R.wr(0x4DA1, 2);
                    if (!R.rd(0x4DA2)) R.wr(0x4DA2, 3);
                    if (!R.rd(0x4DA3)) R.wr(0x4DA3, 3);
                    int t0 = M[idx][1];
                    if (R.rd(0x4DC1) == 0 && R.rd16(0x4DC2) == t0 - 100) {
                        R.wr(0x4DC2, (t0 - 1) & 255);
                        R.wr(0x4DC3, (t0 - 1) >> 8);
                    }
                } // harness time jump: the rest of the scatter phase is skipped
                R.frame();
                if (R.rd(0x4E00) == 1) break;
            }
            expect(stay(), "the level did not change during the measurement");
            int n_sc[4] = {0}, ok_sc[4] = {0}, n_ch[4] = {0}, ok_ch[4] = {0}, nup[4] = {0}, okup[4] = {0}, path_bad = 0;
            for (size_t ti = t0n; ti < R.tgts.size(); ++ti) {
                const Tgt &t = R.tgts[ti];
                bool scatter_ret = t.ret == 0x2751 || t.ret == 0x2787 || t.ret == 0x27C4 || t.ret == 0x280C;
                bool scatter_phase = (t.phase & 1) == 0 && t.state == 3 && (t.ghost != 0 || t.elroy == 0);
                int pe = t.pe, pd = t.pd, v = t.vec;
                int dir = v == 0xFF00 ? 0 : v == 0x0001 ? 1 : v == 0x0100 ? 2 : v == 0x00FF ? 3 : -1;
                if (t.ghost != 3 && scatter_ret != scatter_phase) {
                    ++path_bad;
                    continue;
                }
                if (scatter_ret && t.ghost != 3) {
                    ++n_sc[t.ghost];
                    if (t.de == corner[t.ghost]) ++ok_sc[t.ghost];
                    continue;
                }
                if (t.ghost == 0) {
                    ++n_ch[0];
                    if (t.de == (pd << 8 | pe)) ++ok_ch[0];
                    continue;
                }
                if (t.ghost == 1 || t.ghost == 2) {
                    if (dir < 0) continue;
                    int ahead = t.ghost == 1 ? 4 : 2, E2 = pe, D2 = pd;
                    if (dir == 0) D2 -= ahead;
                    else if (dir == 2) D2 += ahead;
                    else if (dir == 1) E2 += ahead;
                    else {
                        E2 -= ahead;
                        D2 += ahead;
                    }
                    if (t.ghost == 2) {
                        int be = t.bl & 255, bd = t.bl >> 8;
                        E2 = E2 + (E2 - be);
                        D2 = D2 + (D2 - bd);
                    }
                    int ex2 = (D2 & 255) << 8 | (E2 & 255);
                    ++n_ch[t.ghost];
                    if (t.de == ex2) ++ok_ch[t.ghost];
                    if (dir == 3) {
                        ++nup[t.ghost];
                        if (t.de == ex2) ++okup[t.ghost];
                    }
                    continue;
                }
                int gd = tcol(t.hl >> 8), ge = trow(t.hl & 255), dd = gd - tcol(pd), de2 = ge - trow(pe), d2 = dd * dd + de2 * de2;
                if (scatter_phase) {
                    ++n_sc[3];
                    if (t.de == corner[3]) ++ok_sc[3];
                    continue;
                }
                int ex2 = d2 < 64 ? corner[3] : (pd << 8 | pe);
                ++n_ch[3];
                if (t.de == ex2) ++ok_ch[3];
            }
            const char *nm[4] = {"Blinky", "Pinky", "Inky", "Clyde"};
            say("  %zu target calls\n", R.tgts.size() - t0n);
            for (int g = 0; g < 4; ++g)
                say("  %-6s scatter calls %d (corner in %d), chase calls %d (rule in %d)%s\n", nm[g], n_sc[g], ok_sc[g], n_ch[g], ok_ch[g],
                    (g == 1 || g == 2)
                        ? (std::string(", facing up: ") + std::to_string(nup[g]) + " calls, " + std::to_string(okup[g]) + " as the overflow bug says").c_str()
                        : "");
            bool allok = path_bad == 0;
            for (int g = 0; g < 4; ++g) {
                if (ok_sc[g] != n_sc[g] || ok_ch[g] != n_ch[g]) allok = false;
            }
            std::snprintf(b, sizeof b,
                          "every ghost target on this level follows the Dossier's rules (%zu calls; scatter corners, Blinky, Pinky 4 ahead, Inky via Blinky, Clyde's 8 "
                          "tiles, up-facing bug)",
                          R.tgts.size() - t0n);
            expect(allok && n_ch[0] >= 50 && n_ch[1] >= 20 && n_ch[2] >= 10 && n_ch[3] >= 10 && n_sc[0] >= 4 && n_sc[1] >= 4 && n_sc[2] >= 4 && n_sc[3] >= 4, b);
            expect(nup[1] >= 1 && okup[1] == nup[1] && nup[2] >= 1 && okup[2] == nup[2],
                   "the up-facing overflow bug is present on this level too (Pinky and Inky both seen facing up)");
        }

        // ---------------------------------------------------------------- E: Elroy thresholds (on a fresh run of the same level: the routine only runs while Blinky is out and moving normally, and the test writes the dots-eaten counter)
        say("\nE. Cruise Elroy dot counts (fresh run, harness: Clyde set out, 4DA3 = 3; the dots-eaten counter is written before every frame, counting down one dot at a "
            "time)\n");
        {
            int t1 = e[12], t2 = e[13], bad_flags = 0, frames = 0, e1_first = -1, e2_first = -1, seed_used = 0;
            bool ok = false, clean_run = false;
            const int pts[8] = {t1 + 2, t1 + 1, t1, t1 - 1, t2 + 2, t2 + 1, t2, t2 - 1}; // ascending dots eaten: the flags switch on and then stay on
            for (int seed = 2; seed < 12 && !clean_run; ++seed) { // a death during the hold resets Elroy, so such a run is thrown away and another seed is tried
                Run RE(g_prog, (uint32_t)seed);
                ok = RE.reach(L);
                if (!ok) continue;
                for (int k = 0; k < 200; ++k) {
                    RE.topup();
                    RE.frame();
                }
                bad_flags = frames = 0;
                e1_first = e2_first = -1;
                bool died = false;
                int min_end = 999;
                int lives0 = RE.rd(0x4E14);
                for (int pi = 0; pi < 8 && !died; ++pi) {
                    int left = pts[pi];
                    for (int k = 0; k < 6; ++k) {
                        RE.wr(0x4DA3, 3);
                        RE.wr(0x4E0E, 244 - left);
                        RE.frame();
                        if (RE.rd(0x4E14) < lives0 || RE.rd(0x4DA5) != 0 || RE.rd(0x4E13) != cnt) {
                            died = true;
                            break;
                        }
                        int end_left = 244 - RE.rd(0x4E0E), a = RE.rd(0x4DB6) != 0, c = RE.rd(0x4DB7) != 0;
                        ++frames;
                        if (end_left < min_end)
                            min_end = end_left; // the flags stay on once set, so 'off' is only certain while no frame so far has reached the threshold
                        // certain cases only: a dot eaten in the frame can move the count by one, so 'left' and 'left - 1' are both possible at the check
                        if (left <= t1 && !a) ++bad_flags;
                        if (min_end > t1 && a) ++bad_flags;
                        if (left <= t2 && !c) ++bad_flags;
                        if (min_end > t2 && c) ++bad_flags;
                        if (a && e1_first < 0) e1_first = left;
                        if (c && e2_first < 0) e2_first = left;
                    }
                }
                if (!died) {
                    clean_run = true;
                    seed_used = seed;
                }
            }
            say("  seed %d: Elroy 1 first seen with %d dots left, Elroy 2 first seen with %d dots left (table: %d and %d); frames checked %d, frames contradicting the "
                "rule %d\n",
                seed_used, e1_first, e2_first, t1, t2, frames, bad_flags);
            std::snprintf(b, sizeof b, "Elroy 1 switches on at %d dots left and Elroy 2 at %d, not before (the level's table)", t1, t2);
            expect(ok && clean_run && bad_flags == 0 && (e1_first == t1 || e1_first == t1 + 1) && (e2_first == t2 || e2_first == t2 + 1), b);
        }
    }
    say("\nS30A high-level tests: %d checks, %d failed. %s\n", checks, failures, failures ? "FAIL" : "PASS");
    if (g_out) std::fclose(g_out);
    return failures ? 1 : 0;
}
