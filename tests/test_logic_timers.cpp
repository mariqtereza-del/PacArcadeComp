// T01f: the readable mode timers (src/logic/mode_timers.h) against the ROM, side by side on the real machine.
// Usage: test_logic_timers PROG.BIN FRAMES [RANDOM_CASES]
// ROM pieces checked: the scatter/chase timer 0E36, the frightened timer 1376, and the level start 070E (which copies the level's speed patterns, phase limits,
// ghost house limits, Cruise Elroy limits, frightened time and inactivity time).
// Compared after every call: all work RAM (4C00-4FEF) except the bytes under the stack pointer; for 0E36 and 1376 also the video and colour RAM.
// For 070E only the work RAM (the ROM also draws the level's fruits in video RAM, ROM 2BEA, which is not copied).
// Part A: a scripted player plays FRAMES frames; at every entry of the three routines the readable version gets a copy of the memory; at the return they must match.
// Part B (each routine alone on the real machine):
//   B1 0E36: every phase number 0-255 x frightened or not x the counter just before, at and after the phase limit, and every counter value 0-65535 for each phase 0-7;
//   B2 1376: every combination of frightened flag (0,1,2) x the four blue flags x the four "dead" flags x counter values around 0, 1, 2, 65535 (and random);
//   B3 070E: every level row 0-20 (also reached through the demo pointer 4E0A with the level number 0) on random memory;
//   B4 RANDOM_CASES random runs of each routine with completely random memory.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/logic/mode_timers.h"

