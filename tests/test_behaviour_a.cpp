// S28: behaviour tests A: timing, speed, fruit. Measured on the C++ machine from RAM and from the program counter, across frames, and compared with
// the Pac-Man Dossier (docs/notes/table_comparison.md, MACHINE_NOTES.md section 6). The game code is not changed.
//
// Usage: test_behaviour_a pacman.zip [evidence.txt]
// Build: g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_behaviour_a.cpp src/board/inputs.cpp src/board/romset.cpp
//                    src/board/machine.cpp src/board/bus.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o build/s28/tba
//
// How the measurements are made
//  - The frame loop below is the same as Machine::run_frame (instructions until 50,688 cycles have passed, then the interrupt) but it looks at the
//    program counter before every instruction, so it can count how often a routine of the ROM runs (0E36 mode timer, 1806 Pac-Man speed tick, ...).
//    Part 0 proves it is the same machine (same memory after 1,500 frames as Machine::run_frame).
//  - A scripted player (coin, start, random joystick from a fixed seed) plays through the Inputs class. Seeds are tried in order 1, 2, 3 ... until a
//    game shows what a part needs; the seed used is printed.
//  - Where real play cannot reach the situation in a sensible script (surviving 84 s for every mode change, eating 70 and 170 dots), the test writes
//    one RAM cell (the mode counter, the dots-eaten counter) and says so; the ROM code that reacts is never touched.
//
// Items
//  A. Scatter/chase schedule, level 1 (Dossier: scatter 7 s, chase 20 s, scatter 7 s, chase 20 s, scatter 5 s, chase 20 s, scatter 5 s, chase for ever).
//     The mode timer (0E36) adds 1 to the counter 4DC2/4DC3 once per frame and moves the phase 4DC1 on when the counter equals the next entry of the
//     phase table at 4D86 (cumulative frames). Measured: the table, the first phase change in real time, every boundary (counter set 3 frames
//     before it), the stop at phase 7, and the pause while a power pill is active (4DA6).
//  B. Dot pause: Pac-Man loses 1 frame for a dot and 3 for an energizer (Dossier). The ROM stores a delay in 4D9D (1 dot, 6 energizer) that the speed
//     tick 1806 counts down; measured: calls of 1806 per frame, calls held for a dot and for an energizer, and that Pac-Man does not move meanwhile.
//  C. Fruit: at 70 and 170 dots (Dossier), position 8094, cherry on level 1. Measured from RAM and by counting the spawn code (0ED5).
//  D. Speeds, level 1 (Dossier: 100 % = 1.25 pixels per frame; Pac-Man 80 %, 90 % with a power pill, ghosts 75 %).
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
    char b[1024];
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

