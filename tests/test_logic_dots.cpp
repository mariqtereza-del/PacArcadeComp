// T01c: the readable dots and fruit (src/logic/dots_and_fruit.h) against the ROM, side by side on the real machine.
// Usage: test_logic_dots PROG.BIN FRAMES [RANDOM_CASES]
// Four ROM pieces are checked, each against its readable twin:
//   EAT   19CD  Pac-Man's tile: eat a dot or an energizer (includes the ghost house counting 1B08 and the power pill start 1A70)
//   FRUIT 0EAD  show the fruit at 70 and 170 dots
//   COUNT 1B08  which ghost counts the dot (also on its own)
// Compared after every call: all work RAM (4C00-4FEF), the video RAM (4000-43FF, the maze) and the colour RAM (4400-47FF), except the few bytes under the
// stack pointer that the routine's own CALLs use.
// Part A: a scripted player plays FRAMES frames; at every entry of 19CD and of 0EAD the readable version gets a copy of the memory; at the return they must match.
// Part B: each piece alone on the real machine: B1 every Pac-Man tile (a, b) that lies in the video RAM x dot / energizer / blank tile (65,536 tile positions);
//         B2 RANDOM_CASES random situations for EAT, FRUIT and COUNT each (the whole work RAM random, then the interesting bytes set: dot counts 69-71 and
//         169-171, all levels 0-255, every flag, the task queue pointer anywhere in its page, the timed event table nearly full, ...).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/logic/dots_and_fruit.h"

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
static void model_eat(Mem &x) { logic::pacman_eats_on_tile(logic::WorkRam{x.work}, logic::VideoRam{x.video}); }
static void model_fruit(Mem &x) { logic::maybe_show_fruit(logic::WorkRam{x.work}); }
static void model_count(Mem &x) { logic::count_dot_for_ghost_house(logic::WorkRam{x.work}); }

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_dots PROG.BIN FRAMES [RANDOM_CASES]\n");
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
        if (++bad <= 6) std::printf("  MISMATCH (%s): address %04X, ROM %02X, readable %02X\n", what, where, a, b);
    };

    // ---- Part A
    unsigned long eat_calls = 0, dots = 0, energizers = 0, nothing = 0, fruit_calls = 0, fruit_shown = 0, fruit_by_kind[16] = {0}, ghost_house_cases[5] = {0};
    struct Pend {
        bool on = false;
        unsigned sp = 0;
        Mem before, expect;
    } pe, pf;
    auto hook = [&](uint16_t pc) {
        if (pc == 0x19CD && !pe.on) {
            pe.on = true;
            pe.sp = m.on_get_sp();
            snapshot(m, pe.before);
            pe.expect = pe.before;
            model_eat(pe.expect);
        } else if (pe.on && m.on_get_sp() == pe.sp + 2) {
            pe.on = false;
            ++eat_calls;
            Mem got;
            snapshot(m, got);
            unsigned where = 0;
            uint8_t tile_before = pe.before.video[logic::video_address_of_tile(pe.before.work[0x4D39 - 0x4C00], pe.before.work[0x4D3A - 0x4C00]) - 0x4000 < 0x400
                                                      ? logic::video_address_of_tile(pe.before.work[0x4D39 - 0x4C00], pe.before.work[0x4D3A - 0x4C00]) - 0x4000
                                                      : 0];
            if (tile_before == 0x10) ++dots;
            else if (tile_before == 0x14) ++energizers;
            else ++nothing;
            if (tile_before == 0x10 || tile_before == 0x14) {
                unsigned c = pe.before.work[0x4E12 - 0x4C00]   ? 0
                             : pe.before.work[0x4DA3 - 0x4C00] ? 1
                             : pe.before.work[0x4DA2 - 0x4C00] ? 2
                             : pe.before.work[0x4DA1 - 0x4C00] ? 3
                                                               : 4;
                ++ghost_house_cases[c];
            }
            if (!same(got, pe.expect, pe.sp, &where))
                report("live EAT", where, where >= 0x4C00 ? got.work[where - 0x4C00] : 0, where >= 0x4C00 ? pe.expect.work[where - 0x4C00] : 0);
        }
        if (pc == 0x0EAD && !pf.on) {
            pf.on = true;
            pf.sp = m.on_get_sp();
            snapshot(m, pf.before);
            pf.expect = pf.before;
            model_fruit(pf.expect);
        } else if (pf.on && m.on_get_sp() == pf.sp + 2) {
            pf.on = false;
            ++fruit_calls;
            Mem got;
            snapshot(m, got);
            unsigned where = 0;
            if (pf.expect.work[0x4DD4 - 0x4C00] != pf.before.work[0x4DD4 - 0x4C00]) {
                ++fruit_shown;
                ++fruit_by_kind[pf.expect.work[0x4DD4 - 0x4C00] & 15];
            }
            if (!same(got, pf.expect, pf.sp, &where))
                report("live FRUIT", where, where >= 0x4C00 ? got.work[where - 0x4C00] : 0, where >= 0x4C00 ? pf.expect.work[where - 0x4C00] : 0);
        }
    };
    uint32_t rng = 31337;
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
    std::printf("part A: %ld frames (%.1f hours of game time): EAT %lu calls (dot %lu, energizer %lu, nothing to eat %lu; ghost house: common counter %lu, nobody counts "
                "%lu, Clyde %lu, Inky %lu, Pinky %lu); FRUIT %lu calls, fruit shown %lu times (by points kind: cherry %lu strawberry %lu orange %lu apple %lu melon %lu "
                "galaxian %lu bell %lu key %lu); differences %lu\n",
                frames, frames / 60.606 / 3600.0, eat_calls, dots, energizers, nothing, ghost_house_cases[0], ghost_house_cases[1], ghost_house_cases[2],
                ghost_house_cases[3], ghost_house_cases[4], fruit_calls, fruit_shown, fruit_by_kind[6], fruit_by_kind[7], fruit_by_kind[8], fruit_by_kind[9],
                fruit_by_kind[10], fruit_by_kind[11], fruit_by_kind[12], fruit_by_kind[13], bad);
    unsigned long bad_a = bad;

    // ---- Part B: each piece alone
    static Machine c(prog);
    auto isolated = [&](unsigned pc, const Mem &in, void (*model)(Mem &), unsigned long &counter, const char *what) {
        std::memcpy(c.bus.workram, in.work, sizeof in.work);
        std::memcpy(c.bus.videoram, in.video, sizeof in.video);
        std::memcpy(c.bus.colorram, in.color, sizeof in.color);
        const uint16_t sp = 0x4FC0, ret = 0x3F00;
        c.bus.write((uint16_t)(sp - 2), (uint8_t)(ret & 255));
        c.bus.write((uint16_t)(sp - 1), (uint8_t)(ret >> 8));
        c.on_set_sp((uint16_t)(sp - 2));
        c.on_set_pc((uint16_t)pc);
        Mem want = in;
        model(want);
        int guard = 0;
        while (c.on_get_pc() != ret && ++guard < 20000) c.on_step();
        Mem got;
        snapshot(c, got);
        unsigned where = 0;
        ++counter;
        if (guard >= 20000) report(what, 0, 0, 0);
        else if (!same(got, want, sp - 2, &where))
            report(what, where, where >= 0x4C00 && where < 0x5000 ? got.work[where - 0x4C00] : 0, where >= 0x4C00 && where < 0x5000 ? want.work[where - 0x4C00] : 0);
    };
    uint32_t s = 99991;
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
    unsigned long n_tiles = 0, valid_tiles = 0;
    static const uint8_t tiles[3] = {0x10, 0x14, 0x40};
    for (unsigned a = 0; a < 256; ++a)
        for (unsigned b = 0; b < 256; ++b) {
            unsigned addr = logic::video_address_of_tile((uint8_t)a, (uint8_t)b);
            if (addr < 0x4000 || addr >= 0x4400) continue;
            ++valid_tiles;
            for (int t = 0; t < 3; ++t) {
                Mem x;
                randomize(x);
                W(x, 0x4D39, a);
                W(x, 0x4D3A, b);
                x.video[addr - 0x4000] = tiles[t];
                W(x, 0x4C81, 0x4C);
                isolated(0x19CD, x, model_eat, n_tiles, "every tile");
            }
        }
    std::printf(
        "part B1: every Pac-Man tile position that lies in the video RAM (%lu of 65,536) x dot, energizer, blank: %lu calls of 19CD alone, differences so far %lu\n",
        valid_tiles, n_tiles, bad - bad_a);
    unsigned long bad_b1 = bad, n_eat = 0, n_fruit = 0, n_count = 0;
    for (long i = 0; i < random_cases; ++i) {
        {
            Mem x;
            randomize(x); // EAT
            unsigned a = 32 + nxt() % 40, b = 30 + nxt() % 40;
            if (nxt() % 16 == 0) {
                a = nxt() & 255;
                b = nxt() & 255;
            }
            unsigned addr = logic::video_address_of_tile((uint8_t)a, (uint8_t)b);
            if (addr < 0x4000 || addr >= 0x4400) {
                a = 40;
                b = 40;
                addr = logic::video_address_of_tile(40, 40);
            }
            W(x, 0x4D39, a);
            W(x, 0x4D3A, b);
            static const uint8_t t3[4] = {0x10, 0x14, 0x40, 0x10};
            x.video[addr - 0x4000] = (nxt() % 5 == 0) ? (uint8_t)nxt() : t3[nxt() % 4];
            W(x, 0x4C81, 0x4C);
            if (nxt() % 3 == 0) W(x, 0x4C80, 0xF0 + nxt() % 16); // the task queue pointer often next to the end of its page
            {
                if (nxt() % 2) W(x, 0x4E12, 0);
                if (nxt() % 2) W(x, 0x4DA3, 0);
                if (nxt() % 2) W(x, 0x4DA2, 0);
                if (nxt() % 2) W(x, 0x4DA1, 0);
            }
            isolated(0x19CD, x, model_eat, n_eat, "random EAT");
        }
        {
            Mem x;
            randomize(x); // FRUIT
            static const uint8_t dotsv[] = {70, 170, 69, 71, 169, 171, 0, 244};
            W(x, 0x4E0E, (nxt() % 3) ? dotsv[nxt() % 8] : (nxt() & 255));
            {
                if (nxt() % 2) W(x, 0x4DA5, 0);
                if (nxt() % 2) W(x, 0x4DD4, 0);
                if (nxt() % 2) W(x, 0x4E0C, 0);
                if (nxt() % 2) W(x, 0x4E0D, 0);
            }
            if (nxt() % 2) W(x, 0x4E13, nxt() % 24); // the level (otherwise any byte: the clamp to level 21)
            W(x, 0x4C90 + 3 * (nxt() % 16), 0);      // at least one free timed event slot
            if (nxt() % 2)
                for (unsigned k = 0; k < 16; ++k)
                    if (nxt() % 3) W(x, 0x4C90 + 3 * k, 1 + nxt() % 255); // the table nearly full, but one free slot (the last cleared above stays free only sometimes)
            {
                bool free_slot = false;
                for (unsigned k = 0; k < 16; ++k)
                    if (x.work[0x4C90 + 3 * k - 0x4C00] == 0) free_slot = true;
                if (!free_slot) W(x, 0x4C90 + 3 * (nxt() % 16), 0);
            }
            isolated(0x0EAD, x, model_fruit, n_fruit, "random FRUIT");
        }
        {
            Mem x;
            randomize(x); // COUNT
            {
                if (nxt() % 2) W(x, 0x4E12, 0);
                if (nxt() % 2) W(x, 0x4DA3, 0);
                if (nxt() % 2) W(x, 0x4DA2, 0);
                if (nxt() % 2) W(x, 0x4DA1, 0);
            }
            if (nxt() % 4 == 0) {
                W(x, 0x4D9F, 255);
                W(x, 0x4E0F, 255);
                W(x, 0x4E10, 255);
                W(x, 0x4E11, 255);
            } // counters about to wrap
            isolated(0x1B08, x, model_count, n_count, "random COUNT");
        }
    }
    std::printf("part B2: %lu random situations each for EAT (%lu), FRUIT (%lu), COUNT (%lu): differences %lu\n", (unsigned long)random_cases, n_eat, n_fruit, n_count,
                bad - bad_b1);
    bool ok = bad == 0 && eat_calls > 0 && fruit_calls > 0 && fruit_shown > 0;
    std::printf("%s: %lu live calls and %lu isolated calls compared, %lu differences\n", ok ? "PASS test_logic_dots" : "FAIL test_logic_dots", eat_calls + fruit_calls,
                n_tiles + n_eat + n_fruit + n_count, bad);
    return ok ? 0 : 1;
}
