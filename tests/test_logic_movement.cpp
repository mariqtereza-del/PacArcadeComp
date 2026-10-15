// T01e: the readable movement (src/logic/movement.h) against the ROM, side by side on the real machine.
// Usage: test_logic_movement PROG.BIN FRAMES [RANDOM_CASES]
// ROM pieces checked: Pac-Man's movement 1806 and the four ghost movers 1B36 (Blinky), 1C4B (Pinky), 1D22 (Inky), 1DF9 (Clyde), with everything they call
// for the movement (2000, 2018, 205A, 1ED0, the turn-round routines 1EFE/1F25/1F4C/1F73, Cruise Elroy 20D7, the speed patterns, eating at 19CD, eating the fruit).
// Compared after every call: all work RAM (4C00-4FEF), the video RAM, the colour RAM, except the bytes under the stack pointer used by the routine's own calls.
// Part A: a scripted player plays FRAMES frames; at every entry of the five routines the readable version gets a copy of the memory (and the joystick bytes);
//   at the return they must match.
// Part B (each routine alone on the real machine):
//   B1 Pac-Man: every combination of direction x step x pixel position inside the tile (8 x 8) x joystick (16) x walls around (32) x maze rows / tunnel rows
//      x normal / demo / level-change walk;
//   B2 each ghost: every combination of step x pixel position inside the tile x turn-round flag x blue x tunnel colour x door colour x tile class (5) x the 4E02 setting;
//   B3 RANDOM_CASES random runs of each of the five routines with completely random memory around the fields it uses (speed patterns, flags, tiles, fruit...).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/logic/movement.h"

