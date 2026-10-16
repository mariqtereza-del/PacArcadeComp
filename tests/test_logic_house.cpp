// T02g: the readable ghost house release (src/logic/ghost_house.h) against the ROM, side by side on the real machine.
// Usage: test_logic_house PROG.BIN FRAMES [RANDOM_CASES]
// ROM pieces checked: 1066 (the eaten ghost waiting to become "dead"), 2069 / 208C / 20AF (Pinky, Inky and Clyde leave the house, with 2086, 20A9, 20D1
// inside), and 13DD (the inactivity timer).
// Compared after every call: all work RAM (4C00-4FEF) except the bytes under the stack pointer.
// Part A: a scripted player plays FRAMES frames (many games, other levels, deaths); at every entry of the five routines the readable version gets a copy of
//   the memory; at the return they must match.
// Part B (each routine alone on the real machine):
//   B1 1066: every value of the waiting byte 0-255 x random memory;
//   B2 2069 / 208C / 20AF: every own counter 0-255 x every limit 0-255 x out flag x common-counter mode, and every common counter value 0-255;
//   B3 13DD: every inactivity counter 0-65535 x limit just ahead / equal / elsewhere x dot count changed or not x which ghosts are out;
//   B4 RANDOM_CASES random runs of each routine with completely random memory.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#ifndef HOUSE_HEADER
#define HOUSE_HEADER "../src/logic/ghost_house.h"
#endif
#include HOUSE_HEADER