struct Player { // scripted player: coin at 400, start at 420, random joystick (hold 10-41 frames) from 430
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

struct Stall {
    int v, ticks;
    bool same_pos;
};
struct Spawn {
    int frame, dots, level, code, colour, pos;
};
struct End {
    int frame;
    bool expired;
};
struct FrameLog {
    int f, state, sub, pill, dying, phase, counter, dots, lives, c1806, c1806_free, c0e36, c0e42, c1843, pac_y, pac_x, bl_y, bl_x;
};

struct Run {
    Machine m;
    Inputs in;
    Player player;
    int f = 0;
    int c1806 = 0, c1806_free = 0, c0e36 = 0, c0e42 = 0, c1843 = 0, inc_since = 0;
    std::vector<Stall> stalls;
    std::vector<Spawn> spawns;
    std::vector<End> ends;
    std::vector<FrameLog> log;
    bool stalling = false, spawn_pending = false;
    int stall_v = 0, stall_count = 0, stall_y = 0, stall_x = 0;
    int changes = 0;
    std::vector<int> change_inc, change_counter, change_frames;
    explicit Run(const std::vector<uint8_t> &prog, uint32_t seed) : m(prog), player(seed) {}
    int rd(int a) { return m.bus.read((uint16_t)a); }
    int rd16(int a) { return rd(a) | rd(a + 1) << 8; }
    void wr(int a, int v) { m.bus.write((uint16_t)a, (uint8_t)v); }
    void hook(unsigned pc) {
        switch (pc) {
        case 0x0E36: ++c0e36; break;
        case 0x0E42:
            if (rd16(0x4DC2) == 0) inc_since = 0;
            ++inc_since;
            ++c0e42;
            break; // the counter is about to be incremented
        case 0x0E5C:
            ++changes;
            change_inc.push_back(inc_since);
            change_counter.push_back(rd16(0x4DC2));
            change_frames.push_back(f + 1);
            inc_since = 0;
            break;
        case 0x1806:
            ++c1806;
            if (rd(0x4D9D) == 0xFF) ++c1806_free;
            if (stalling) {
                if (rd(0x4D9D) == 0xFF) {
                    stalls.push_back({stall_v, stall_count, stall_y == rd(0x4D08) && stall_x == rd(0x4D09)});
                    stalling = false;
                } else ++stall_count;
            }
            break;
        case 0x1843: ++c1843; break; // Pac-Man's speed pattern fired: one step
        case 0x1A00:
            stalling = true;
            stall_v = rd(0x4D9D);
            stall_count = 0;
            stall_y = rd(0x4D08);
            stall_x = rd(0x4D09);
            break; // delay just stored
        case 0x0ED5: spawn_pending = true; break;
        case 0x1000: ends.push_back({f + 1, true}); break;  // fruit expiry task
        case 0x19C1: ends.push_back({f + 1, false}); break; // fruit eaten
        default: break;
        }
    }
    void frame(bool instrumented = true) {
        player.apply(in, f);
        in.apply(m.bus);
        c1806 = c1806_free = c0e36 = c0e42 = c1843 = 0;
        uint32_t t0 = m.ticks_in_frame();
        for (;;) {
            if (instrumented) hook(m.on_get_pc());
            m.on_step();
            if (m.ticks_in_frame() - t0 >= Machine::cycles_per_frame) break;
        }
        ++m.frame;
        m.bus.set_frame(m.frame);
        m.deliver_vblank_interrupt();
        ++f;
        log.push_back({f, rd(0x4E00), rd(0x4E04), rd(0x4DA6), rd(0x4DA5), rd(0x4DC1), rd16(0x4DC2), rd(0x4E0E), rd(0x4E14), c1806, c1806_free, c0e36, c0e42, c1843,
                       rd(0x4D08), rd(0x4D09), rd(0x4D00), rd(0x4D01)});
        if (spawn_pending) {
            spawn_pending = false;
            spawns.push_back({f, rd(0x4E0E), rd(0x4E13), rd(0x4C0C), rd(0x4C0D), rd16(0x4DD2)});
        }
    }
    bool playing() { return rd(0x4E00) == 3 && rd(0x4E04) == 3; }
    std::string ramhash() {
        uint8_t ram[0x1000];
        std::memcpy(ram, m.bus.videoram, 0x400);
        std::memcpy(ram + 0x400, m.bus.colorram, 0x400);
        std::memset(ram + 0x800, 0, 0x400);
        std::memcpy(ram + 0xC00, m.bus.workram, 0x3F0);
        std::memcpy(ram + 0xFF0, m.bus.spriteram, 0x10);
        return sha1_hex(ram, sizeof ram);
    }
};

static std::vector<uint8_t> g_prog;
// Dossier, level 1, in frames at 60 per second: phase ends at 7, 27, 34, 54, 59, 79, 84 s
static const int PHASE_TABLE[7] = {420, 1620, 2040, 3240, 3540, 4740, 5040};

int main(int argc, char **argv) {
    if (argc < 2) {
        std::printf("usage: test_behaviour_a pacman.zip [evidence.txt]\n");
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
    say("S28 behaviour tests A (timing, speed, fruit). The game code is not changed; every value below is measured on the running ROM.\n\n");

    // ---------------------------------------------------------------- 0
    say("0. The instrumented frame loop is the same machine as Machine::run_frame\n");
    {
        Run a(g_prog, 3);
        for (int i = 0; i < 1500; ++i) a.frame(true);
        Machine c(g_prog);
        Inputs in;
        Player p(3);
        for (int i = 0; i < 1500; ++i) {
            p.apply(in, i);
            in.apply(c.bus);
            c.run_frame();
        }
        uint8_t ram[0x1000];
        std::memcpy(ram, c.bus.videoram, 0x400);
        std::memcpy(ram + 0x400, c.bus.colorram, 0x400);
        std::memset(ram + 0x800, 0, 0x400);
        std::memcpy(ram + 0xC00, c.bus.workram, 0x3F0);
        std::memcpy(ram + 0xFF0, c.bus.spriteram, 0x10);
        std::string ha = a.ramhash(), hb = sha1_hex(ram, sizeof ram);
        say("  memory hash after 1,500 frames: instrumented %s, Machine::run_frame %s\n", ha.c_str(), hb.c_str());
        expect(ha == hb && a.rd(0x4E00) == 3, "identical memory, and the game is running (4E00 = 3)");
    }

    // ---------------------------------------------------------------- survey run
    say("\nSurvey: scripted games until one shows the first phase change in real time, many dot pauses and an energizer\n");
    Run *R = nullptr;
    uint32_t used_seed = 0;
    for (uint32_t seed = 1; seed <= 400 && !used_seed; ++seed) {
        delete R;
        R = new Run(g_prog, seed);
        for (int i = 0; i < 5000; ++i) {
            R->frame();
            if (R->rd(0x4E00) == 1 && R->f > 600) break;
            bool energizer = false;
            for (auto &s : R->stalls)
                if (s.v == 6) energizer = true;
            if (R->changes >= 1 && energizer && R->stalls.size() >= 60 && R->f >= 2600) {
                used_seed = seed;
                break;
            }
        }
    }
    if (!used_seed) {
        say("  no suitable scripted game in 400 seeds\n");
        expect(false, "survey run found");
        return 1;
    }
    say("  seed %u: ran %d frames, %zu dot/energizer pauses seen, %d phase change(s) in real time\n", used_seed, R->f, R->stalls.size(), R->changes);

    // ---------------------------------------------------------------- A
    say("\nA. Scatter/chase schedule, level 1\n");
    {
        bool eq = true;
        std::string s;
        for (int i = 0; i < 7; ++i) {
            int t = R->rd16(0x4D86 + 2 * i);
            s += std::to_string(t) + (i < 6 ? " " : "");
            if (t != PHASE_TABLE[i]) eq = false;
        }
        say("  phase table at 4D86 (cumulative frames): %s\n", s.c_str());
        say("  Dossier at 60 frames per second: 7, 27, 34, 54, 59, 79, 84 s = 420 1620 2040 3240 3540 4740 5040 frames\n");
        expect(eq, "table = scatter 7 / chase 20 / scatter 7 / chase 20 / scatter 5 / chase 20 / scatter 5 / chase for ever");
        say("  first phase change in real play (no time jump): at frame %d, timer increments counted since the counter was 0: %d, counter at the change: %d\n",
            R->change_frames[0], R->change_inc[0], R->change_counter[0]);
        expect(R->change_inc[0] == 420 && R->change_counter[0] == 420, "phase 0 -> 1 after exactly 420 timer increments (7 s)");
        int playing = 0, once = 0, pill_frames = 0, pill_inc = 0, other = 0, other_ok = 0;
        for (auto &l : R->log) {
            if (l.sub != 3 || l.state != 3) continue;
            ++playing;
            if (l.c0e36 == 1) ++once;
            if (l.pill) {
                ++pill_frames;
                pill_inc += l.c0e42;
            } else if (l.phase != 7) {
                ++other;
                if (l.c0e42 == 1) ++other_ok;
            }
        }
        say("  maze-running frames: %d, mode timer called once in %d; power-pill frames: %d (counter increments in them: %d); other frames: %d (exactly one increment in "
            "%d)\n",
            playing, once, pill_frames, pill_inc, other, other_ok);
        expect(playing > 1000 && once >= playing - 2, "the mode timer (0E36) runs once per frame while the maze runs");
        expect(pill_frames > 0 && pill_inc == 0, "the counter does not advance while a power pill is active (4DA6)");
        expect(other_ok >= other - 2, "otherwise the counter advances by exactly 1 per frame");
        int run = 0, best = 0, runs = 0;
        for (auto &l : R->log) {
            if (l.pill) ++run;
            else {
                if (run) {
                    ++runs;
                    if (run > best) best = run;
                }
                run = 0;
            }
        }
        say("  power pills seen: %d, longest active time %d frames (Dossier level 1: 6 s = 360 frames; shorter ones were cut by eating ghosts or a death)\n", runs, best);
    }
    {
        say("  every boundary: the counter 4DC2/4DC3 and the phase 4DC1 are set once to 3 frames before the boundary (time jump), then the ROM runs by itself\n");
        bool done = false;
        uint32_t qseed = 0;
        for (uint32_t seed = 1; seed <= 200 && !done; ++seed) {
            Run Q(g_prog, seed);
            while (Q.f < 700) Q.frame();
            if (!Q.playing()) continue;
            std::vector<std::string> lines;
            bool good = true;
            for (int i = 0; i < 7 && good; ++i) {
                Q.wr(0x4DC1, i);
                Q.wr(0x4DC2, (PHASE_TABLE[i] - 3) & 255);
                Q.wr(0x4DC3, (PHASE_TABLE[i] - 3) >> 8);
                int before = Q.changes, f0 = Q.f;
                for (int k = 0; k < 12 && Q.changes == before; ++k) {
                    Q.frame();
                    if (!Q.playing() || Q.rd(0x4DA6)) {
                        good = false;
                        break;
                    }
                }
                if (!good || Q.changes != before + 1) {
                    good = false;
                    break;
                }
                char b[200];
                std::snprintf(b, sizeof b, "    phase %d -> %d: counter %d (table %d), %d frames after the jump, %d increments", i, Q.rd(0x4DC1), Q.change_counter.back(),
                              PHASE_TABLE[i], Q.f - f0, Q.change_inc.back());
                lines.push_back(b);
                if (Q.rd(0x4DC1) != i + 1 || Q.change_counter.back() != PHASE_TABLE[i]) good = false;
            }
            if (!good) continue;
            // phase 7: the counter stops
            int cnt = Q.rd16(0x4DC2);
            int ph = Q.rd(0x4DC1), inc = 0;
            for (int k = 0; k < 120; ++k) {
                Q.frame();
                if (Q.rd(0x4DA6) == 0) inc += Q.log.back().c0e42;
            }
            done = (ph == 7 && Q.rd(0x4DC1) == 7 && Q.rd16(0x4DC2) == cnt && inc == 0 && Q.playing());
            qseed = seed;
            if (done) {
                for (auto &l : lines) say("%s\n", l.c_str());
                say("    phase 7 (chase for ever): counter stays %d, %d increments in 120 more frames\n", cnt, inc);
            }
        }
        say("  (seed %u)\n", qseed);
        expect(done, "all seven boundaries fire at the Dossier frame counts; phase 7 never ends");
    }

    // ---------------------------------------------------------------- B
    say("\nB. Dot pause\n");
    {
        int frames = 0, two = 0, dying = 0, dying_calls = 0;
        for (auto &l : R->log)
            if (l.sub == 3 && l.state == 3) {
                if (l.dying) {
                    ++dying;
                    dying_calls += l.c1806;
                    continue;
                }
                ++frames;
                if (l.c1806 == 2) ++two;
            }
        say("  Pac-Man speed tick (1806): exactly 2 calls in %d of %d maze-running frames (not counting %d frames of Pac-Man's death animation, where it runs %d "
            "times)\n",
            two, frames, dying, dying_calls);
        expect(two >= frames - 2, "the speed tick runs exactly twice per frame");
        int nd = 0, ne = 0, dok = 0, eok = 0, moved = 0;
        for (auto &s : R->stalls) {
            if (s.v == 1) {
                ++nd;
                if (s.ticks == 2) ++dok;
            } else if (s.v == 6) {
                ++ne;
                if (s.ticks == 7) ++eok;
            }
            if (!s.same_pos) ++moved;
        }
        say("  dots: delay value stored 1, calls held %d times out of %d = 2 calls = 1 frame (Dossier: 1 frame)\n", dok, nd);
        say("  energizers: delay value stored 6, calls held %d times out of %d = 7 calls = 3.5 frames (Dossier: 3 frames)\n", eok, ne);
        say("  Pac-Man's position changed during a pause: %d times\n", moved);
        expect(nd >= 30 && dok == nd, "a dot holds Pac-Man for 2 speed-tick calls = 1 frame = Dossier");
        expect(ne >= 1 && eok == ne,
               "an energizer holds Pac-Man for 7 speed-tick calls (3.5 frames); the Dossier says 3 frames: RECORDED DISAGREEMENT (the ROM is the reference)");
        expect(moved == 0, "Pac-Man does not move during a pause");
    }

    // ---------------------------------------------------------------- C
    say("\nC. Fruit\n");
    {
        bool done = false;
        for (uint32_t seed = 1; seed <= 300 && !done; ++seed) {
            Run F(g_prog, seed);
            while (F.f < 560 || !(F.playing() && F.rd(0x4E0E) >= 3)) {
                F.frame();
                if (F.f > 4000) break;
            }
            if (!(F.playing() && F.rd(0x4E0E) >= 3 && F.rd(0x4E0E) < 60)) continue;
            int d0 = F.rd(0x4E0E);
            F.wr(0x4E0E, 68); // time jump: dots eaten set to 68 (was d0)
            bool ok = true;
            int limit = 0;
            while (F.rd(0x4E0E) < 70 && ok && limit++ < 900) {
                F.frame();
                if (F.rd(0x4E00) != 3) ok = false;
            }
            if (!ok || F.rd(0x4E0E) != 70) continue;
            for (int k = 0; k < 3; ++k) F.frame();
            if (F.spawns.size() != 1) continue;
            Spawn s1 = F.spawns[0];
            int flag1 = F.rd(0x4E0C);
            say("  seed %u: dots-eaten counter set from %d to 68 (time jump), then the game ate two dots by itself\n", seed, d0);
            say("  first fruit: spawn code ran at 4E0E = %d (level %d), flag 4E0C = %d, position 4DD2 = %04X, sprite code %02X, colour %02X (cherry)\n", s1.dots,
                s1.level + 1, flag1, s1.pos, s1.code, s1.colour);
            // wait until the first fruit is gone (eaten or expired)
            int fr0 = F.f;
            limit = 0;
            while (F.ends.empty() && limit++ < 1500) {
                F.frame();
                if (F.rd(0x4E00) != 3) break;
            }
            if (F.ends.empty()) continue;
            int gone = F.ends[0].frame - s1.frame;
            bool expired = F.ends[0].expired;
            (void)fr0;
            // let the maze get back to running, then second fruit
            while (!F.playing() && F.f < 20000) F.frame();
            for (int k = 0; k < 3 && F.rd(0x4E0E) >= 168; ++k) F.frame();
            int d1 = F.rd(0x4E0E);
            if (d1 >= 165) continue;
            F.wr(0x4E0E, 168); // time jump: dots eaten set to 168
            ok = true;
            limit = 0;
            while (F.rd(0x4E0E) < 170 && ok && limit++ < 1500) {
                F.frame();
                if (F.rd(0x4E00) != 3) ok = false;
            }
            if (!ok || F.rd(0x4E0E) != 170) continue;
            for (int k = 0; k < 3; ++k) F.frame();
            if (F.spawns.size() != 2) continue;
            Spawn s2 = F.spawns[1];
            say("  fruit left the screen %d frames after it appeared (%.2f s) - %s\n", gone, gone / 60.0, expired ? "it timed out" : "Pac-Man ate it");
            say("  second fruit (dots-eaten counter set from %d to 168, time jump): spawn code ran at 4E0E = %d, flag 4E0D = %d, position %04X, sprite %02X colour "
                "%02X\n",
                d1, s2.dots, F.rd(0x4E0D), s2.pos, s2.code, s2.colour);
            say("  spawn code ran %zu times in the whole game (exactly at 70 and 170 dots, never at 68-69, 71-169)\n", F.spawns.size());
            expect(s1.dots == 70 && s2.dots == 170, "fruit spawns at 70 and 170 dots (Dossier)");
            expect(s1.pos == 0x8094 && s2.pos == 0x8094, "fruit position is 8094, below the ghost house (Dossier: fixed spot under the house)");
            expect(s1.level == 0 && s1.code == 0 && s1.colour == 0x14, "level 1 fruit is the cherry (sprite 00, colour 14), ROM table 0EFD");
            expect(flag1 == 1 && F.rd(0x4E0D) == 1, "the two fruit flags 4E0C and 4E0D are set once each");
            say("  Dossier says the fruit stays 9-10 s with the exact time random: measured %.2f s, recorded (closes the open item 'fruit on-screen time').\n",
                gone / 60.0);
            done = true;
        }
        expect(done, "fruit run completed");
    }

    // ---------------------------------------------------------------- D
    say("\nD. Speeds, level 1 (100 %% = 1.25 pixels per frame)\n");
    {
        long ev_n = 0, calls_n = 0, ev_p = 0, calls_p = 0;
        double bl_sum = 0;
        long bl_n = 0;
        for (size_t i = 1; i < R->log.size(); ++i) {
            const FrameLog &l = R->log[i], &p = R->log[i - 1];
            if (l.sub != 3 || l.state != 3 || p.sub != 3 || l.dying || p.dying) continue;
            if (l.pill) {
                ev_p += l.c1843;
                calls_p += l.c1806_free;
            } else {
                ev_n += l.c1843;
                calls_n += l.c1806_free;
            }
            if (!l.pill && !p.pill && l.f > 700 && l.dots < 150 && l.c1806 == 2) {
                int dy = std::abs(l.bl_y - p.bl_y), dx = std::abs(l.bl_x - p.bl_x);
                if (dy + dx <= 2 && l.bl_y != 0x64 && l.bl_y != 0x80) {
                    bl_sum += dx + dy;
                    ++bl_n;
                }
            }
        }
        double pm = calls_n ? 2.0 * ev_n / calls_n : 0, pp = calls_p ? 2.0 * ev_p / calls_p : 0, bl = bl_n ? bl_sum / bl_n : 0;
        say("  Pac-Man, no power pill: %ld steps in %ld free speed-tick calls = %.4f steps per frame = %.1f %% (Dossier 80 %% = 1.0000)\n", ev_n, calls_n, pm,
            pm / 1.25 * 100);
        say("  Pac-Man, power pill active: %ld steps in %ld calls = %.4f per frame = %.1f %% (Dossier 90 %% = 1.1250)\n", ev_p, calls_p, pp, pp / 1.25 * 100);
        say("  Blinky outside the tunnel, no power pill: %.4f pixels per frame over %ld frames = %.1f %% (Dossier 75 %% = 0.9375)\n", bl, bl_n, bl / 1.25 * 100);
        expect(calls_n > 1000 && std::fabs(pm - 1.0) < 0.03, "Pac-Man runs at 80 % (within 3 %)");
        expect(calls_p > 100 && std::fabs(pp - 1.125) < 0.04, "Pac-Man with a power pill runs at 90 % (within 3.5 %)");
        expect(bl_n > 500 && std::fabs(bl - 0.9375) < 0.04, "Blinky runs at 75 % (within 4 %)");
    }

    say("\n%d checks, %d failed. %s\n", checks, failures, failures ? "FAIL" : "PASS");
    if (g_out) std::fclose(g_out);
    return failures ? 1 : 0;
}