struct Mem {
    uint8_t work[logic::kWorkRamSize], video[logic::kVideoRamSize], color[0x400];
};
static const unsigned kStackMargin = 48;
static void snapshot(Machine &m, Mem &x) {
    std::memcpy(x.work, m.bus.workram, sizeof x.work);
    std::memcpy(x.video, m.bus.videoram, sizeof x.video);
    std::memcpy(x.color, m.bus.colorram, sizeof x.color);
}
static bool same(const Mem &rom, const Mem &model, unsigned sp, unsigned *where) {
    for (unsigned i = 0; i < logic::kWorkRamSize; ++i) {
        unsigned a = logic::kWorkRamStart + i;
        if (a + kStackMargin >= sp && a < sp + 2) continue;
        if (rom.work[i] != model.work[i]) {
            *where = a;
            return false;
        }
    }
    for (unsigned i = 0; i < sizeof rom.video; ++i)
        if (rom.video[i] != model.video[i]) {
            *where = 0x4000 + i;
            return false;
        }
    for (unsigned i = 0; i < sizeof rom.color; ++i)
        if (rom.color[i] != model.color[i]) {
            *where = 0x4400 + i;
            return false;
        }
    return true;
}
static uint8_t byte_at(const Mem &x, unsigned a) { return a >= 0x4C00 ? x.work[a - 0x4C00] : a >= 0x4400 ? x.color[a - 0x4400] : x.video[a - 0x4000]; }
static logic::WorkRam WR(Mem &x) { return logic::WorkRam{x.work}; }
static logic::Screen SC(Mem &x) { return logic::Screen{x.video, x.color}; }
static const unsigned kUnitPc[5] = {0x1B36, 0x1C4B, 0x1D22, 0x1DF9, 0x1806}; // units 0-3 the ghosts, 4 Pac-Man
static const char *kUnitName[5] = {"Blinky", "Pinky", "Inky", "Clyde", "Pac-Man"};
static void model_unit(int u, Mem &x, logic::JoystickPorts ports) {
    if (u == 4) logic::pacman_move(WR(x), SC(x), ports);
    else logic::ghost_move(WR(x), SC(x), (unsigned)u);
}
static void W(Mem &x, unsigned a, unsigned v) { x.work[a - 0x4C00] = (uint8_t)v; }
static uint8_t R(const Mem &x, unsigned a) { return x.work[a - 0x4C00]; }

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_movement PROG.BIN FRAMES [RANDOM_CASES]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    const long frames = std::atol(argv[2]);
    const long random_cases = argc > 3 ? std::atol(argv[3]) : 1000000;
    static Machine m(prog);
    unsigned long bad = 0;
    auto report = [&](const char *what, unsigned where, unsigned a, unsigned b) {
        if (++bad <= 8) std::printf("  MISMATCH (%s): address %04X, ROM %02X, readable %02X\n", what, where, a, b);
    };

    // ---- Part A
    unsigned long calls[5] = {0}, moved[5] = {0}, pac_turn = 0, pac_stop = 0, pac_demo = 0, pac_fruit = 0, pac_dot = 0, pac_tunnel_rows = 0, ghost_blue = 0,
                  ghost_tunnel = 0, ghost_middle[4] = {0}, ghost_reverse = 0, elroy1 = 0, elroy2 = 0;
    struct Pend {
        bool on = false;
        unsigned sp = 0;
        logic::JoystickPorts ports{0, 0};
        Mem before, expect;
    } pend[5];
    auto hook = [&](uint16_t pcnow) {
        for (int u = 0; u < 5; ++u) {
            if (pcnow == kUnitPc[u] && !pend[u].on) {
                Pend &p = pend[u];
                p.on = true;
                p.sp = m.on_get_sp();
                p.ports = {m.bus.in0, m.bus.in1};
                snapshot(m, p.before);
                p.expect = p.before;
                model_unit(u, p.expect, p.ports);
            } else if (pend[u].on && m.on_get_sp() == pend[u].sp + 2) {
                Pend &p = pend[u];
                p.on = false;
                ++calls[u];
                Mem got;
                snapshot(m, got);
                unsigned where = 0;
                const unsigned px = u == 4 ? 0x4D08 : 0x4D00 + 2 * u;
                if (R(got, px) != R(p.before, px) || R(got, px + 1) != R(p.before, px + 1)) ++moved[u];
                if (u == 4) {
                    if (R(got, 0x4D30) != R(p.before, 0x4D30)) ++pac_turn;
                    if (R(p.before, 0x4E00) == 1 || R(p.before, 0x4E04) >= 0x10) ++pac_demo;
                    if (R(p.before, 0x4D3A) < 0x21 || R(p.before, 0x4D3A) >= 0x3B) ++pac_tunnel_rows;
                    if (R(got, 0x4E0E) != R(p.before, 0x4E0E)) ++pac_dot;
                    if (R(p.before, 0x4DD2) && R(got, 0x4DD2) == 0 && R(p.before, 0x4DD2) != 0) ++pac_fruit;
                    if (R(p.before, 0x4D9D) == 0xFF && R(got, 0x4D08) == R(p.before, 0x4D08) && R(got, 0x4D09) == R(p.before, 0x4D09) && R(p.before, 0x4E00) != 1)
                        ++pac_stop;
                } else {
                    if (R(p.before, 0x4DA7 + u)) ++ghost_blue;
                    if (R(got, 0x4D99 + u)) ++ghost_tunnel;
                    if (R(got, 0x4DB1 + u) != R(p.before, 0x4DB1 + u)) ++ghost_reverse;
                    if ((R(p.before, 0x4D14 + 2 * u) ? R(p.before, px) : R(p.before, px + 1)) % 8 == 4) ++ghost_middle[u];
                    if (u == 0) {
                        if (R(got, 0x4DB6) && !R(p.before, 0x4DB6)) ++elroy1;
                        if (R(got, 0x4DB7) && !R(p.before, 0x4DB7)) ++elroy2;
                    }
                }
                if (!same(got, p.expect, p.sp, &where)) report(kUnitName[u], where, byte_at(got, where), byte_at(p.expect, where));
            }
        }
    };
    uint32_t rng = 7, hold = 0;
    int dir = 0x04;
    for (long fr = 0; fr < frames; ++fr) {
        long p = fr % 15000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
        if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20;
        if (p >= 430) {
            if (hold == 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + ((rng >> 16) & 31);
            }
            --hold;
            in0 &= (uint8_t)~dir;
        }
        m.bus.in0 = in0;
        m.bus.in1 = in1;
        m.bus.dsw1 = (uint8_t)(0xC9 | (((fr / 15000) % 3 == 1) ? 0x04 : 0));
        probe::run_frame(m, hook);
    }
    std::printf(
        "part A: %ld frames (%.1f hours of game time): calls Pac-Man %lu (moved %lu, direction changed %lu, stood still %lu, demo/level-change walk %lu, in tunnel rows "
        "%lu, ate a dot %lu, ate the fruit %lu), Blinky %lu (moved %lu, at a tile middle %lu, Cruise Elroy 1 started %lu, 2 started %lu), Pinky %lu (moved %lu, middle "
        "%lu), Inky %lu (moved %lu, middle %lu), Clyde %lu (moved %lu, middle %lu); blue %lu, in tunnel %lu, turn-round flags used %lu; differences %lu\n",
        frames, frames / 60.606 / 3600.0, calls[4], moved[4], pac_turn, pac_stop, pac_demo, pac_tunnel_rows, pac_dot, pac_fruit, calls[0], moved[0], ghost_middle[0],
        elroy1, elroy2, calls[1], moved[1], ghost_middle[1], calls[2], moved[2], ghost_middle[2], calls[3], moved[3], ghost_middle[3], ghost_blue, ghost_tunnel,
        ghost_reverse, bad);
    const unsigned long bad_a = bad;

    // ---- Part B
    static Machine c(prog);
    const uint16_t sp0 = 0x4FC0, ret = 0x3F00;
    auto prepare = [&](const Mem &in, unsigned pc_start) {
        std::memcpy(c.bus.workram, in.work, sizeof in.work);
        std::memcpy(c.bus.videoram, in.video, sizeof in.video);
        std::memcpy(c.bus.colorram, in.color, sizeof in.color);
        c.bus.write((uint16_t)(sp0 - 2), (uint8_t)(ret & 255));
        c.bus.write((uint16_t)(sp0 - 1), (uint8_t)(ret >> 8));
        c.on_set_sp((uint16_t)(sp0 - 2));
        c.on_set_pc((uint16_t)pc_start);
    };
    auto run = [&]() {
        int guard = 0;
        while (c.on_get_pc() != ret && ++guard < 20000) c.on_step();
        return guard < 20000;
    };
    uint32_t s = 4242;
    auto nxt = [&]() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    };
    auto randomize = [&](Mem &x) {
        for (auto &b : x.work) b = (uint8_t)nxt();
        for (auto &b : x.video) b = (uint8_t)nxt();
        for (auto &b : x.color) b = (uint8_t)nxt();
    };
    auto nonwall = [&]() {
        uint8_t v;
        do v = (uint8_t)nxt();
        while ((v & 0xC0) == 0xC0);
        return v;
    };
    auto set_video = [&](Mem &x, unsigned address, bool wall) {
        if (address >= 0x4000 && address < 0x4400) x.video[address - 0x4000] = wall ? (uint8_t)(0xC0 | (nxt() & 0x3F)) : nonwall();
        else if (address >= 0x4400 && address < 0x4800) x.color[address - 0x4400] = wall ? (uint8_t)(0xC0 | (nxt() & 0x3F)) : nonwall();
    };
    auto video_of = [&](unsigned a, unsigned b) { return logic::video_address_of_tile((uint8_t)a, (uint8_t)b); };
    auto run_case = [&](Mem &x, int u, logic::JoystickPorts ports, unsigned long &counter, const char *what) {
        W(x, 0x4C80, 0xC0 + nxt() % 0x40);
        W(x, 0x4C81, 0x4C); // the task queue pointer must point into its page (the ROM would write anywhere)
        Mem want = x;
        model_unit(u, want, ports);
        c.bus.in0 = ports.in0;
        c.bus.in1 = ports.in1;
        prepare(x, kUnitPc[u]);
        ++counter;
        if (!run()) {
            report(what, 0, 0, 0);
            return;
        }
        Mem got;
        snapshot(c, got);
        unsigned where = 0;
        if (!same(got, want, sp0 - 2, &where)) report(what, where, byte_at(got, where), byte_at(want, where));
    };
    // make a pattern move on this call (a 1 bit about to fall out) or not
    auto set_pattern = [&](Mem &x, unsigned hi, bool carry) {
        unsigned h = carry ? (0x8000 | (nxt() & 0x7FFF)) : (nxt() & 0x7FFF);
        W(x, hi, h & 255);
        W(x, hi + 1, h >> 8);
    };
    unsigned long n_pac = 0, n_ghost = 0, n_rand[5] = {0};

    // B1: Pac-Man, every combination
    {
        Mem base;
        randomize(base);
        for (unsigned mode = 0; mode < 6; ++mode)
            for (unsigned d0 = 0; d0 < 4; ++d0)
                for (unsigned vecsel = 0; vecsel < 5; ++vecsel)
                    for (unsigned pxl = 0; pxl < 64; ++pxl)
                        for (unsigned stick = 0; stick < 16; ++stick)
                            for (unsigned walls = 0; walls < 32; ++walls) {
                                Mem x = base;
                                if ((n_pac & 255) == 0) {
                                    randomize(base);
                                    x = base;
                                }
                                const bool tunnel_rows = mode & 1;
                                const unsigned kind = mode >> 1; // 0 normal, 1 demo (4E00 = 1), 2 level change (4E04 >= 10)
                                W(x, 0x4D9D, 0xFF);
                                W(x, 0x4DA6, nxt() & 1);
                                set_pattern(x, 0x4D46, true);
                                set_pattern(x, 0x4D4A, true);
                                W(x, 0x4E00, kind == 1 ? 1 : (nxt() & 1) ? 3 : 0);
                                W(x, 0x4E04, kind == 2 ? 0x10 + (nxt() & 15) : 3);
                                W(x, 0x4E09, nxt() & 1);
                                W(x, 0x4E72, nxt() & 1);
                                unsigned ta = 0x21 + nxt() % 0x1E, tb = tunnel_rows ? ((nxt() & 1) ? 0x1E + nxt() % 3 : 0x3B + nxt() % 3) : 0x21 + nxt() % 0x1A;
                                W(x, 0x4D39, ta);
                                W(x, 0x4D3A, tb);
                                const unsigned px = (nxt() & 0xF8) | (pxl & 7), py = (nxt() & 0xF8) | (pxl >> 3);
                                W(x, 0x4D08, px);
                                W(x, 0x4D09, py);
                                W(x, 0x4D30, d0);
                                if (vecsel < 4) {
                                    W(x, 0x4D1C, (uint8_t)logic::kStepA[vecsel]);
                                    W(x, 0x4D1D, (uint8_t)logic::kStepB[vecsel]);
                                } else {
                                    W(x, 0x4D1C, 0);
                                    W(x, 0x4D1D, 0);
                                }
                                W(x, 0x4D3C, nxt() & 3);
                                W(x, 0x4DBF, 0);
                                W(x, 0x4D12, nxt());
                                W(x, 0x4D13, (nxt() & 1) ? 0x1D : (nxt() & 1) ? 0x3E : 0x21 + nxt() % 0x1A);
                                for (unsigned d = 0; d < 4; ++d) set_video(x, video_of(ta + (uint8_t)logic::kStepA[d], tb + (uint8_t)logic::kStepB[d]), (walls >> d) & 1);
                                set_video(x, video_of(ta, tb), (walls >> 4) & 1);
                                W(x, 0x4DD2, (nxt() & 3) ? 0 : 0x94);
                                W(x, 0x4DD4, nxt() & 3 ? 0 : 1 + nxt() % 8);
                                const uint8_t jv = (uint8_t)(0xF0 | stick);
                                run_case(x, 4, logic::JoystickPorts{jv, jv}, n_pac, "Pac-Man every combination");
                            }
    }
    std::printf("part B1: Pac-Man alone, every combination of mode (maze rows / tunnel rows x normal / demo / level change) x direction x step x pixel position in the "
                "tile x joystick x walls: %lu calls, differences so far %lu\n",
                n_pac, bad - bad_a);
    const unsigned long bad_b1 = bad;

    // B2: each ghost, every combination
    for (unsigned g = 0; g < 4; ++g) {
        Mem base;
        randomize(base);
        for (unsigned vecsel = 0; vecsel < 5; ++vecsel)
            for (unsigned pxl = 0; pxl < 64; ++pxl)
                for (unsigned flags = 0; flags < 32; ++flags)
                    for (unsigned cls = 0; cls < 5; ++cls)
                        for (unsigned var = 0; var < 4; ++var) {
                            Mem x = base;
                            if ((n_ghost & 255) == 0) {
                                randomize(base);
                                x = base;
                            }
                            // flags: 1 turn-round, 2 blue, 4 tunnel colour on the move tile, 8 door colour on the next tile, 16 4E02 = 0x22
                            for (unsigned i = 0; i < 4; ++i) {
                                W(x, 0x4DA0 + i, i == 0 ? 1 + (nxt() & 3) : 1);
                                W(x, 0x4DAC + i, 0);
                                W(x, 0x4DB1 + i, 0);
                                W(x, 0x4DA7 + i, 0);
                                W(x, 0x4D99 + i, nxt() & 1);
                            }
                            W(x, 0x4DA3, 1);
                            W(x, 0x4DB1 + g, flags & 1);
                            W(x, 0x4DA7 + g, (flags >> 1) & 1);
                            W(x, 0x4E02, (flags & 16) ? 0x22 : (nxt() & 0x3F));
                            set_pattern(x, logic::kSpeedNormalAddress[g], true);
                            auto setp = [&](unsigned hi) { set_pattern(x, hi, true); };
                            setp(logic::kSpeedBlueAddress[g]);
                            setp(logic::kSpeedTunnelAddress[g]);
                            setp(logic::kSpeedElroy1Address);
                            setp(logic::kSpeedElroy2Address);
                            W(x, 0x4DB6, nxt() & 1);
                            W(x, 0x4DB7, nxt() & 1);
                            W(x, 0x4DBB, nxt());
                            W(x, 0x4DBC, nxt());
                            W(x, 0x4E0E, nxt() % 245);
                            const unsigned px = (nxt() & 0xF8) | (pxl & 7), py = (nxt() & 0xF8) | (pxl >> 3);
                            W(x, 0x4D00 + 2 * g, px);
                            W(x, 0x4D01 + 2 * g, py);
                            if (vecsel < 4) {
                                W(x, 0x4D14 + 2 * g, (uint8_t)logic::kStepA[vecsel]);
                                W(x, 0x4D15 + 2 * g, (uint8_t)logic::kStepB[vecsel]);
                            } else {
                                W(x, 0x4D14 + 2 * g, 0);
                                W(x, 0x4D15 + 2 * g, 0);
                            }
                            W(x, 0x4D1E + 2 * g, (uint8_t)logic::kStepA[nxt() & 3]);
                            W(x, 0x4D1F + 2 * g, (uint8_t)logic::kStepB[nxt() & 3]);
                            {
                                unsigned d = nxt() & 3;
                                W(x, 0x4D1E + 2 * g, (uint8_t)logic::kStepA[d]);
                                W(x, 0x4D1F + 2 * g, (uint8_t)logic::kStepB[d]);
                                W(x, 0x4D2C + g, d);
                                W(x, 0x4D28 + g, nxt() & 3);
                            }
                            const unsigned ta = nxt() & 255, tb0 = nxt() & 255;
                            const unsigned tb = cls == 0 ? 0x1D : cls == 1 ? 0x3E : cls == 2 ? (nxt() % 0x1D) : cls == 3 ? 0x3B + nxt() % 0x80 : 0x21 + nxt() % 0x1A;
                            (void)tb0;
                            W(x, 0x4D0A + 2 * g, ta);
                            W(x, 0x4D0B + 2 * g, tb);
                            const unsigned ma = nxt() & 255, mb = nxt() & 255;
                            W(x, 0x4D31 + 2 * g, ma);
                            W(x, 0x4D32 + 2 * g, mb);
                            {
                                unsigned a = video_of(ma, mb) + 0x400;
                                if (a >= 0x4400 && a < 0x4800) x.color[a - 0x4400] = (flags & 4) ? 0x1B : (uint8_t)(0x05 + (nxt() & 1));
                            }
                            {
                                unsigned a = video_of(ta, tb) + 0x400;
                                if (a >= 0x4400 && a < 0x4800) x.color[a - 0x4400] = (flags & 8) ? 0x1A : 0x05;
                            }
                            run_case(x, (int)g, logic::JoystickPorts{0xFF, 0xFF}, n_ghost, "ghost every combination");
                            (void)var;
                        }
    }
    std::printf("part B2: each ghost alone, every combination of step x pixel position in the tile x turn-round x blue x tunnel colour x door colour x tile class x 4E02 "
                "setting: %lu calls, differences so far %lu\n",
                n_ghost, bad - bad_b1);
    const unsigned long bad_b2 = bad;

    // B3: random
    for (long i = 0; i < random_cases; ++i)
        for (int u = 0; u < 5; ++u) {
            Mem x;
            randomize(x);
            for (unsigned k = 0; k < 4; ++k) {
                W(x, 0x4DA0 + k, k == 0 ? (nxt() % 5 ? 1 : 0) : (nxt() % 6 ? 1 : nxt() & 3));
                W(x, 0x4DAC + k, nxt() % 8 ? 0 : 1);
                W(x, 0x4DB1 + k, nxt() % 6 ? 0 : 1);
                W(x, 0x4DA7 + k, nxt() & 1);
            }
            for (unsigned hi :
                 {0x4D46u, 0x4D4Au, 0x4D4Eu, 0x4D52u, 0x4D56u, 0x4D5Au, 0x4D5Eu, 0x4D62u, 0x4D66u, 0x4D6Au, 0x4D6Eu, 0x4D72u, 0x4D76u, 0x4D7Au, 0x4D7Eu, 0x4D82u})
                set_pattern(x, hi, nxt() % 4 != 0);
            W(x, 0x4D9D, nxt() % 4 ? 0xFF : nxt() & 0x1F);
            W(x, 0x4DA6, nxt() & 1);
            W(x, 0x4E00, nxt() % 6 ? (nxt() & 1 ? 3 : 0) : 1);
            W(x, 0x4E04, nxt() % 5 ? 3 : nxt() & 0x1F);
            W(x, 0x4DB6, nxt() & 1);
            W(x, 0x4DB7, nxt() & 1);
            W(x, 0x4DBF, nxt() % 8 ? 0 : 1);
            W(x, 0x4E02, nxt() % 3 ? 0x22 : nxt() & 0x3F);
            W(x, 0x4E0E, nxt() % 245);
            W(x, 0x4E12, nxt() & 1);
            W(x, 0x4DD2, (nxt() & 1) ? 0 : 0x94);
            W(x, 0x4DD4, nxt() & 1 ? 0 : 1 + nxt() % 8);
            // positions: a random tile in the maze region with its pixel position; steps are real steps; directions 0-3
            auto place = [&](unsigned pixaddr, unsigned tileaddr, bool fruit_aim) {
                unsigned px = nxt() & 255, py = nxt() & 255;
                if (nxt() % 3 == 0) { px = (px & 0xF8) | 4; }
                if (nxt() % 3 == 0) { py = (py & 0xF8) | 4; }
                if (fruit_aim) {
                    px = 0x94;
                    py = 0x80;
                }
                W(x, pixaddr, px);
                W(x, pixaddr + 1, py);
                W(x, tileaddr, nxt() & 255);
                W(x, tileaddr + 1, nxt() % 4 ? 0x21 + nxt() % 0x1A : (nxt() & 1 ? 0x1D : nxt() & 255));
            };
            auto stepat = [&](unsigned a) {
                unsigned d = nxt() % 5;
                if (d == 4) {
                    W(x, a, 0);
                    W(x, a + 1, 0);
                } else {
                    W(x, a, (uint8_t)logic::kStepA[d]);
                    W(x, a + 1, (uint8_t)logic::kStepB[d]);
                }
            };
            if (u == 4) {
                const bool aim = nxt() % 4 == 0;
                place(0x4D08, 0x4D39, aim);
                if (aim) {
                    unsigned d = (nxt() & 1) ? 0 : 2;
                    W(x, 0x4D30, d);
                    W(x, 0x4D1C, (uint8_t)logic::kStepA[d]);
                    W(x, 0x4D1D, (uint8_t)logic::kStepB[d]);
                    W(x, 0x4D08, 0x94);
                    W(x, 0x4D09, (uint8_t)(0x80 - (int8_t)logic::kStepB[d]));
                } else {
                    stepat(0x4D1C);
                    W(x, 0x4D30, nxt() & 3);
                }
                stepat(0x4D26);
                W(x, 0x4D3C, nxt() & 3);
                W(x, 0x4D12, nxt() & 255);
                W(x, 0x4D13, nxt() % 3 ? 0x21 + nxt() % 0x1A : nxt() & 1 ? 0x1D : 0x3E);
                for (unsigned d = 0; d < 5; ++d) {
                    unsigned a =
                        d < 4 ? video_of(R(x, 0x4D39) + (uint8_t)logic::kStepA[d], R(x, 0x4D3A) + (uint8_t)logic::kStepB[d]) : video_of(R(x, 0x4D39), R(x, 0x4D3A));
                    set_video(x, a, nxt() & 1);
                }
            } else {
                const unsigned g = (unsigned)u;
                place(0x4D00 + 2 * g, 0x4D31 + 2 * g, false);
                stepat(0x4D14 + 2 * g);
                stepat(0x4D1E + 2 * g);
                W(x, 0x4D28 + g, nxt() & 3);
                W(x, 0x4D2C + g, nxt() & 3);
                W(x, 0x4D0A + 2 * g, nxt() & 255);
                W(x, 0x4D0B + 2 * g, nxt() % 3 ? 0x21 + nxt() % 0x1A : nxt() % 4 == 0 ? 0x1D : nxt() % 3 == 0 ? 0x3E : nxt() & 255);
                auto colour_at = [&](unsigned a, unsigned b, unsigned v) {
                    unsigned ad = video_of(a, b) + 0x400;
                    if (ad >= 0x4400 && ad < 0x4800) x.color[ad - 0x4400] = (uint8_t)v;
                };
                colour_at(R(x, 0x4D31 + 2 * g), R(x, 0x4D32 + 2 * g), nxt() % 3 ? 0x1B : nxt() & 0x3F);
                colour_at(R(x, 0x4D0A + 2 * g), R(x, 0x4D0B + 2 * g), nxt() % 3 ? 0x1A : nxt() & 0x3F);
            }
            run_case(x, u, logic::JoystickPorts{(uint8_t)(0xF0 | (nxt() & 15)), (uint8_t)(0xF0 | (nxt() & 15))}, n_rand[u], u == 4 ? "random Pac-Man" : "random ghost");
        }
    std::printf("part B3: %lu random runs of each routine (Pac-Man %lu, Blinky %lu, Pinky %lu, Inky %lu, Clyde %lu): differences %lu\n", (unsigned long)random_cases,
                n_rand[4], n_rand[0], n_rand[1], n_rand[2], n_rand[3], bad - bad_b2);
    bool ok = bad == 0 && calls[4] > 0 && calls[0] > 0 && calls[1] > 0 && calls[2] > 0 && calls[3] > 0;
    std::printf("%s: %lu live calls and %lu isolated calls compared, %lu differences\n", ok ? "PASS test_logic_movement" : "FAIL test_logic_movement",
                calls[0] + calls[1] + calls[2] + calls[3] + calls[4], n_pac + n_ghost + n_rand[0] + n_rand[1] + n_rand[2] + n_rand[3] + n_rand[4], bad);
    return ok ? 0 : 1;
}