struct Mem {
    uint8_t work[logic::kWorkRamSize];
};
static const unsigned kStackMargin = 48;
static void snapshot(Machine &m, Mem &x) { std::memcpy(x.work, m.bus.workram, sizeof x.work); }
static bool same(const Mem &rom, const Mem &model, unsigned sp, unsigned *where) {
    for (unsigned i = 0; i < logic::kWorkRamSize; ++i) {
        unsigned a = logic::kWorkRamStart + i;
        if (a + kStackMargin >= sp && a < sp + 2) continue;
        if (rom.work[i] != model.work[i]) {
            *where = a;
            return false;
        }
    }
    return true;
}
static logic::WorkRam WR(Mem &x) { return logic::WorkRam{x.work}; }
static void W(Mem &x, unsigned a, unsigned v) { x.work[a - 0x4C00] = (uint8_t)v; }
static void W16(Mem &x, unsigned a, unsigned v) {
    W(x, a, v & 255);
    W(x, a + 1, v >> 8);
}
static uint8_t R(const Mem &x, unsigned a) { return x.work[a - 0x4C00]; }
static const unsigned kUnitPc[5] = {0x1066, 0x2069, 0x208C, 0x20AF, 0x13DD};
static const char *kUnitName[5] = {"eaten ghost 1066", "Pinky 2069", "Inky 208C", "Clyde 20AF", "inactivity 13DD"};

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_house PROG.BIN FRAMES [RANDOM_CASES]\n");
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
    auto model_unit = [&](int u, Mem &x) {
        if (u == 0) logic::eaten_ghost_waiting_tick(WR(x));
        else if (u <= 3) logic::house_release_check(WR(x), (unsigned)u - 1);
        else logic::inactivity_timer_tick(WR(x));
    };

    // ---- Part A
    unsigned long calls[5] = {0}, effects[5] = {0}, common_mode_calls = 0, common_end = 0;
    struct Pend {
        bool on = false;
        unsigned sp = 0;
        Mem before, expect;
    } pend[5];
    auto hook = [&](uint16_t pcnow) {
        for (int u = 0; u < 5; ++u) {
            if (pcnow == kUnitPc[u] && !pend[u].on) {
                Pend &p = pend[u];
                p.on = true;
                p.sp = m.on_get_sp();
                snapshot(m, p.before);
                p.expect = p.before;
                model_unit(u, p.expect);
            } else if (pend[u].on && m.on_get_sp() == pend[u].sp + 2) {
                Pend &p = pend[u];
                p.on = false;
                ++calls[u];
                Mem got;
                snapshot(m, got);
                unsigned where = 0;
                if (std::memcmp(got.work, p.before.work, sizeof got.work) != 0) ++effects[u];
                if (u >= 1 && u <= 3 && R(p.before, 0x4E12)) ++common_mode_calls;
                if (u == 3 && R(p.before, 0x4E12) && !R(got, 0x4E12)) ++common_end;
                if (!same(got, p.expect, p.sp, &where)) report(kUnitName[u], where, R(got, where), R(p.expect, where));
            }
        }
    };
    uint32_t rng = 11, hold = 0;
    int dir = 0x04;
    for (long fr = 0; fr < frames; ++fr) {
        long p = fr % 15000, game = fr / 15000;
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
        m.bus.dsw1 = (uint8_t)(0xC9 | ((game % 3 == 1) ? 0x04 : 0));
        if (p == 1000 && game % 3 == 1) m.bus.write(0x4E13, (uint8_t)(game % 24));
        probe::run_frame(m, hook);
    }
    std::printf("part A: %ld frames (%.1f hours of game time): calls 1066 %lu (changed memory %lu), Pinky 2069 %lu (%lu), Inky 208C %lu (%lu), Clyde 20AF %lu (%lu), "
                "inactivity 13DD %lu (%lu); release calls in the common counter mode %lu, its end seen %lu times; differences %lu\n",
                frames, frames / 60.606 / 3600.0, calls[0], effects[0], calls[1], effects[1], calls[2], effects[2], calls[3], effects[3], calls[4], effects[4],
                common_mode_calls, common_end, bad);
    const unsigned long bad_a = bad;

    // ---- Part B
    static Machine c(prog);
    const uint16_t sp0 = 0x4FC0, ret = 0x3F00;
    auto prepare = [&](const Mem &in, unsigned pc_start) {
        std::memcpy(c.bus.workram, in.work, sizeof in.work);
        c.bus.write((uint16_t)(sp0 - 2), (uint8_t)(ret & 255));
        c.bus.write((uint16_t)(sp0 - 1), (uint8_t)(ret >> 8));
        c.on_set_sp((uint16_t)(sp0 - 2));
        c.on_set_pc((uint16_t)pc_start);
    };
    auto run = [&]() {
        int guard = 0;
        while (c.on_get_pc() != ret && ++guard < 400000) c.on_step();
        return guard < 400000;
    };
    uint32_t s = 777;
    auto nxt = [&]() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    };
    auto randomize = [&](Mem &x) {
        for (auto &b : x.work) b = (uint8_t)nxt();
    };
    auto run_case = [&](Mem &x, int u, unsigned long &counter, const char *what) {
        Mem want = x;
        model_unit(u, want);
        prepare(x, kUnitPc[u]);
        ++counter;
        if (!run()) {
            report(what, 0, 0, 0);
            return;
        }
        Mem got;
        snapshot(c, got);
        unsigned where = 0;
        if (!same(got, want, sp0 - 2, &where)) report(what, where, R(got, where), R(want, where));
    };
    unsigned long n_b1 = 0, n_b2 = 0, n_b3 = 0, n_rand[5] = {0};
    {
        Mem x;
        randomize(x);
        for (unsigned v = 0; v < 256; ++v)
            for (unsigned rep = 0; rep < 8; ++rep) {
                if (rep == 0) randomize(x);
                W(x, 0x4DAB, v);
                for (unsigned g = 0; g < 4; ++g) W(x, 0x4DAC + g, nxt() % 3 == 0 ? nxt() & 255 : 0);
                run_case(x, 0, n_b1, "eaten ghost every value");
            }
    }
    std::printf("part B1: 1066 alone: every waiting value 0-255 x 8 random fillings: %lu calls, differences so far %lu\n", n_b1, bad - bad_a);
    const unsigned long bad_b1 = bad;
    {
        Mem x;
        randomize(x);
        for (unsigned who = 0; who < 3; ++who) {
            for (unsigned flag = 0; flag < 2; ++flag)
                for (unsigned mode = 0; mode < 2; ++mode) {
                    for (unsigned counter = 0; counter < 256; ++counter)
                        for (unsigned limit = 0; limit < 256; ++limit) {
                            if (((counter ^ limit) & 63) == 0) randomize(x);
                            W(x, 0x4DA1 + who, flag ? 1 + (nxt() & 3) : 0);
                            W(x, 0x4E12, mode ? 1 + (nxt() & 1) : 0);
                            W(x, 0x4E0F + who, counter);
                            W(x, 0x4DB8 + who, limit);
                            W(x, 0x4D9F, nxt() & 255);
                            run_case(x, 1 + who, n_b2, "house release every counter and limit");
                        }
                    for (unsigned common = 0; common < 256; ++common) {
                        randomize(x);
                        W(x, 0x4DA1 + who, flag ? 1 + (nxt() & 3) : 0);
                        W(x, 0x4E12, mode ? 1 + (nxt() & 1) : 0);
                        W(x, 0x4D9F, common);
                        run_case(x, 1 + who, n_b2, "house release every common counter value");
                    }
                }
        }
    }
    std::printf("part B2: the three release checks alone: every own counter x every limit x out flag x common mode, and every common counter value: %lu calls, differences so far %lu\n",
                n_b2, bad - bad_b1);
    const unsigned long bad_b2 = bad;
    {
        Mem x;
        randomize(x);
        for (unsigned counter = 0; counter < 65536; ++counter)
            for (unsigned rel = 0; rel < 3; ++rel)
                for (unsigned out = 0; out < 8; ++out) {
                    if ((counter & 1023) == 0 && rel == 0 && out == 0) randomize(x);
                    W(x, 0x4E0E, nxt() & 255);
                    W(x, 0x4D9E, rel == 2 ? (R(x, 0x4E0E) + 1 + nxt() % 255) & 255 : R(x, 0x4E0E));
                    W16(x, 0x4D97, counter);
                    W16(x, 0x4D95, rel == 0 ? (counter + 1) & 0xFFFF : rel == 1 ? nxt() & 0xFFFF : counter);
                    for (unsigned g = 0; g < 3; ++g) W(x, 0x4DA1 + g, (out >> g & 1) ? 1 + (nxt() & 3) : 0);
                    run_case(x, 4, n_b3, "inactivity every counter value");
                }
    }
    std::printf("part B3: 13DD alone: every counter 0-65535 x limit ahead/elsewhere/equal x dot count same/changed x which ghosts are out: %lu calls, differences so far %lu\n",
                n_b3, bad - bad_b2);
    const unsigned long bad_b3 = bad;
    for (long i = 0; i < random_cases; ++i) {
        for (int u = 0; u < 5; ++u) {
            Mem x;
            randomize(x);
            if (u == 0 && (nxt() & 1)) W(x, 0x4DAB, nxt() % 6);
            if (u >= 1 && u <= 3) {
                W(x, 0x4DA1 + u - 1, nxt() % 3 ? 0 : nxt() & 3);
                W(x, 0x4E12, nxt() % 3 ? 0 : 1);
                W(x, 0x4D9F, nxt() % 2 ? "\x07\x11\x20"[u - 1] : nxt() & 255);
                if (nxt() & 1) W(x, 0x4DB8 + u - 1, R(x, 0x4E0F + u - 1) + (int)(nxt() % 3) - 1);
            }
            if (u == 4) {
                if (nxt() & 1) W(x, 0x4D9E, R(x, 0x4E0E));
                if (nxt() & 1) W16(x, 0x4D95, (R(x, 0x4D97) | (R(x, 0x4D98) << 8)) + 1);
                for (unsigned g = 0; g < 3; ++g) W(x, 0x4DA1 + g, nxt() & 1 ? 0 : nxt() & 3);
            }
            run_case(x, u, n_rand[u], "random");
        }
    }
    std::printf("part B4: %lu random runs of each routine: differences %lu\n", (unsigned long)random_cases, bad - bad_b3);
    bool ok = bad == 0 && calls[0] > 0 && calls[1] > 0 && calls[2] > 0 && calls[3] > 0 && calls[4] > 0;
    std::printf("%s: %lu live calls and %lu isolated calls compared, %lu differences\n", ok ? "PASS test_logic_house" : "FAIL test_logic_house",
                calls[0] + calls[1] + calls[2] + calls[3] + calls[4], n_b1 + n_b2 + n_b3 + n_rand[0] + n_rand[1] + n_rand[2] + n_rand[3] + n_rand[4], bad);
    return ok ? 0 : 1;
}
