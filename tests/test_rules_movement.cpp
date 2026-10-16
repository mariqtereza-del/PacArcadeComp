// T02e: the readable movement of Pac-Man and the four ghosts IN the game (src/rules/rules.h: movement::pacman_1806, movement::ghost_move<0..3>) against
// the original ROM routines 1806, 1B36, 1C4B, 1D22, 1DF9, which run every update of the game.
// Usage: test_rules_movement PROG.BIN FRAMES [ISOLATED_CASES_PER_ROUTINE]
// Part 1 (isolated): the ROM routine and the hook each run a whole call from the same random situation until it returns. All CPU registers are filled
//   with garbage first (garbage-register test). Compared: all memory (work RAM with the stack, video RAM, colour RAM), the cycle count of the call, all
//   CPU registers including R, the I/O writes. Also counted: how often the hook gave way to the ROM (it must stay rare).
// Part 2 (lockstep): original machine versus the machine with ALL readable rules so far, for FRAMES frames of scripted play; after EVERY frame: all
//   memory, I/O writes, the cycle count of the frame, the CPU registers.
// Part 3: deliberately wrong versions must be caught.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static unsigned long g_calls[5] = {0, 0, 0, 0, 0};
static void c_pac(Machine &m) { ++g_calls[0]; rules::movement::pacman_1806(m); }
static void c_g0(Machine &m) { ++g_calls[1]; rules::movement::ghost_move<0>(m); }
static void c_g1(Machine &m) { ++g_calls[2]; rules::movement::ghost_move<1>(m); }
static void c_g2(Machine &m) { ++g_calls[3]; rules::movement::ghost_move<2>(m); }
static void c_g3(Machine &m) { ++g_calls[4]; rules::movement::ghost_move<3>(m); }
static void wrong_pac(Machine &m) { rules::movement::pacman_1806(m); m.on_tick(1); }
static void wrong_g0(Machine &m) { rules::movement::ghost_move<0>(m); m.bus.workram[0x4D00 - 0x4C00] ^= 1; }
static void wrong_g1(Machine &m) { rules::movement::ghost_move<1>(m); m.on_tick(1); }
static void wrong_g2(Machine &m) { rules::movement::ghost_move<2>(m); m.bus.workram[0x4D31 + 4 - 0x4C00] ^= 1; }
static void wrong_g3(Machine &m) { rules::movement::ghost_move<3>(m); m.on_set_a((uint8_t)(m.on_get_a() ^ 1)); }

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





static uint32_t g_rng = 424242;
static unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

enum Routine { kPac = 0, kG0, kG1, kG2, kG3 };
static const uint16_t kEntry[5] = {0x1806, 0x1B36, 0x1C4B, 0x1D22, 0x1DF9};