struct Mem {
    uint8_t work[logic::kWorkRamSize], video[logic::kVideoRamSize], color[0x400];
};
static const unsigned kStackMargin = 48;
static void snapshot(Machine &m, Mem &x) {
    std::memcpy(x.work, m.bus.workram, sizeof x.work);
    std::memcpy(x.video, m.bus.videoram, sizeof x.video);
    std::memcpy(x.color, m.bus.colorram, sizeof x.color);
}
static bool same(const Mem &rom, const Mem &model, unsigned sp, bool screen_too, unsigned *where) {
    for (unsigned i = 0; i < logic::kWorkRamSize; ++i) {
        unsigned a = logic::kWorkRamStart + i;
        if (a + kStackMargin >= sp && a < sp + 2) continue;
        if (rom.work[i] != model.work[i]) {
            *where = a;
            return false;
        }
    }
    if (!screen_too) return true;
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
static void W(Mem &x, unsigned a, unsigned v) { x.work[a - 0x4C00] = (uint8_t)v; }
static void W16(Mem &x, unsigned a, unsigned v) {
    W(x, a, v & 255);
    W(x, a + 1, v >> 8);
}
static uint8_t R(const Mem &x, unsigned a) { return x.work[a - 0x4C00]; }
static const unsigned kUnitPc[3] = {0x0E36, 0x1376, 0x070E};
static const char *kUnitName[3] = {"mode timer 0E36", "frightened timer 1376", "level start 070E"};

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_timers PROG.BIN FRAMES [RANDOM_CASES]\n");
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
    // the level row the ROM uses for a call with register B (B = 0: the byte the pointer 4E0A points at, in the program ROM)
    auto level_row = [&](const Mem &x, unsigned b) { return b != 0 ? b : prog[(R(x, 0x4E0A) | (R(x, 0x4E0B) << 8)) & 0x3FFF]; };
    auto model_unit = [&](int u, Mem &x, unsigned b) {
        if (u == 0) logic::mode_timer_tick(WR(x));
        else if (u == 1) logic::frightened_timer_tick(WR(x));
        else logic::start_level_timers(WR(x), level_row(x, b));
    };

    // ---- Part A
    unsigned long calls[3] = {0}, phase_changes = 0, frozen = 0, fright_end = 0, fright_count = 0, level_starts_by_row[22] = {0}, out_of_table = 0;
    unsigned phase_seen[8] = {0};
    struct Pend {
        bool on = false;
        unsigned sp = 0, b = 0;
        Mem before, expect;
    } pend[3];
    auto hook = [&](uint16_t pcnow) {
        for (int u = 0; u < 3; ++u) {
            if (pcnow == kUnitPc[u] && !pend[u].on) {
                Pend &p = pend[u];
                p.on = true;
                p.sp = m.on_get_sp();
                p.b = (m.on_get_bc() >> 8) & 255;
                snapshot(m, p.before);
                p.expect = p.before;
                if (u == 2 && level_row(p.before, p.b) >= logic::kLevelCount) {
                    p.on = false;
                    ++out_of_table;
                    continue;
                } // a level number the table does not cover: not modelled
                model_unit(u, p.expect, p.b);
            } else if (pend[u].on && m.on_get_sp() == pend[u].sp + 2) {
                Pend &p = pend[u];
                p.on = false;
                ++calls[u];
                Mem got;
                snapshot(m, got);
                unsigned where = 0;
                if (u == 0) {
                    if (R(got, 0x4DC1) != R(p.before, 0x4DC1)) ++phase_changes;
                    if (R(p.before, 0x4DA6)) ++frozen;
                    if (R(got, 0x4DC1) < 8) ++phase_seen[R(got, 0x4DC1)];
                }
                if (u == 1) {
                    if (R(p.before, 0x4DA6)) ++fright_count;
                    if (R(p.before, 0x4DA6) && !R(got, 0x4DA6)) ++fright_end;
                }
                if (u == 2) ++level_starts_by_row[level_row(p.before, p.b)];
                if (!same(got, p.expect, p.sp, u != 2, &where)) report(kUnitName[u], where, byte_at(got, where), byte_at(p.expect, where));
            }
        }
    };
    uint32_t rng = 11, hold = 0;
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
    unsigned rows_seen = 0;
    for (unsigned i = 0; i < 21; ++i)
        if (level_starts_by_row[i]) ++rows_seen;
    std::printf("part A: %ld frames (%.1f hours of game time): mode timer %lu calls (phase changes %lu, frozen by a power pill %lu; phases reached 0-7: %u %u %u %u %u "
                "%u %u %u), frightened timer %lu calls (%lu while frightened, the end seen %lu times), level start %lu calls (%u different level rows, %lu calls with a "
                "level number outside the table, not compared); differences %lu\n",
                frames, frames / 60.606 / 3600.0, calls[0], phase_changes, frozen, phase_seen[0], phase_seen[1], phase_seen[2], phase_seen[3], phase_seen[4],
                phase_seen[5], phase_seen[6], phase_seen[7], calls[1], fright_count, fright_end, calls[2], rows_seen, out_of_table, bad);
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
        for (auto &b : x.video) b = (uint8_t)nxt();
        for (auto &b : x.color) b = (uint8_t)nxt();
    };
    auto run_case = [&](Mem &x, int u, unsigned b, unsigned long &counter, const char *what) {
        W(x, 0x4C80, 0xC0 + nxt() % 0x40);
        W(x, 0x4C81, 0x4C);
        Mem want = x;
        model_unit(u, want, b);
        prepare(x, kUnitPc[u]);
        if (u == 2) c.on_set_bc((uint16_t)(b << 8 | (nxt() & 255)));
        ++counter;
        if (!run()) {
            report(what, 0, 0, 0);
            return;
        }
        Mem got;
        snapshot(c, got);
        unsigned where = 0;
        if (!same(got, want, sp0 - 2, u != 2, &where)) report(what, where, byte_at(got, where), byte_at(want, where));
    };
    unsigned long n_b1 = 0, n_b2 = 0, n_b3 = 0, n_rand[3] = {0};
    // B1: the phase timer
    {
        Mem x;
        randomize(x);
        for (unsigned fright = 0; fright < 2; ++fright)
            for (unsigned phase = 0; phase < 256; ++phase)
                for (unsigned k = 0; k < 20; ++k) {
                    if (k == 0) randomize(x);
                    W(x, 0x4DA6, fright ? 1 + (nxt() & 1) : 0);
                    W(x, 0x4DC1, phase);
                    unsigned limit = (R(x, 0x4D86 + ((2 * phase) & 255)) | (R(x, 0x4D87 + ((2 * phase) & 255)) << 8));
                    if (phase < 8 && (nxt() & 1)) {
                        limit = 1 + nxt() % 7000;
                        W16(x, 0x4D86 + 2 * phase, limit);
                    }
                    static const int around[7] = {-2, -1, 0, 1, 2, 3, 0};
                    unsigned counter = k < 7 ? (limit + around[k]) & 0xFFFF : nxt() & 0xFFFF;
                    W16(x, 0x4DC2, counter);
                    for (unsigned g = 0; g < 4; ++g) W(x, 0x4DB1 + g, nxt() & 1);
                    run_case(x, 0, 0, n_b1, "phase timer every phase");
                }
        for (unsigned phase = 0; phase < 8; ++phase)
            for (unsigned counter = 0; counter < 65536; ++counter) { // every counter value for each of the eight phases
                if ((counter & 4095) == 0) randomize(x);
                W(x, 0x4DA6, 0);
                W(x, 0x4DC1, phase);
                W16(x, 0x4DC2, counter);
                W16(x, 0x4D86 + 2 * phase, (counter + 1) & 0xFFFF);
                if (counter & 1) W16(x, 0x4D86 + 2 * phase, nxt() & 0xFFFF);
                run_case(x, 0, 0, n_b1, "phase timer every counter value");
            }
    }
    std::printf("part B1: the phase timer alone: every phase number 0-255 x frightened x counter around the limit, and every counter value 0-65535 for each phase 0-7: "
                "%lu calls, differences so far %lu\n",
                n_b1, bad - bad_a);
    const unsigned long bad_b1 = bad;
    // B2: the frightened timer
    {
        Mem x;
        randomize(x);
        static const unsigned counters[10] = {0, 1, 2, 3, 0xFFFF, 0x0100, 0x0101, 0x8000, 0x1234, 0};
        for (unsigned fr = 0; fr < 3; ++fr)
            for (unsigned blue = 0; blue < 16; ++blue)
                for (unsigned dead = 0; dead < 16; ++dead)
                    for (unsigned ci = 0; ci < 10; ++ci)
                        for (unsigned rep = 0; rep < 4; ++rep) {
                            if (((n_b2 + 1) & 255) == 0) randomize(x);
                            W(x, 0x4DA6, fr);
                            for (unsigned g = 0; g < 4; ++g) {
                                W(x, 0x4DA7 + g, (blue >> g & 1) ? 1 + (nxt() & 1) : 0);
                                W(x, 0x4DAC + g, (dead >> g & 1) ? 1 + (nxt() & 1) : 0);
                            }
                            W16(x, 0x4DCB, counters[ci]);
                            run_case(x, 1, 0, n_b2, "frightened timer every combination");
                        }
    }
    std::printf(
        "part B2: the frightened timer alone: every combination of frightened flag x blue flags x dead flags x counter values: %lu calls, differences so far %lu\n", n_b2,
        bad - bad_b1);
    const unsigned long bad_b2 = bad;
    // B3: the level start
    {
        for (unsigned row = 1; row < logic::kLevelCount; ++row)
            for (unsigned rep = 0; rep < 40; ++rep) {
                Mem x;
                randomize(x);
                W(x, 0x4E00, rep & 1 ? 1 : (nxt() & 3));
                W(x, 0x4E13, nxt() % 30);
                run_case(x, 2, row, n_b3, "level start every level");
            }
        // level number 0: the level comes from the byte the pointer 4E0A points at
        for (unsigned row = 0; row < logic::kLevelCount; ++row)
            for (unsigned rep = 0; rep < 40; ++rep) {
                Mem x;
                randomize(x);
                unsigned at;
                do at = nxt() & 0x3FFF;
                while (prog[at] != row);
                W16(x, 0x4E0A, at);
                W(x, 0x4E00, rep & 1 ? 1 : (nxt() & 3));
                W(x, 0x4E13, nxt() % 30);
                run_case(x, 2, 0, n_b3, "level start through the demo pointer");
            }
    }
    std::printf(
        "part B3: the level start alone: every level row 0-20 (also through the demo pointer, level number 0) on random memory: %lu calls, differences so far %lu\n",
        n_b3, bad - bad_b2);
    const unsigned long bad_b3 = bad;
    // B4: random
    for (long i = 0; i < random_cases; ++i) {
        {
            Mem x;
            randomize(x);
            W(x, 0x4DC1, nxt() % 10 == 0 ? nxt() & 255 : nxt() & 7);
            W(x, 0x4DA6, nxt() % 3 ? 0 : nxt() & 3);
            unsigned ph = R(x, 0x4DC1);
            unsigned lim = R(x, 0x4D86 + ((2 * ph) & 255)) | (R(x, 0x4D87 + ((2 * ph) & 255)) << 8);
            if (nxt() & 1) W16(x, 0x4DC2, (lim - 1) & 0xFFFF);
            run_case(x, 0, 0, n_rand[0], "random phase timer");
        }
        {
            Mem x;
            randomize(x);
            W(x, 0x4DA6, nxt() % 4 ? 1 : nxt() & 3);
            for (unsigned g = 0; g < 4; ++g) {
                W(x, 0x4DA7 + g, nxt() % 3 ? 1 : 0);
                W(x, 0x4DAC + g, nxt() % 3 ? 0 : nxt() & 3);
            }
            if (nxt() & 1) W16(x, 0x4DCB, nxt() % 4);
            run_case(x, 1, 0, n_rand[1], "random frightened timer");
        }
        {
            Mem x;
            randomize(x);
            run_case(x, 2, 1 + nxt() % 20, n_rand[2], "random level start");
        }
    }
    std::printf("part B4: %lu random runs of each routine (phase timer %lu, frightened timer %lu, level start %lu): differences %lu\n", (unsigned long)random_cases,
                n_rand[0], n_rand[1], n_rand[2], bad - bad_b3);
    bool ok = bad == 0 && calls[0] > 0 && calls[1] > 0 && calls[2] > 0;
    std::printf("%s: %lu live calls and %lu isolated calls compared, %lu differences\n", ok ? "PASS test_logic_timers" : "FAIL test_logic_timers",
                calls[0] + calls[1] + calls[2], n_b1 + n_b2 + n_b3 + n_rand[0] + n_rand[1] + n_rand[2], bad);
    return ok ? 0 : 1;
}
