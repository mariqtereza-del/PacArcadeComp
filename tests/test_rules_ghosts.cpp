// T02d: the readable ghost targeting and direction chooser IN the game (src/rules/rules.h: ghosts::chooser_hook_2966, ghosts::blinky_2730, pinky_276C,
// inky_27A9, clyde_27F1) against the original ROM routines 2966 (with 29EA and 2A12 inside) and the four target tasks.
// Usage: test_rules_ghosts PROG.BIN FRAMES [ISOLATED_CASES_PER_ROUTINE]
// Part 1 (isolated): the ROM routine and the hook each run a whole call from the same random situation until it returns. All CPU registers (A, F, BC, DE,
//   HL, IX, IY) are filled with garbage first, so this is also the garbage-register test. Compared: all memory (work RAM with the stack, video RAM, colour
//   RAM), the cycle count of the call, all CPU registers including R, the I/O writes. Ghosts stand on random maze tiles with random walls around them.
// Part 2 (lockstep): original machine versus the machine with ALL readable rules so far, in Machine::run_frame, for FRAMES frames of scripted play;
//   after EVERY frame: all memory, I/O writes, cycle count, CPU registers.
// Part 3: deliberately wrong versions must be caught.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static unsigned long g_calls[5] = {0, 0, 0, 0, 0};
static void c_chooser(Machine &m) { ++g_calls[4]; rules::ghosts::chooser_hook_2966(m); }
static void c_blinky(Machine &m) { ++g_calls[0]; rules::ghosts::blinky_2730(m); }
static void c_pinky(Machine &m) { ++g_calls[1]; rules::ghosts::pinky_276C(m); }
static void c_inky(Machine &m) { ++g_calls[2]; rules::ghosts::inky_27A9(m); }
static void c_clyde(Machine &m) { ++g_calls[3]; rules::ghosts::clyde_27F1(m); }
static void wrong_slow(Machine &m) { rules::ghosts::chooser_hook_2966(m); m.on_tick(1); }
static void wrong_pinky(Machine &m) { rules::ghosts::pinky_276C(m); m.bus.workram[0x4D2D - 0x4C00] ^= 1; } // a different direction stored
static void wrong_blinky(Machine &m) { rules::ghosts::blinky_2730(m); m.bus.workram[0x4D1E - 0x4C00] ^= 1; } // a different step stored
static void wrong_inky(Machine &m) { rules::ghosts::inky_27A9(m); m.on_tick(1); }
static void wrong_clyde(Machine &m) { rules::ghosts::clyde_27F1(m); m.bus.workram[0x4D24 - 0x4C00] ^= 2; }

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




static uint32_t g_rng = 777777;
static unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

enum Routine { kChooser = 0, kBlinky, kPinky, kInky, kClyde };
static const uint16_t kEntry[5] = {0x2966, 0x2730, 0x276C, 0x27A9, 0x27F1};
static const unsigned kGhostTile[4] = {0x4D0A, 0x4D0C, 0x4D0E, 0x4D10};
static const unsigned kGhostDir[4] = {0x4D2C, 0x4D2D, 0x4D2E, 0x4D2F};

static unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, Routine which, long cases) {
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
        // a tile (a,b) whose four neighbours are inside the video RAM
        auto pick_tile = [&](uint8_t &ta, uint8_t &tb) {
            for (;;) {
                const unsigned u = 1 + rnd() % 250, v = 1 + rnd() % 30;
                const unsigned at = 0x4040 + u + 32 * v;
                if (at - 32 < 0x4000 || at + 32 >= 0x4400) continue;
                ta = (uint8_t)(u + 32);
                tb = (uint8_t)(v + 32 + 32 * (rnd() % 8)); // (the upper bits of b are ignored by the ROM's address calculation)
                return;
            }
        };
        uint8_t ga, gb;
        pick_tile(ga, gb);
        const uint8_t dir = (rnd() % 8 == 0) ? (uint8_t)rnd() % 4 : (uint8_t)(rnd() % 4);
        W(0x4DC7) = (uint8_t)rnd();
        W(0x4DC1) = (uint8_t)(rnd() % 6);
        W(0x4DB6) = (rnd() % 4 == 0) ? (uint8_t)(1 + rnd() % 3) : 0;
        W(0x4E04) = (rnd() % 5 == 0) ? (uint8_t)(rnd() % 6) : 3;
        for (int g = 0; g < 4; ++g) {
            uint8_t a, b;
            pick_tile(a, b);
            W(kGhostTile[g]) = a;
            W(kGhostTile[g] + 1) = b;
            W(kGhostDir[g]) = (uint8_t)(rnd() % 4);
        }
        if (rnd() % 3 == 0) { // Pac-Man near Clyde (to exercise the "closer than 8 tiles" branch)
            W(0x4D39) = (uint8_t)(W(0x4D10) + (int)(rnd() % 9) - 4);
            W(0x4D3A) = (uint8_t)(W(0x4D11) + (int)(rnd() % 9) - 4);
        }
        if (rnd() % 2) { // Pac-Man's step: a proper direction step most of the time
            static const uint8_t sa[4] = {0, 1, 0, 0xFF}, sb[4] = {0xFF, 0, 1, 0};
            const unsigned k = rnd() % 4;
            W(0x4D1C) = sa[k];
            W(0x4D1D) = sb[k];
        }
        uint16_t hl = (uint16_t)rnd(), de = (uint16_t)rnd(), af = (uint16_t)rnd();
        if (which == kChooser) {
            hl = (uint16_t)(ga | (gb << 8));
            af = (uint16_t)((dir << 8) | (af & 0xFF));
        } else {
            const unsigned g = (unsigned)which - 1;
            W(kGhostTile[g]) = ga;
            W(kGhostTile[g] + 1) = gb;
            W(kGhostDir[g]) = dir;
        }
        if (rnd() % 8 == 0) { // every way blocked, except the one behind (and sometimes that too)
            const uint8_t ta = which == kChooser ? ga : W(kGhostTile[(unsigned)which - 1]);
            const uint8_t tb = which == kChooser ? gb : W(kGhostTile[(unsigned)which - 1] + 1);
            for (unsigned d = 0; d < 4; ++d) {
                const uint8_t na = (uint8_t)(ta + rules::z80::kXY * 0 + (d == 1 ? 1 : d == 3 ? -1 : 0)), nb = (uint8_t)(tb + (d == 0 ? -1 : d == 2 ? 1 : 0));
                const unsigned at = 0x4040 + (uint8_t)(na - 32) + 32 * (((uint8_t)(nb - 32)) & 31);
                video[at - 0x4000] = (uint8_t)(0xC0 | (rnd() & 0x3F));
            }
        }
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            std::memcpy(m->bus.videoram, video, sizeof video);
            for (unsigned k = 0; k < 0x400; ++k) m->bus.colorram[k] = (uint8_t)(k * 3 + c);
            m->bus.write((uint16_t)(sp - 2), kRet & 255);
            m->bus.write((uint16_t)(sp - 1), kRet >> 8);
        }
        const uint16_t bc = (uint16_t)rnd(), ix = (uint16_t)rnd(), iy = (uint16_t)rnd();
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
        while (Y.on_get_pc() != kRet && guard++ < 100000) {
            if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
            else Y.on_step();
        }
        const char *w = differ(X, Y, true);
        if (!w && X.ticks_in_frame() != Y.ticks_in_frame()) w = "cycle count";
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, hook %u)\n", kEntry[which], c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
    }
    return bad;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_ghosts PROG.BIN FRAMES [ISOLATED_CASES]\n");
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
    for (int r = 0; r < 5; ++r) bad[r] = isolated(X, Y, rules::table(), (Routine)r, cases);
    std::printf("part 1: %ld isolated whole calls of each routine with garbage in all registers: 2966 differences %lu; Blinky 2730 %lu; Pinky 276C %lu; Inky 27A9 %lu; Clyde 27F1 %lu\n",
                cases, bad[0], bad[1], bad[2], bad[3], bad[4]);

    // ---------------- Part 2
    static Machine::RuleHook counted[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x2966] = c_chooser;
    counted[0x2730] = c_blinky;
    counted[0x276C] = c_pinky;
    counted[0x27A9] = c_inky;
    counted[0x27F1] = c_clyde;
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
            if (p == 1000 && game % 3 == 1) m->bus.write(0x4E13, (uint8_t)(game % 24)); // other levels (other timers, Cruise Elroy, ...)
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
                if (!std::strcmp(w, "I/O writes")) {
                    std::printf("  io sizes %zu / %zu\n", A.bus.io_log.size(), B.bus.io_log.size());
                    for (size_t k = 0; k < A.bus.io_log.size() && k < B.bus.io_log.size(); ++k)
                        if (A.bus.io_log[k].frame != B.bus.io_log[k].frame || A.bus.io_log[k].address != B.bus.io_log[k].address || A.bus.io_log[k].value != B.bus.io_log[k].value) {
                            std::printf("  entry %zu: original frame %u %04X=%02X, readable frame %u %04X=%02X\n", k, A.bus.io_log[k].frame, A.bus.io_log[k].address, A.bus.io_log[k].value, B.bus.io_log[k].frame, B.bus.io_log[k].address, B.bus.io_log[k].value);
                            break;
                        }
                }
                if (!std::strcmp(w, "work RAM"))
                    for (unsigned k = 0; k < sizeof A.bus.workram; ++k)
                        if (A.bus.workram[k] != B.bus.workram[k]) { std::printf("  work RAM %04X: original %02X, readable %02X\n", 0x4C00 + k, A.bus.workram[k], B.bus.workram[k]); break; }
            } else if (reg_b < 0 && differ(A, B, true)) reg_b = fr;
        }
        if (bad_b >= 0) break;
    }
    std::printf("part 2: %ld frames; the original machine entered the tasks Blinky %lu, Pinky %lu, Inky %lu, Clyde %lu times and 2966 %lu times (also from inside the tasks); the hooks ran the tasks %lu / %lu / %lu / %lu times and 2966 on its own %lu times (the hooks gave way to the ROM %lu times)\n",
                frames, entries[1], entries[2], entries[3], entries[4], entries[0], g_calls[0], g_calls[1], g_calls[2], g_calls[3], g_calls[4], rules::fallbacks);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, why_b);
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "DIFFER");

    // ---------------- Part 3
    struct Wrong { const char *name; Routine r; unsigned pc; Machine::RuleHook hook; };
    const Wrong wrongs[] = {{"direction chooser one cycle too slow", kChooser, 0x2966, wrong_slow}, {"Blinky stores a wrong step", kBlinky, 0x2730, wrong_blinky},
                            {"Pinky stores a wrong direction", kPinky, 0x276C, wrong_pinky}, {"Inky one cycle too slow", kInky, 0x27A9, wrong_inky},
                            {"Clyde stores a wrong step", kClyde, 0x27F1, wrong_clyde}};
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
    const bool ok = total_bad == 0 && bad_b < 0 && reg_b < 0 && caught == 5 && entries[1] == g_calls[0] && entries[2] == g_calls[1] && entries[3] == g_calls[2] && entries[4] == g_calls[3] && entries[1] > 0;
    std::printf("%s test_rules_ghosts: %ld isolated calls per routine, %ld frames, 0 differences required\n", ok ? "PASS" : "FAIL", cases, frames);
    return ok ? 0 : 1;
}