static unsigned long g_isolated_fallbacks = 0;
static unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, Routine which, long cases) {
    Y.rules = table;
    X.rules = nullptr;
    unsigned long bad = 0;
    const uint16_t kRet = 0x3F00, sp = 0x4FC0;
    static const int sa[4] = {-1, 0, 1, 0}, sb[4] = {0, 1, 0, -1}; // (these are the ROM's steps; not stored as bytes of the ROM)
    for (long c = 0; c < cases; ++c) {
        uint8_t ram[0x3F0];
        for (auto &b : ram) b = (uint8_t)rnd();
        auto W = [&](unsigned a) -> uint8_t & { return ram[a - 0x4C00]; };
        auto word = [&](unsigned a, unsigned v) { W(a) = (uint8_t)(v & 255); W(a + 1) = (uint8_t)(v >> 8); };
        auto step = [&](unsigned a, unsigned d) { W(a) = (uint8_t)sa[d]; W(a + 1) = (uint8_t)sb[d]; };
        uint8_t video[0x400];
        for (auto &b : video) b = (rnd() % 4 == 0) ? (uint8_t)(0xC0 | (rnd() & 0x3F)) : (uint8_t)(rnd() & 0x3F);
        // ---- Pac-Man
        W(0x4D9D) = (rnd() % 5) ? 0xFF : (uint8_t)(rnd() % 4);
        W(0x4DA6) = (uint8_t)(rnd() % 2);
        for (unsigned a = 0x4D46; a < 0x4D4E; a += 2) word(a, (rnd() % 3 == 0) ? 0x8000 | (rnd() & 0xFFFF) : (rnd() & 0xFFFF)); // (high words often have the top bit set)
        W(0x4E72) = (uint8_t)(rnd() % 2);
        W(0x4E09) = (uint8_t)(rnd() % 2);
        W(0x4E00) = (rnd() % 8 == 0) ? 1 : (uint8_t)(3 + rnd() % 2);
        W(0x4E04) = (rnd() % 8 == 0) ? (uint8_t)(0x10 + rnd() % 4) : 3;
        W(0x4E02) = (rnd() % 2) ? 0x22 : (uint8_t)rnd();
        // pixel positions: a tile inside the maze, anywhere in the tile (the high bit of some positions is set, as in the tunnel)
        W(0x4D08) = (uint8_t)(rnd()); W(0x4D09) = (uint8_t)(rnd());
        if (rnd() % 3) { W(0x4D08) = (uint8_t)((rnd() % 28 + 1) * 8 + 4); } // at a tile middle (x)
        if (rnd() % 3) { W(0x4D09) = (uint8_t)((rnd() % 28 + 2) * 8 + 4); }
        W(0x4D39) = (uint8_t)(0x20 + rnd() % 32); W(0x4D3A) = (rnd() % 6) ? (uint8_t)(0x21 + rnd() % 26) : (uint8_t)(0x1E + rnd() % 0x20);
        { unsigned d = rnd() % 4; step(0x4D1C, d); W(0x4D30) = (uint8_t)d; }
        { unsigned d = rnd() % 4; step(0x4D26, d); W(0x4D3C) = (uint8_t)d; }
        W(0x4DBF) = (uint8_t)(rnd() % 2);
        W(0x4D12) = (uint8_t)rnd(); W(0x4D13) = (rnd() % 3) ? (uint8_t)(0x21 + rnd() % 26) : (uint8_t)(rnd() % 4 == 0 ? 0x1D : rnd() % 2 ? 0x3E : 0x20 + rnd() % 30);
        if (rnd() % 20 == 0) { word(0x4DD2, 0x8094); W(0x4DD4) = 1; word(0x4D08, 0x8094); }
        // the task queue: the pointer stays in its page
        W(0x4C80) = (uint8_t)(0xC0 + rnd() % 64); W(0x4C81) = 0x4C;
        W(0x4E0E) = (uint8_t)rnd();
        // ---- ghosts
        static const unsigned kPix[4] = {0x4D00, 0x4D02, 0x4D04, 0x4D06}, kMoveTile[4] = {0x4D31, 0x4D33, 0x4D35, 0x4D37}, kStep[4] = {0x4D14, 0x4D16, 0x4D18, 0x4D1A},
                              kVec[4] = {0x4D1E, 0x4D20, 0x4D22, 0x4D24}, kCur[4] = {0x4D28, 0x4D29, 0x4D2A, 0x4D2B}, kWant[4] = {0x4D2C, 0x4D2D, 0x4D2E, 0x4D2F},
                              kTile[4] = {0x4D0A, 0x4D0C, 0x4D0E, 0x4D10}, kAct[4] = {0x4DA0, 0x4DA1, 0x4DA2, 0x4DA3}, kBlk[4] = {0x4DAC, 0x4DAD, 0x4DAE, 0x4DAF},
                              kEdible[4] = {0x4DA7, 0x4DA8, 0x4DA9, 0x4DAA}, kRev[4] = {0x4DB1, 0x4DB2, 0x4DB3, 0x4DB4};
        for (int g = 0; g < 4; ++g) {
            W(kPix[g]) = (uint8_t)rnd(); W(kPix[g] + 1) = (uint8_t)rnd();
            if (rnd() % 3) W(kPix[g]) = (uint8_t)((rnd() % 28 + 1) * 8 + 4);
            if (rnd() % 3) W(kPix[g] + 1) = (uint8_t)((rnd() % 28 + 2) * 8 + 4);
            W(kMoveTile[g]) = (uint8_t)(0x20 + rnd() % 32); W(kMoveTile[g] + 1) = (uint8_t)(0x21 + rnd() % 26);
            step(kStep[g], rnd() % 4); step(kVec[g], rnd() % 4);
            W(kCur[g]) = (uint8_t)(rnd() % 4); W(kWant[g]) = (uint8_t)(rnd() % 4);
            W(kTile[g]) = (uint8_t)(0x20 + rnd() % 32);
            W(kTile[g] + 1) = (rnd() % 8) ? (uint8_t)(0x21 + rnd() % 26) : (uint8_t)(rnd() % 4 == 0 ? 0x1D : rnd() % 3 == 0 ? 0x3E : 0x1E + rnd() % 0x22);
            W(kAct[g]) = (rnd() % 6) ? 1 : (uint8_t)(rnd() % 3);
            W(kBlk[g]) = (rnd() % 8 == 0) ? 1 : 0;
            W(kEdible[g]) = (uint8_t)(rnd() % 2);
            W(kRev[g]) = (rnd() % 4 == 0) ? 1 : 0;
            W(0x4D99 + g) = (uint8_t)(rnd() % 2);
        }
        W(0x4DB6) = (uint8_t)(rnd() % 2); W(0x4DB7) = (uint8_t)(rnd() % 2);
        W(0x4DBB) = (uint8_t)(rnd() % 0x40); W(0x4DBC) = (uint8_t)(rnd() % 0x20);
        if (rnd() % 3 == 0) W(0x4E0E) = (uint8_t)(0xF4 - rnd() % 0x40);
        uint8_t colour[0x400];
        for (unsigned k = 0; k < 0x400; ++k) colour[k] = (rnd() % 8 == 0) ? (uint8_t)0x1B : (rnd() % 8 == 0) ? (uint8_t)0x1A : (uint8_t)(k * 3 + c);
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            std::memcpy(m->bus.videoram, video, sizeof video);
            std::memcpy(m->bus.colorram, colour, sizeof colour);
        }
        const uint8_t in0 = (uint8_t)(0xF0 | (rnd() % 3 == 0 ? 0x0F : (0x0F & ~(1u << (rnd() % 4))))), in1 = (uint8_t)(0xF0 | (rnd() % 3 == 0 ? 0x0F : (0x0F & ~(1u << (rnd() % 4)))));
        for (Machine *m : {&X, &Y}) {
            m->bus.write((uint16_t)(sp - 2), kRet & 255);
            m->bus.write((uint16_t)(sp - 1), kRet >> 8);
            m->bus.in0 = in0;
            m->bus.in1 = in1;
        }
        const uint16_t hl = (uint16_t)rnd(), de = (uint16_t)rnd(), af = (uint16_t)rnd(), bc = (uint16_t)rnd(), ix = (uint16_t)rnd(), iy = (uint16_t)rnd();
        for (Machine *m : {&X, &Y}) {
            m->on_set_sp((uint16_t)(sp - 2));
            m->on_set_pc(kEntry[which]);
            m->on_set_af(af); m->on_set_bc(bc); m->on_set_de(de); m->on_set_hl(hl); m->on_set_ix(ix); m->on_set_iy(iy);
            m->on_set_is_int_disabled(false); m->on_set_r((uint8_t)(c * 7));
            m->bus.io_log.clear();
        }
        const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (50688 - 1 - (rnd() % 4000));
        X.set_ticks_in_frame(start);
        Y.set_ticks_in_frame(start);
        unsigned guard = 0;
        while (X.on_get_pc() != kRet && guard++ < 100000) X.on_step();
        guard = 0;
        const unsigned long fb0 = rules::fallbacks;
        while (Y.on_get_pc() != kRet && guard++ < 100000) {
            if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
            else Y.on_step();
        }
        g_isolated_fallbacks += rules::fallbacks - fb0;
        const char *w = differ(X, Y, true);
        if (!w && X.ticks_in_frame() != Y.ticks_in_frame()) w = "cycle count";
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, hook %u)\n", kEntry[which], c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
    }
    return bad;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_movement PROG.BIN FRAMES [ISOLATED_CASES]\n");
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
    const long cases = argc > 3 ? std::atol(argv[3]) : 100000;

    // ---------------- Part 1
    static Machine X(prog), Y(prog);
    unsigned long bad[5];
    unsigned long fb[5];
    for (int r = 0; r < 5; ++r) {
        g_isolated_fallbacks = 0;
        bad[r] = isolated(X, Y, rules::table(), (Routine)r, cases);
        fb[r] = g_isolated_fallbacks;
    }
    std::printf("part 1: %ld isolated whole calls of each routine with garbage in all registers: Pac-Man 1806 differences %lu; Blinky 1B36 %lu; Pinky 1C4B %lu; Inky 1D22 %lu; Clyde 1DF9 %lu\n",
                cases, bad[0], bad[1], bad[2], bad[3], bad[4]);
    std::printf("  (the hook gave way to the ROM in %lu / %lu / %lu / %lu / %lu of those calls)\n", fb[0], fb[1], fb[2], fb[3], fb[4]);

    // ---------------- Part 2
    static Machine::RuleHook counted[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x1806] = c_pac;
    counted[0x1B36] = c_g0;
    counted[0x1C4B] = c_g1;
    counted[0x1D22] = c_g2;
    counted[0x1DF9] = c_g3;
    static Machine A(prog), B(prog);
    B.rules = counted;
    rules::fallbacks = 0;
    uint32_t rng = 2024;
    int dir = 0x04, hold = 0;
    unsigned long entries[5] = {0, 0, 0, 0, 0};
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
            if (p == 1000 && game % 3 == 1) m->bus.write(0x4E13, (uint8_t)(game % 24)); // other levels
            if (p == 1200 && game % 4 == 2) m->bus.write(0x4E0E, 200); // near the end of the level: Cruise Elroy
        }
        const uint32_t a0 = A.ticks_in_frame();
        probe::run_frame(A, [&](uint16_t pc) {
            for (int r = 0; r < 5; ++r) if (pc == kEntry[r]) ++entries[r];
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
    std::printf("part 2: %ld frames; the original machine entered Pac-Man %lu, Blinky %lu, Pinky %lu, Inky %lu, Clyde %lu times; the hooks ran %lu / %lu / %lu / %lu / %lu times (the hooks gave way to the ROM %lu times)\n",
                frames, entries[0], entries[1], entries[2], entries[3], entries[4], g_calls[0], g_calls[1], g_calls[2], g_calls[3], g_calls[4], rules::fallbacks);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, why_b);
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "DIFFER");

    // ---------------- Part 3
    struct Wrong { const char *name; Routine r; unsigned pc; Machine::RuleHook hook; };
    const Wrong wrongs[] = {{"Pac-Man one cycle too slow", kPac, 0x1806, wrong_pac}, {"Blinky position byte changed", kG0, 0x1B36, wrong_g0},
                            {"Pinky one cycle too slow", kG1, 0x1C4B, wrong_g1}, {"Inky tile byte changed", kG2, 0x1D22, wrong_g2},
                            {"Clyde leaves a wrong A", kG3, 0x1DF9, wrong_g3}};
    unsigned caught = 0;
    static Machine::RuleHook table_w[0x10000];
    for (const Wrong &w : wrongs) {
        for (int i = 0; i < 0x10000; ++i) table_w[i] = rules::table()[i];
        table_w[w.pc] = w.hook;
        const unsigned long n = isolated(X, Y, table_w, w.r, 5000);
        std::printf("part 3: wrong version \"%s\": %s (%lu of 5000 cases differ)\n", w.name, n ? "caught" : "NOT caught", n);
        caught += n > 0;
    }
    unsigned long total_bad = bad[0] + bad[1] + bad[2] + bad[3] + bad[4];
    const bool ok = total_bad == 0 && bad_b < 0 && reg_b < 0 && caught == 5 && entries[0] == g_calls[0] && entries[1] == g_calls[1] && entries[2] == g_calls[2] && entries[3] == g_calls[3] && entries[4] == g_calls[4] && entries[0] > 0;
    std::printf("%s test_rules_movement: %ld isolated calls per routine, %ld frames, 0 differences required\n", ok ? "PASS" : "FAIL", cases, frames);
    return ok ? 0 : 1;
}
