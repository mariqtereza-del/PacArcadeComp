// T01b: the readable scoring (src/logic/scoring.h) against the ROM routine add_score at 2A5A, side by side on the real machine.
// Usage: test_logic_scoring PROG.BIN FRAMES [RANDOM_CASES [SCORE_LIMIT]]
// What is compared: all of the work RAM (4C00-4FEF) after the routine, except the few bytes under the stack pointer that the routine's own CALLs and
// PUSHes use. The video RAM (the redrawing of the score and of the little Pac-Mans) is not compared: it is not part of the rules.
// Part A: a scripted player plays FRAMES frames (many games, both players' turns are reached through the DIP switch for two players via start 2);
//         at every call of 2A5A the readable version gets a copy of the work RAM and the same points number; at the return the two must be identical.
// Part B: the routine alone on the real machine: (1) every score from 000000 to 999999 (SCORE_LIMIT shortens it for quick runs) with every one of the 14 points kinds (14,000,000 calls);
//         (2) RANDOM_CASES random situations (both players, demo or game, all four DIP limits, flags, lives, high scores equal or near the score).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/logic/scoring.h"

static const unsigned kStackMargin = 48; // bytes below the entry stack pointer the routine may use for its calls

static bool same_ram(const uint8_t *rom_ram, const uint8_t *model_ram, unsigned sp, unsigned *where) {
    for (unsigned i = 0; i < logic::kWorkRamSize; ++i) {
        unsigned a = logic::kWorkRamStart + i;
        if (a + kStackMargin >= sp && a < sp + 2) continue; // the routine's stack scratch space
        if (rom_ram[i] != model_ram[i]) {
            *where = a;
            return false;
        }
    }
    return true;
}
static unsigned bcd6(unsigned v) {
    unsigned r = 0;
    for (int s = 0; s < 24; s += 4) {
        r |= (v % 10) << s;
        v /= 10;
    }
    return r;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_scoring PROG.BIN FRAMES [RANDOM_CASES]\n");
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
    const long random_cases = argc > 3 ? std::atol(argv[3]) : 4000000;
    const unsigned score_limit = argc > 4 ? (unsigned)std::atol(argv[4]) : 1000000; // the 4th argument shortens part B1 for quick runs
    static Machine m(prog);
    unsigned long calls = 0, bad = 0, kind_count[16] = {0}, by_player[2] = {0}, demo_calls = 0, extra_lives = 0, new_highs = 0;
    bool pending = false;
    unsigned sp0 = 0, idx0 = 0;
    uint8_t expect[logic::kWorkRamSize], before_ram[logic::kWorkRamSize];
    auto report = [&](const char *what, unsigned where, unsigned a, unsigned b) {
        if (++bad <= 6) std::printf("  MISMATCH (%s): address %04X, ROM %02X, readable %02X\n", what, where, a, b);
    };
    auto hook = [&](uint16_t pc) {
        if (pc == 0x2A5A && !pending) {
            pending = true;
            sp0 = m.on_get_sp();
            idx0 = (m.on_get_bc() >> 8) & 255;
            std::memcpy(before_ram, m.bus.workram, sizeof before_ram);
            std::memcpy(expect, before_ram, sizeof expect);
            if (idx0 < logic::kPointsKinds) logic::add_score(logic::WorkRam{expect}, idx0);
        } else if (pending && m.on_get_sp() == sp0 + 2) { // the instruction after the RET
            pending = false;
            ++calls;
            unsigned where = 0;
            if (idx0 >= logic::kPointsKinds) {
                report("points number outside the table", idx0, 0, 0);
                return;
            }
            ++kind_count[idx0];
            unsigned pl = before_ram[0x4E09 - 0x4C00] != 0;
            ++by_player[pl];
            if (before_ram[0x4E00 - 0x4C00] == 1) ++demo_calls;
            if (expect[0x4E14 - 0x4C00] != before_ram[0x4E14 - 0x4C00]) ++extra_lives;
            if (std::memcmp(expect + 0x4E88 - 0x4C00, before_ram + 0x4E88 - 0x4C00, 3)) ++new_highs;
            if (!same_ram(m.bus.workram, expect, sp0, &where)) report("live call", where, m.bus.workram[where - 0x4C00], expect[where - 0x4C00]);
        }
    };
    // ---- Part A
    uint32_t rng = 777;
    int hold = 0, dir = 0x04;
    for (long fr = 0; fr < frames; ++fr) {
        long p = fr % 15000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        const bool two = (fr / 15000) % 3 == 2; // every third game is a two player game
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
        if (two && p >= 410 && p < 415) in0 &= (uint8_t)~0x20; // a second coin
        if (p >= 420 && p < 424) in1 &= (uint8_t) ~(two ? 0x40 : 0x20);
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
        m.bus.dsw1 = (uint8_t)(0xC9 | (((fr / 15000) % 4 == 1) ? 0x04 : 0)); // some games with 5 lives
        probe::run_frame(m, hook);
    }
    std::printf("part A: %ld frames (%.1f hours of game time), %lu calls of 2A5A compared: dots %lu, energizers %lu, ghosts %lu/%lu/%lu/%lu, fruit and bonus %lu; player "
                "1 %lu, player 2 %lu, in the demo %lu; extra lives given %lu; high scores raised %lu; differences %lu\n",
                frames, frames / 60.606 / 3600.0, calls, kind_count[0], kind_count[1], kind_count[2], kind_count[3], kind_count[4], kind_count[5],
                kind_count[6] + kind_count[7] + kind_count[8] + kind_count[9] + kind_count[10] + kind_count[11] + kind_count[12] + kind_count[13], by_player[0],
                by_player[1], demo_calls, extra_lives, new_highs, bad);
    unsigned long bad_a = bad;

    // ---- Part B: the routine alone
    static Machine c(prog);
    auto isolated = [&](unsigned idx, const uint8_t *ram_in, unsigned long &counter, const char *what) {
        std::memcpy(c.bus.workram, ram_in, logic::kWorkRamSize);
        const uint16_t sp = 0x4FC0, ret = 0x3F00;
        c.bus.write((uint16_t)(sp - 2), (uint8_t)(ret & 255));
        c.bus.write((uint16_t)(sp - 1), (uint8_t)(ret >> 8));
        c.on_set_sp((uint16_t)(sp - 2));
        c.on_set_pc(0x2A5A);
        c.on_set_bc((uint16_t)(idx << 8));
        uint8_t want[logic::kWorkRamSize];
        std::memcpy(want, ram_in, sizeof want);
        logic::add_score(logic::WorkRam{want}, idx);
        int guard = 0;
        while (c.on_get_pc() != ret && ++guard < 20000) c.on_step();
        unsigned where = 0;
        ++counter;
        if (guard >= 20000) report(what, 0, 0, 0);
        else if (!same_ram(c.bus.workram, want, sp - 2, &where)) report(what, where, c.bus.workram[where - 0x4C00], want[where - 0x4C00]);
    };
    uint8_t base[logic::kWorkRamSize];
    std::memset(base, 0, sizeof base);
    auto W = [&](uint8_t *r, unsigned a, unsigned v) { r[a - 0x4C00] = (uint8_t)v; };
    unsigned long n_all = 0;
    for (unsigned score = 0; score < score_limit; ++score) {
        unsigned b = bcd6(score);
        for (unsigned idx = 0; idx < logic::kPointsKinds; ++idx) {
            uint8_t r[logic::kWorkRamSize];
            std::memcpy(r, base, sizeof r);
            W(r, 0x4E00, 3);
            W(r, 0x4E09, 0);
            W(r, 0x4E71, 0x10);
            W(r, 0x4E14, 3);
            W(r, 0x4E15, 3);
            W(r, 0x4E80, b & 255);
            W(r, 0x4E81, (b >> 8) & 255);
            W(r, 0x4E82, (b >> 16) & 255);
            isolated(idx, r, n_all, "every score");
        }
    }
    std::printf("part B1: every score 000000-999999 with each of the 14 points kinds, run through the ROM routine alone: %lu calls, differences so far %lu\n", n_all,
                bad - bad_a);
    unsigned long bad_b1 = bad;
    unsigned long n_rand = 0;
    uint32_t s = 4242;
    auto nxt = [&]() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    };
    for (long i = 0; i < random_cases; ++i) {
        uint8_t r[logic::kWorkRamSize];
        for (unsigned k = 0; k < logic::kWorkRamSize; ++k) r[k] = (uint8_t)nxt(); // everything random first: garbage must not matter
        unsigned pl = nxt() & 1, a = pl ? 0x4E84 : 0x4E80;
        unsigned sc = (nxt() % 3 == 0) ? (nxt() % 1000000) : (nxt() % 4 == 0 ? 9900 + nxt() % 200 : nxt() % 40000) + (nxt() % 5 == 0 ? 10000 * (1 + nxt() % 3) - 100 : 0);
        sc %= 1000000;
        unsigned b = bcd6(sc);
        W(r, a, b & 255);
        W(r, a + 1, (b >> 8) & 255);
        W(r, a + 2, (b >> 16) & 255);
        W(r, a + 3, (nxt() & 1) ? 0x01 : (nxt() & 0xFE));
        unsigned o = pl ? 0x4E80 : 0x4E84, ob = bcd6(nxt() % 1000000);
        W(r, o, ob & 255);
        W(r, o + 1, (ob >> 8) & 255);
        W(r, o + 2, (ob >> 16) & 255);
        W(r, o + 3, nxt() & 255); // the other player's score: must not be touched
        static const unsigned limits[] = {0x10, 0x15, 0x20, 0xFF, 0x00, 0x01, 0x09, 0x99};
        W(r, 0x4E71, (nxt() % 3) ? limits[nxt() % 4] : limits[nxt() % 8]);
        W(r, 0x4E00, (nxt() % 6 == 0) ? 1 : 3);
        W(r, 0x4E09, pl);
        W(r, 0x4E14, nxt() & 255);
        W(r, 0x4E15, nxt() & 255);
        unsigned hs;
        switch (nxt() % 4) {
        case 0: hs = sc; break;
        case 1: hs = sc ? sc - 1 : 0; break;
        case 2: hs = (sc + 1) % 1000000; break;
        default: hs = nxt() % 1000000;
        }
        unsigned hb = bcd6(hs);
        W(r, 0x4E88, hb & 255);
        W(r, 0x4E89, (hb >> 8) & 255);
        W(r, 0x4E8A, (hb >> 16) & 255);
        isolated(nxt() % logic::kPointsKinds, r, n_rand, "random case");
    }
    std::printf("part B2: %lu random situations (both players, demo and game, all limits, flags, lives, high scores equal or next to the score): differences %lu\n",
                n_rand, bad - bad_b1);
    bool ok = bad == 0 && calls > 0 && n_all == 14UL * score_limit;
    std::printf("%s: %lu live calls and %lu isolated calls compared, %lu differences\n", ok ? "PASS test_logic_scoring" : "FAIL test_logic_scoring", calls,
                n_all + n_rand, bad);
    return ok ? 0 : 1;
}
