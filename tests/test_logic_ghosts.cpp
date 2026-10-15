// T01d: the readable ghost targeting (src/logic/ghost_targeting.h) against the ROM, side by side on the real machine.
// Usage: test_logic_ghosts PROG.BIN FRAMES [RANDOM_CASES]
// ROM pieces checked: the direction chooser 2966 (also its distance helpers 29EA and 2A12) and the four target tasks 2730 (Blinky), 276C (Pinky), 27A9 (Inky), 27F1 (Clyde).
// Compared after every call: all work RAM (4C00-4FEF, which includes the chooser's working bytes), the video RAM (the maze) and the colour RAM, except the bytes under the
// stack pointer used by the routine's own calls; for the chooser also its answer in the registers (A = direction, HL = step).
// Part A: a scripted player plays FRAMES frames; at every entry of the five ROM pieces the readable version gets a copy of the memory (and the registers); at the return they must match.
// Part B (each piece alone on the real machine):
//   B1 the multiplication helper 2A12 for every input 0-255, and the distance 29EA for every pair of values on each of the two axes;
//   B2 the chooser for EVERY combination of ghost tile x target tile x direction in the maze area (tiles a, b = 1E-40), with random walls around the ghost;
//   B3 RANDOM_CASES random choosers (random ghost tile, any 16-bit target, any direction, completely random maze and memory);
//   B4 RANDOM_CASES random runs of each of the four target tasks (random phase, Cruise Elroy, level state, Pac-Man's step bytes (also odd ones), tiles, directions).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/logic/ghost_targeting.h"

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
static logic::WorkRam WR(Mem &x) { return logic::WorkRam{x.work}; }
static logic::VideoRam VR(Mem &x) { return logic::VideoRam{x.video}; }
static void model_task(int g, Mem &x) {
    if (g == 0) logic::blinky_target_task(WR(x), VR(x));
    else if (g == 1) logic::pinky_target_task(WR(x), VR(x));
    else if (g == 2) logic::inky_target_task(WR(x), VR(x));
    else logic::clyde_target_task(WR(x), VR(x));
}
static const unsigned kTaskPc[4] = {0x2730, 0x276C, 0x27A9, 0x27F1};
// a ghost tile whose own tile and four neighbours lie in the video RAM (what the maze always does)
static bool tile_ok(unsigned a, unsigned b) {
    for (int d = -1; d < 4; ++d) {
        unsigned aa = (a + (d < 0 ? 0 : logic::kStepA[d])) & 255, bb = (b + (d < 0 ? 0 : logic::kStepB[d])) & 255,
                 ad = logic::video_address_of_tile((uint8_t)aa, (uint8_t)bb);
        if (ad < 0x4000 || ad >= 0x4400) return false;
    }
    return true;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_ghosts PROG.BIN FRAMES [RANDOM_CASES]\n");
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
        if (++bad <= 8) std::printf("  MISMATCH (%s): address/register %04X, ROM %02X, readable %02X\n", what, where, a, b);
    };

    // ---- Part A
    unsigned long task_calls[4] = {0}, task_scatter[4] = {0}, task_chase[4] = {0}, chooser_calls = 0, turns = 0, dirs[4] = {0}, elroy = 0;
    struct Pend {
        bool on = false;
        unsigned sp = 0, hl = 0, de = 0, a = 0;
        Mem before, expect;
        logic::Choice choice{0, 0};
    } pt[4], pc;
    auto hook = [&](uint16_t pcnow) {
        for (int g = 0; g < 4; ++g) {
            if (pcnow == kTaskPc[g] && !pt[g].on) {
                pt[g].on = true;
                pt[g].sp = m.on_get_sp();
                snapshot(m, pt[g].before);
                pt[g].expect = pt[g].before;
                model_task(g, pt[g].expect);
            } else if (pt[g].on && m.on_get_sp() == pt[g].sp + 2) {
                pt[g].on = false;
                ++task_calls[g];
                Mem got;
                snapshot(m, got);
                unsigned where = 0;
                bool scat = logic::scatter_phase(logic::WorkRam{pt[g].before.work});
                if (g == 0 && pt[0].before.work[logic::kElroyAddress - 0x4C00]) {
                    scat = false;
                    ++elroy;
                }
                if (scat) ++task_scatter[g];
                else ++task_chase[g];
                if (!same(got, pt[g].expect, pt[g].sp, &where))
                    report(g == 0   ? "live Blinky"
                           : g == 1 ? "live Pinky"
                           : g == 2 ? "live Inky"
                                    : "live Clyde",
                           where, got.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0], pt[g].expect.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0]);
            }
        }
        if (pcnow == 0x2966 && !pc.on) {
            pc.on = true;
            pc.sp = m.on_get_sp();
            pc.hl = m.on_get_hl();
            pc.de = m.on_get_de();
            pc.a = m.on_get_a();
            snapshot(m, pc.before);
            pc.expect = pc.before;
            pc.choice = logic::choose_direction(WR(pc.expect), VR(pc.expect), logic::from_word((uint16_t)pc.hl), logic::from_word((uint16_t)pc.de), (uint8_t)pc.a);
        } else if (pc.on && m.on_get_sp() == pc.sp + 2) {
            pc.on = false;
            ++chooser_calls;
            Mem got;
            snapshot(m, got);
            unsigned where = 0;
            if (pc.choice.direction != (pc.a & 255)) { ++turns; }
            ++dirs[pc.choice.direction & 3];
            if (m.on_get_a() != pc.choice.direction) report("live chooser register A", 0xA, m.on_get_a(), pc.choice.direction);
            if (m.on_get_hl() != pc.choice.step) report("live chooser register HL", 0x41, m.on_get_hl() & 255, pc.choice.step & 255);
            if (!same(got, pc.expect, pc.sp, &where))
                report("live chooser memory", where, got.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0], pc.expect.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0]);
        }
    };
    uint32_t rng = 2024;
    int hold = 0, dir = 0x04;
    for (long fr = 0; fr < frames; ++fr) {
        long p = fr % 15000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
        if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20;
        if (p >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
            in0 &= (uint8_t)~dir;
        }
        m.bus.in0 = in0;
        m.bus.in1 = in1;
        m.bus.dsw1 = (uint8_t)(0xC9 | (((fr / 15000) % 3 == 1) ? 0x04 : 0));
        probe::run_frame(m, hook);
    }
    std::printf("part A: %ld frames (%.1f hours of game time): chooser %lu calls (direction changed %lu times; chosen right %lu down %lu left %lu up %lu); target tasks "
                "Blinky %lu (scatter %lu, chase %lu, of which Cruise Elroy %lu), Pinky %lu (scatter %lu, chase %lu), Inky %lu (scatter %lu, chase %lu), Clyde %lu "
                "(scatter %lu, chase %lu); differences %lu\n",
                frames, frames / 60.606 / 3600.0, chooser_calls, turns, dirs[0], dirs[1], dirs[2], dirs[3], task_calls[0], task_scatter[0], task_chase[0], elroy,
                task_calls[1], task_scatter[1], task_chase[1], task_calls[2], task_scatter[2], task_chase[2], task_calls[3], task_scatter[3], task_chase[3], bad);
    unsigned long bad_a = bad;

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
    uint32_t s = 5150;
    auto nxt = [&]() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    };
    auto randomize = [&](Mem &x) {
        for (auto &b : x.work) b = (uint8_t)nxt();
        for (auto &b : x.video) b = (uint8_t)nxt();
        for (auto &b : x.color) b = (uint8_t)nxt();
    };
    auto W = [&](Mem &x, unsigned a, unsigned v) { x.work[a - 0x4C00] = (uint8_t)v; };
    auto nonwall = [&]() {
        uint8_t v;
        do v = (uint8_t)nxt();
        while ((v & 0xC0) == 0xC0);
        return v;
    };
    auto walls = [&](Mem &x, unsigned a, unsigned b, unsigned bits) { // bit d of `bits` = the neighbour in direction d is a wall
        for (unsigned d = 0; d < 4; ++d) {
            unsigned ad = logic::video_address_of_tile((uint8_t)(a + logic::kStepA[d]), (uint8_t)(b + logic::kStepB[d]));
            x.video[ad - 0x4000] = (bits >> d & 1) ? (uint8_t)(0xC0 | (nxt() & 0x3F)) : nonwall();
        }
    };
    unsigned long n_dist = 0;
    for (unsigned v = 0; v < 256; ++v) { // 2A12: A x A
        Mem x;
        randomize(x);
        prepare(x, 0x2A12);
        c.on_set_a((uint8_t)v);
        if (!run()) report("2A12 hang", 0, 0, 0);
        else if (c.on_get_hl() != (unsigned)(v * v)) report("2A12 square", v, c.on_get_hl() & 255, (v * v) & 255);
        ++n_dist;
    }
    for (int axis = 0; axis < 2; ++axis)
        for (unsigned p = 0; p < 256; ++p)
            for (unsigned q = 0; q < 256; ++q) { // 29EA: every pair of values on each axis, the other axis random
                Mem x;
                for (auto &b : x.work) b = (uint8_t)nxt();
                std::memset(x.video, 0, sizeof x.video);
                std::memset(x.color, 0, sizeof x.color);
                unsigned o1 = nxt() & 255, o2 = nxt() & 255;
                logic::Tile P = axis == 0 ? logic::Tile{(uint8_t)p, (uint8_t)o1} : logic::Tile{(uint8_t)o1, (uint8_t)p},
                            Q = axis == 0 ? logic::Tile{(uint8_t)q, (uint8_t)o2} : logic::Tile{(uint8_t)o2, (uint8_t)q};
                W(x, 0x4D40, P.a);
                W(x, 0x4D41, P.b);
                W(x, 0x4D42, Q.a);
                W(x, 0x4D43, Q.b);
                prepare(x, 0x29EA);
                c.on_set_ixh(0x4D);
                c.on_set_ixl(0x40);
                c.on_set_iyh(0x4D);
                c.on_set_iyl(0x42);
                if (!run()) report("29EA hang", 0, 0, 0);
                else if (c.on_get_hl() != logic::squared_distance(P, Q)) report("29EA distance", (p << 8) | q, c.on_get_hl() & 255, logic::squared_distance(P, Q) & 255);
                ++n_dist;
            }
    std::printf(
        "part B1: the multiplication helper (all 256 inputs) and the distance (every pair of values on each axis, 131,072 pairs): %lu calls, differences so far %lu\n",
        n_dist, bad - bad_a);
    unsigned long bad_b1 = bad;

    auto chooser_case = [&](Mem &x, unsigned ga, unsigned gb, unsigned ta, unsigned tb, unsigned d, unsigned long &counter, const char *what) {
        Mem want = x;
        logic::Choice ch = logic::choose_direction(WR(want), VR(want), logic::Tile{(uint8_t)ga, (uint8_t)gb}, logic::Tile{(uint8_t)ta, (uint8_t)tb}, (uint8_t)d);
        prepare(x, 0x2966);
        c.on_set_hl((uint16_t)(ga | (gb << 8)));
        c.on_set_de((uint16_t)(ta | (tb << 8)));
        c.on_set_a((uint8_t)d);
        ++counter;
        if (!run()) {
            report(what, 0, 0, 0);
            return;
        }
        Mem got;
        snapshot(c, got);
        unsigned where = 0;
        if (c.on_get_a() != ch.direction) report(what, 0xA, c.on_get_a(), ch.direction);
        else if (c.on_get_hl() != ch.step) report(what, 0x41, c.on_get_hl() & 255, ch.step & 255);
        else if (!same(got, want, sp0 - 2, &where))
            report(what, where, got.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0], want.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0]);
    };
    unsigned long n_all = 0, valid_ghost = 0;
    {
        std::vector<std::pair<unsigned, unsigned>> ghosts;
        for (unsigned a = 0x1E; a <= 0x40; ++a)
            for (unsigned b = 0x1E; b <= 0x40; ++b)
                if (tile_ok(a, b)) ghosts.push_back({a, b});
        valid_ghost = ghosts.size();
        for (auto &g : ghosts)
            for (unsigned ta = 0x1E; ta <= 0x40; ++ta)
                for (unsigned tb = 0x1E; tb <= 0x40; ++tb)
                    for (unsigned d = 0; d < 4; ++d) {
                        static Mem base;
                        if ((n_all & 1023) == 0) randomize(base);
                        Mem x = base;
                        walls(x, g.first, g.second, nxt() & 15);
                        chooser_case(x, g.first, g.second, ta, tb, d, n_all, "every combination");
                    }
    }
    std::printf("part B2: the chooser for every ghost tile (%lu tiles of the maze area with all four neighbours on the screen) x every target tile of the maze area x 4 "
                "directions, random walls: %lu calls, differences so far %lu\n",
                valid_ghost, n_all, bad - bad_b1);
    unsigned long bad_b2 = bad, n_rand = 0;
    for (long i = 0; i < random_cases; ++i) {
        Mem x;
        randomize(x);
        unsigned ga, gb;
        do {
            ga = nxt() & 255;
            gb = nxt() & 255;
        } while (!tile_ok(ga, gb));
        chooser_case(x, ga, gb, nxt() & 255, nxt() & 255, nxt() & 3, n_rand, "random chooser");
    }
    std::printf("part B3: %lu random choosers (any ghost tile that has its neighbours on the screen, any target, any direction, completely random memory and maze): "
                "differences so far %lu\n",
                n_rand, bad - bad_b2);
    unsigned long bad_b3 = bad, n_task[4] = {0};
    for (long i = 0; i < random_cases; ++i)
        for (int g = 0; g < 4; ++g) {
            Mem x;
            randomize(x);
            unsigned ga, gb;
            do {
                ga = nxt() & 255;
                gb = nxt() & 255;
            } while (!tile_ok(ga, gb));
            W(x, logic::kGhostTileAddress[g], ga);
            W(x, logic::kGhostTileAddress[g] + 1, gb);
            if (g == 2) {
                unsigned ba, bb;
                do {
                    ba = nxt() & 255;
                    bb = nxt() & 255;
                } while (!tile_ok(ba, bb));
                W(x, 0x4D0A, ba);
                W(x, 0x4D0B, bb);
            } // Inky also reads Blinky's tile
            W(x, logic::kGhostDirectionAddress[g], nxt() & 3);
            W(x, logic::kPhaseAddress, (nxt() % 3 == 0) ? (nxt() & 255) : (nxt() & 1));
            W(x, logic::kLevelStateAddress, (nxt() % 4) ? 3 : (nxt() & 255));
            W(x, logic::kElroyAddress, (nxt() % 3) ? 0 : (nxt() & 255));
            unsigned pa = 0x1E + nxt() % 0x23, pb = 0x1E + nxt() % 0x23;
            if (nxt() % 8 == 0) {
                pa = nxt() & 255;
                pb = nxt() & 255;
            }
            W(x, 0x4D39, pa);
            W(x, 0x4D3A, pb);
            if (nxt() % 3) {
                unsigned d = nxt() & 3;
                W(x, 0x4D1C, (uint8_t)logic::kStepA[d]);
                W(x, 0x4D1D, (uint8_t)logic::kStepB[d]);
            } // Pac-Man's step: a real one, else any two bytes
            Mem want = x;
            model_task(g, want);
            prepare(x, kTaskPc[g]);
            ++n_task[g];
            if (!run()) {
                report("task hang", 0, 0, 0);
                continue;
            }
            Mem got;
            snapshot(c, got);
            unsigned where = 0;
            if (!same(got, want, sp0 - 2, &where))
                report(g == 0   ? "random Blinky"
                       : g == 1 ? "random Pinky"
                       : g == 2 ? "random Inky"
                                : "random Clyde",
                       where, got.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0], want.work[where - 0x4C00 < 0x3F0 ? where - 0x4C00 : 0]);
        }
    std::printf("part B4: %lu random runs of each target task (Blinky %lu, Pinky %lu, Inky %lu, Clyde %lu): differences %lu\n", (unsigned long)random_cases, n_task[0],
                n_task[1], n_task[2], n_task[3], bad - bad_b3);
    bool ok = bad == 0 && chooser_calls > 0 && task_calls[0] > 0 && task_calls[1] > 0 && task_calls[2] > 0 && task_calls[3] > 0;
    std::printf("%s: %lu live calls and %lu isolated calls compared, %lu differences\n", ok ? "PASS test_logic_ghosts" : "FAIL test_logic_ghosts",
                chooser_calls + task_calls[0] + task_calls[1] + task_calls[2] + task_calls[3], n_dist + n_all + n_rand + n_task[0] + n_task[1] + n_task[2] + n_task[3],
                bad);
    return ok ? 0 : 1;
}
