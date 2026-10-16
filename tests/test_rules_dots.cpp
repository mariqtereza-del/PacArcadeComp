// T02c: the readable dots, energizers, ghost house counters, power pill start and fruit IN the game (src/rules/rules.h: dots::eat_19CD,
// dots::power_pill_1A70, dots::fruit_0EAD) against the original ROM routines 19CD (with 1B08 and 1A70 inside), 1A70 and 0EAD.
// Usage: test_rules_dots PROG.BIN FRAMES [ISOLATED_CASES_PER_ROUTINE]
// Part 1 (isolated): the ROM routine and the hook each run a whole call from the same random situation until it returns. The CPU registers (A, F, BC, DE,
//   HL, IX, IY) are filled with garbage first, so this is also the garbage-register test: the hook must leave every register exactly as the ROM does.
//   Compared: all memory (work RAM with the stack, video RAM, colour RAM), the cycle count of the call, all CPU registers including R, the I/O writes.
// Part 2 (lockstep): A = original machine, B = the machine with ALL readable rules so far (random numbers, scoring, dots, power pill, fruit) in
//   Machine::run_frame. A scripted player plays one- and two-player games; the level number is written into both machines now and then so that every
//   fruit appears. After EVERY frame: all memory, I/O writes, cycle count of the frame, and the CPU registers.
// Part 3: deliberately wrong versions must be caught.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static unsigned long g_calls[3] = {0, 0, 0};
static void counted_eat(Machine &m) { ++g_calls[0]; rules::dots::eat_19CD(m); }
static void counted_power(Machine &m) { ++g_calls[1]; rules::dots::power_pill_1A70(m); }
static void counted_fruit(Machine &m) { ++g_calls[2]; rules::dots::fruit_0EAD(m); }
static void wrong_eat_pause(Machine &m) { // deliberately wrong: the energizer pause is 5 frames instead of 6
    rules::dots::eat_19CD(m);
    if (m.bus.workram[0x4D9D - 0x4C00] == 6) m.bus.workram[0x4D9D - 0x4C00] = 5;
}
static void wrong_eat_slow(Machine &m) { rules::dots::eat_19CD(m); m.on_tick(1); } // one cycle too slow
static void wrong_fruit_first(Machine &m) { // deliberately wrong: the first fruit at 71 dots
    if (m.bus.workram[0x4E0E - 0x4C00] == 70) { m.bus.workram[0x4E0E - 0x4C00] = 69; rules::dots::fruit_0EAD(m); m.bus.workram[0x4E0E - 0x4C00] = 70; return; }
    rules::dots::fruit_0EAD(m);
}
static void wrong_power(Machine &m) { rules::dots::power_pill_1A70(m); m.bus.workram[0x4DC8 - 0x4C00] = 1; } // siren byte left at 1

// Compares two machines; returns a short name of the first difference or nullptr. regs = also compare the CPU registers.
static const char *differ(Machine &a, Machine &b, bool regs) {
    const Bus &x = a.bus, &y = b.bus;
    if (std::memcmp(x.workram, y.workram, sizeof x.workram)) return "work RAM";
    if (std::memcmp(x.videoram, y.videoram, sizeof x.videoram)) return "video RAM";
    if (std::memcmp(x.colorram, y.colorram, sizeof x.colorram)) return "colour RAM";
    if (std::memcmp(x.spriteram, y.spriteram, sizeof x.spriteram) || std::memcmp(x.sprite_coords, y.sprite_coords, sizeof x.sprite_coords)) return "sprites";
    if (std::memcmp(x.sound_regs, y.sound_regs, sizeof x.sound_regs)) return "sound registers";
    if (std::memcmp(x.latch, y.latch, sizeof x.latch) || x.irq_vector != y.irq_vector || x.watchdog_writes != y.watchdog_writes) return "latches";
    if (x.io_log.size() != y.io_log.size()) return "I/O writes";
    for (size_t i = 0; i < x.io_log.size(); ++i)
        if (x.io_log[i].frame != y.io_log[i].frame || x.io_log[i].address != y.io_log[i].address || x.io_log[i].value != y.io_log[i].value) return "I/O writes";
    if (regs && (a.on_get_af() != b.on_get_af() || a.on_get_bc() != b.on_get_bc() || a.on_get_de() != b.on_get_de() || a.on_get_hl() != b.on_get_hl() ||
                 a.on_get_ix() != b.on_get_ix() || a.on_get_iy() != b.on_get_iy() || a.on_get_sp() != b.on_get_sp() || a.on_get_pc() != b.on_get_pc() ||
                 a.on_get_ir() != b.on_get_ir() || a.on_get_iff1() != b.on_get_iff1()))
        return "CPU registers";
    return nullptr;
}



static uint32_t g_rng = 4242;
static unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

enum Routine { kEat = 0, kPower = 1, kFruit = 2 };
static const uint16_t kEntry[3] = {0x19CD, 0x1A70, 0x0EAD};

// Runs `cases` random whole calls of one routine on X (ROM only) and Y (with the hooks in `table`); returns the number of differences.
static unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, Routine which, long cases, unsigned long *counts) {
    Y.rules = table;
    X.rules = nullptr;
    unsigned long bad = 0;
    const uint16_t kRet = 0x3F00, sp = 0x4FC0;
    for (long c = 0; c < cases; ++c) {
        uint8_t ram[0x3F0];
        for (auto &b : ram) b = (uint8_t)rnd();
        auto W = [&](unsigned a) -> uint8_t & { return ram[a - 0x4C00]; };
        uint8_t video[0x400];
        for (auto &b : video) b = (uint8_t)rnd();
        for (unsigned k = 0; k < 0x400; ++k) {
            const unsigned r = rnd() % 8;
            if (r == 0) video[k] = 0x10; else if (r == 1) video[k] = 0x14; else if (r == 2) video[k] = 0x40;
        }
        if (which == kEat) {
            unsigned u, v;
            do { u = rnd() & 255; v = rnd() % 32; } while (0x4040 + u + 32 * v >= 0x4400);
            W(0x4D39) = (uint8_t)(u + 32);
            W(0x4D3A) = (uint8_t)(v + 32);
            const unsigned addr = 0x4040 + u + 32 * v;
            const unsigned r = rnd() % 5;
            video[addr - 0x4000] = r == 0 ? 0x10 : r == 1 ? 0x14 : r == 2 ? 0x40 : (uint8_t)rnd();
            W(0x4E12) = (rnd() % 3) == 0 ? (uint8_t)(rnd() & 3) : 0;
            for (unsigned a = 0x4DA1; a <= 0x4DA3; ++a) W(a) = (rnd() % 2) ? 1 : 0;
            W(0x4E0E) = (uint8_t)rnd();
            W(0x4C80) = (rnd() % 3 == 0) ? (uint8_t)(0xFC + rnd() % 4) : (uint8_t)(0xC0 + rnd() % 0x40);
            W(0x4C81) = 0x4C;
        } else if (which == kFruit) {
            W(0x4DA5) = (rnd() % 4) ? 0 : (uint8_t)(1 + rnd() % 3);
            W(0x4DD4) = (rnd() % 4) ? 0 : (uint8_t)(1 + rnd() % 7);
            const unsigned r = rnd() % 4;
            W(0x4E0E) = r == 0 ? 70 : r == 1 ? 170 : (uint8_t)rnd();
            W(0x4E0C) = (uint8_t)(rnd() % 2);
            W(0x4E0D) = (uint8_t)(rnd() % 2);
            W(0x4E13) = (rnd() % 2) ? (uint8_t)(rnd() % 30) : (uint8_t)rnd();
            const unsigned taken = rnd() % 17; // slots 0..taken-1 occupied
            for (unsigned i = 0; i < 16; ++i) {
                const unsigned a = 0x4C90 + 3 * i;
                W(a) = i < taken ? (uint8_t)(1 + rnd() % 255) : 0;
            }
        }
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            std::memcpy(m->bus.videoram, video, sizeof video);
            for (unsigned k = 0; k < 0x400; ++k) m->bus.colorram[k] = (uint8_t)(k * 3 + c);
            m->bus.write((uint16_t)(sp - 2), kRet & 255);
            m->bus.write((uint16_t)(sp - 1), kRet >> 8);
        }
        const uint16_t af = (uint16_t)rnd(), bc = (uint16_t)rnd(), de = (uint16_t)rnd(), hl = (uint16_t)rnd(), ix = (uint16_t)rnd(), iy = (uint16_t)rnd();
        for (Machine *m : {&X, &Y}) {
            m->on_set_sp((uint16_t)(sp - 2));
            m->on_set_pc(kEntry[which]);
            m->on_set_af(af); m->on_set_bc(bc); m->on_set_de(de); m->on_set_hl(hl); m->on_set_ix(ix); m->on_set_iy(iy);
            m->on_set_is_int_disabled(false);
            m->bus.io_log.clear();
        }
        const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (50688 - 1 - (rnd() % 1500));
        X.set_ticks_in_frame(start);
        Y.set_ticks_in_frame(start);
        unsigned guard = 0;
        while (X.on_get_pc() != kRet && guard++ < 100000) X.on_step();
        guard = 0;
        while (Y.on_get_pc() != kRet && guard++ < 100000) {
            if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
            else Y.on_step();
        }
        const char *w = differ(X, Y, true);
        if (!w && X.ticks_in_frame() != Y.ticks_in_frame()) w = "cycle count";
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, hook %u)\n", kEntry[which], c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
        if (counts) {
            const uint8_t before_dots = ram[0x4E0E - 0x4C00];
            if (which == kEat && X.bus.workram[0x4E0E - 0x4C00] != before_dots) ++counts[X.bus.workram[0x4D9D - 0x4C00] == 6 ? 1 : 0];
            if (which == kFruit && X.bus.workram[0x4DD4 - 0x4C00] != ram[0x4DD4 - 0x4C00]) ++counts[2];
        }
    }
    return bad;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_dots PROG.BIN FRAMES [ISOLATED_CASES]\n");
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
    const long cases = argc > 3 ? std::atol(argv[3]) : 300000;

    // ---------------- Part 1
    static Machine X(prog), Y(prog);
    unsigned long counts[3] = {0, 0, 0}, bad[3];
    for (int r = 0; r < 3; ++r) bad[r] = isolated(X, Y, rules::table(), (Routine)r, cases, counts);
    std::printf("part 1: %ld isolated whole calls of each routine with garbage in all registers: 19CD (dots eaten %lu, energizers eaten %lu) differences %lu; 1A70 differences %lu; "
                "0EAD (fruit shown %lu) differences %lu\n", cases, counts[0], counts[1], bad[0], bad[1], counts[2], bad[2]);

    // ---------------- Part 2
    static Machine::RuleHook counted[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x19CD] = counted_eat;
    counted[0x1A70] = counted_power;
    counted[0x0EAD] = counted_fruit;
    static Machine A(prog), B(prog);
    B.rules = counted;
    rules::fallbacks = 0;
    uint32_t rng = 31337;
    int dir = 0x04, hold = 0;
    unsigned long entries[3] = {0, 0, 0};
    long bad_b = -1, reg_b = -1;
    const char *why_b = nullptr;
    for (long fr = 0; fr < frames; ++fr) {
        long p = fr % 15000, game = fr / 15000;
        const bool two = game % 3 == 2;
        if (p >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
        }
        for (Machine *m : {&A, &B}) {
            uint8_t in0 = 0xFF, in1 = 0xFF;
            if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
            if (two && p >= 410 && p < 415) in0 &= (uint8_t)~0x20;
            if (p >= 420 && p < 424) in1 &= (uint8_t) ~(two ? 0x40 : 0x20);
            if (p >= 430) in0 &= (uint8_t)~dir;
            m->bus.in0 = in0;
            m->bus.in1 = in1;
            m->bus.dsw1 = (uint8_t)(0xC9 | ((game % 2) ? 0x04 : 0));
            if (p == 1000) m->bus.write(0x4E13, (uint8_t)(game % 24)); // a different fruit in every game (the level number is written into both machines)
            if (p == 1100 && game % 4 == 1) m->bus.write(0x4E0E, 60);  // (near the 70-dot fruit)
            if (p == 2500 && game % 4 == 3) m->bus.write(0x4E0E, 160); // (near the 170-dot fruit)
        }
        const uint32_t a0 = A.ticks_in_frame();
        probe::run_frame(A, [&](uint16_t pc) {
            for (int r = 0; r < 3; ++r) if (pc == kEntry[r]) ++entries[r];
        });
        const uint32_t a_cycles = A.ticks_in_frame() - a0;
        B.run_frame();
        if (bad_b < 0) {
            const char *w = differ(A, B, false);
            if (!w && a_cycles != B.ticks_in_frame()) w = "cycle count of the frame";
            if (w) {
                bad_b = fr, why_b = w;
                if (!std::strcmp(w, "work RAM"))
                    for (unsigned k = 0; k < sizeof A.bus.workram; ++k)
                        if (A.bus.workram[k] != B.bus.workram[k]) { std::printf("  work RAM %04X: original %02X, readable %02X\n", 0x4C00 + k, A.bus.workram[k], B.bus.workram[k]); break; }
            } else if (reg_b < 0 && differ(A, B, true)) reg_b = fr;
        }
        if (bad_b >= 0) break;
    }
    std::printf("part 2: %ld frames; the original machine entered 19CD %lu, 1A70 %lu, 0EAD %lu times; the hooks ran %lu / %lu / %lu times (1A70 is also reached inside 19CD by an energizer, where the 19CD hook does it; the hooks gave way to the ROM %lu times)\n", frames,
                entries[0], entries[1], entries[2], g_calls[0], g_calls[1], g_calls[2], rules::fallbacks);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, why_b);
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "DIFFER");

    // ---------------- Part 3: wrong versions must be caught (isolated runs)
    struct Wrong { const char *name; Routine r; unsigned pc; Machine::RuleHook hook; };
    const Wrong wrongs[] = {{"energizer pause 5 instead of 6", kEat, 0x19CD, wrong_eat_pause}, {"eating one cycle too slow", kEat, 0x19CD, wrong_eat_slow},
                            {"first fruit at 71 dots", kFruit, 0x0EAD, wrong_fruit_first}, {"siren byte left at 1 after the power pill", kPower, 0x1A70, wrong_power}};
    unsigned caught = 0;
    static Machine::RuleHook table_w[0x10000];
    for (const Wrong &w : wrongs) {
        for (int i = 0; i < 0x10000; ++i) table_w[i] = rules::table()[i];
        table_w[w.pc] = w.hook;
        const unsigned long n = isolated(X, Y, table_w, w.r, 20000, nullptr);
        std::printf("part 3: wrong version \"%s\": %s (%lu of 20000 cases differ)\n", w.name, n ? "caught" : "NOT caught", n);
        caught += n > 0;
    }
    unsigned long total_bad = bad[0] + bad[1] + bad[2];
    const bool ok = total_bad == 0 && bad_b < 0 && reg_b < 0 && caught == 4 && entries[0] == g_calls[0] && entries[2] == g_calls[2] && entries[0] > 0 && entries[2] > 0;
    std::printf("%s test_rules_dots: %ld isolated calls per routine, %ld frames, 0 differences required\n", ok ? "PASS" : "FAIL", cases, frames);
    return ok ? 0 : 1;
}
