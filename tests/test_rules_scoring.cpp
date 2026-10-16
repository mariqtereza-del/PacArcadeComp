// T02b: the readable scoring IN the game (src/rules/rules.h: add_score_2A5A, high_score_2A89) against the original ROM routine 2A5A.
// Usage: test_rules_scoring PROG.BIN FRAMES [ISOLATED_CASES]
// Part 1 (isolated): the ROM routine and the readable rules each run a whole call from the same random situation (both players, demo and game, all
//   extra-life limits and flags, every points kind, high scores equal / next to / unrelated to the score, random video RAM) until the routine returns;
//   compared: all memory, the cycle count of the whole call, the stack pointer and the return address.
// Part 2 (lockstep): A = original machine, B = all readable rules (random numbers and scoring) in Machine::run_frame, C = like B with garbage in the registers
//   the rules leave alone. The scripted player of T01b plays one- and two-player games with different extra-life limits; to reach the extra life and
//   the high score within a short test, the same score is written into all three machines now and then (it changes nothing in the rules).
//   After EVERY frame: all memory, I/O writes, the cycle count of the frame (and, for B, the CPU registers as information).
// Part 3: deliberately wrong versions of the rules are run and must be caught.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static unsigned long g_calls = 0;
static void counted_score(Machine &m) {
    ++g_calls;
    rules::add_score_2A5A(m);
}
static void wrong_score_energizer(Machine &m) { // deliberately wrong: an energizer is scored as a dot (10 instead of 50)
    if (((m.on_get_bc() >> 8) & 255) == 1) m.on_set_bc((uint16_t)((0 << 8) | (m.on_get_bc() & 255)));
    rules::add_score_2A5A(m);
}
static void wrong_score_limit(Machine &m) { // deliberately wrong: the extra life limit tested with "<=" instead of "<": give the life one step too early
    if (m.bus.workram[0x4E71 - 0x4C00] != 0xFF) m.bus.workram[0x4E71 - 0x4C00] = (uint8_t)(m.bus.workram[0x4E71 - 0x4C00] + 1);
    rules::add_score_2A5A(m);
}
static void wrong_high_slow(Machine &m) { // deliberately wrong: the high score rule is right but takes one cycle too long
    rules::high_score_2A89(m);
    m.on_tick(1);
}

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


static uint32_t g_rng = 99;
static unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
static uint8_t bcd_byte() { return (uint8_t)(((rnd() % 10) << 4) | (rnd() % 10)); }

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_scoring PROG.BIN FRAMES [ISOLATED_CASES]\n");
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
    const long isolated_cases = argc > 3 ? std::atol(argv[3]) : 300000;

    // ---------------- Part 1: isolated whole calls
    static Machine X(prog), Y(prog);
    X.rules = nullptr;
    Y.rules = rules::table();
    unsigned long iso_bad = 0, kinds[8] = {0}; // 0 demo, 1 no extra life, 2 life already given, 3 life now, 4 high score raised, 5 player 2, 6 high equal, 7 lower
    const uint16_t kRet = 0x3F00;
    for (long c = 0; c < isolated_cases; ++c) {
        uint8_t ram[0x3F0];
        for (auto &b : ram) b = (uint8_t)rnd();
        for (unsigned a = 0x4E80; a < 0x4E8B; ++a) ram[a - 0x4C00] = bcd_byte();
        ram[0x4E83 - 0x4C00] = (uint8_t)(rnd() & 1);
        ram[0x4E87 - 0x4C00] = (uint8_t)(rnd() & 1);
        ram[0x4E09 - 0x4C00] = (uint8_t)(rnd() & 1);
        ram[0x4E00 - 0x4C00] = (rnd() % 4 == 0) ? 1 : 3;
        static const uint8_t limits[] = {0x10, 0x15, 0x20, 0xFF, 0x01, 0x02, 0x05, 0x09};
        ram[0x4E71 - 0x4C00] = (rnd() % 3) ? limits[rnd() % 8] : (uint8_t)bcd_byte();
        const unsigned pl = ram[0x4E09 - 0x4C00] ? 0x4E84 : 0x4E80;
        const unsigned mode = rnd() % 6;
        if (mode == 0) std::memcpy(ram + 0x4E88 - 0x4C00, ram + pl - 0x4C00, 3);              // high score = score
        else if (mode == 1) { std::memcpy(ram + 0x4E88 - 0x4C00, ram + pl - 0x4C00, 3); ram[0x4E88 - 0x4C00 + (rnd() % 3)] ^= 0x01 << (rnd() % 2 ? 0 : 4); if ((ram[0x4E88-0x4C00]&0x0F)>9||(ram[0x4E88-0x4C00]>>4)>9||(ram[0x4E89-0x4C00]&0x0F)>9||(ram[0x4E89-0x4C00]>>4)>9||(ram[0x4E8A-0x4C00]&0x0F)>9||(ram[0x4E8A-0x4C00]>>4)>9) std::memcpy(ram + 0x4E88 - 0x4C00, ram + pl - 0x4C00, 3); }
        else if (mode == 2) { // close below the score so that the points make it higher
            std::memcpy(ram + 0x4E88 - 0x4C00, ram + pl - 0x4C00, 3);
        }
        ram[0x4E14 - 0x4C00] = (uint8_t)(rnd() % 6);
        ram[0x4E15 - 0x4C00] = (uint8_t)(rnd() % 6);
        const unsigned idx = rnd() % 14;
        const uint16_t sp = 0x4FC0;
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            for (unsigned k = 0; k < 0x400; ++k) { m->bus.videoram[k] = (uint8_t)(k * 7 + c); m->bus.colorram[k] = (uint8_t)(k * 3 + c); }
            m->bus.write((uint16_t)(sp - 2), kRet & 255);
            m->bus.write((uint16_t)(sp - 1), kRet >> 8);
            m->on_set_sp((uint16_t)(sp - 2));
            m->on_set_pc(0x2A5A);
            m->on_set_bc((uint16_t)((idx << 8) | 0x34));
            m->on_set_de(0x1234);
            m->on_set_hl(0x5678);
            m->on_set_af(0x5A00);
            m->on_set_is_int_disabled(false);
            m->bus.io_log.clear();
        }
        const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (50688 - 1 - (rnd() % 1200)); // mostly early in the frame, sometimes where the routine would not fit any more
        X.set_ticks_in_frame(start);
        Y.set_ticks_in_frame(start);
        const uint32_t xt = start, yt = start;
        unsigned guard = 0;
        while (X.on_get_pc() != kRet && guard++ < 100000) X.on_step();
        guard = 0;
        while (Y.on_get_pc() != kRet && guard++ < 100000) {
            if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
            else Y.on_step();
        }
        const uint32_t xc = X.ticks_in_frame() - xt, yc = Y.ticks_in_frame() - yt;
        const char *w = differ(X, Y, true);
        if (!w && xc != yc) w = "cycle count";
        if (!w && (X.on_get_sp() != Y.on_get_sp())) w = "stack pointer";
        if (w) {
            if (++iso_bad <= 5) std::printf("  ISOLATED MISMATCH case %ld: %s (ROM %u cycles, rules %u)\n", c, w, xc, yc);
        }
        
        if (ram[0x4E00 - 0x4C00] == 1) ++kinds[0];
        else {
            ++kinds[1];
            if (ram[0x4E09 - 0x4C00]) ++kinds[5];
            if (std::memcmp(X.bus.workram + 0x4E88 - 0x4C00, ram + 0x4E88 - 0x4C00, 3)) ++kinds[4];
            if (X.bus.workram[0x4E14 - 0x4C00] != ram[0x4E14 - 0x4C00]) ++kinds[3];
        }
    }
    std::printf("part 1: %ld isolated whole calls of 2A5A (demo %lu, game %lu, player 2 %lu, life given now %lu, high score raised %lu): differences %lu\n", isolated_cases,
                kinds[0], kinds[1], kinds[5], kinds[3], kinds[4], iso_bad);

    // ---------------- Part 2: lockstep
    static Machine::RuleHook counted[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x2A5A] = counted_score;
    static Machine A(prog), B(prog), C(prog);
    B.rules = counted;
    C.rules = rules::table();
    uint32_t rng = 777;
    int dir = 0x04, hold = 0;
    auto inputs = [&](Machine &x, long fr, bool two, long game) {
        long p = fr % 15000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
        if (two && p >= 410 && p < 415) in0 &= (uint8_t)~0x20;
        if (p >= 420 && p < 424) in1 &= (uint8_t) ~(two ? 0x40 : 0x20);
        if (p >= 430) in0 &= (uint8_t)~dir;
        x.bus.in0 = in0;
        x.bus.in1 = in1;
        static const uint8_t dips[4] = {0xC9, 0xD9, 0xE9, 0xF9}; // extra life at 10000, 15000, 20000, never; lives 3
        x.bus.dsw1 = (uint8_t)(dips[game % 4] | (((game / 4) % 2) ? 0x04 : 0));
    };
    rules::fallbacks = 0;
    unsigned long pokes = 0, extra_a = 0, highs_a = 0, p2_a = 0, entries = 0;
    long bad_b = -1, bad_c = -1, reg_b = -1;
    const char *why_b = nullptr, *why_c = nullptr;
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
        for (Machine *m : {&A, &B, &C}) inputs(*m, fr, two, game);
        if (p == 1200 || p == 3000) { // the same score into all three machines: near the extra life, or just a high score
            ++pokes;
            for (Machine *m : {&A, &B, &C}) {
                const unsigned who = (p == 3000 && two) ? 0x4E84 : 0x4E80;
                const unsigned k = (unsigned)(game % 4);
                const uint8_t mid = (k == 1) ? 0x45 : 0x95, top = (k == 1 || k == 2) ? 0x01 : 0x00; // just below the 10,000 / 15,000 / 20,000 limit
                m->bus.write((uint16_t)who, 0x80);
                m->bus.write((uint16_t)(who + 1), mid);
                m->bus.write((uint16_t)(who + 2), top);
            }
        }
        if (const char *dbg = std::getenv("T02_DEBUG_FRAME")) { // debugging aid: step A and B side by side through this frame and report the first memory difference
            if (fr == std::atol(dbg)) {
                A.set_ticks_in_frame(0); B.set_ticks_in_frame(0);
                A.bus.io_log.clear(); B.bus.io_log.clear();
                unsigned steps = 0;
                for (;;) {
                    const uint16_t pcb = (uint16_t)B.on_get_pc();
                    const bool hooked = B.rules[pcb] != nullptr;
                    if (hooked) B.rules[pcb](B); else B.on_step();
                    while (A.ticks_in_frame() < B.ticks_in_frame() || (A.on_get_pc() != B.on_get_pc() && A.ticks_in_frame() == B.ticks_in_frame() && hooked)) A.on_step();
                    if (std::memcmp(A.bus.workram, B.bus.workram, sizeof A.bus.workram)) {
                        std::printf("  DEBUG: after B's step at pc %04X (%s), ticks A %u B %u, pc A %04X B %04X: work RAM differs at:", pcb, hooked ? "rule" : "ROM", A.ticks_in_frame(), B.ticks_in_frame(), (unsigned)A.on_get_pc(), (unsigned)B.on_get_pc());
                        for (unsigned k = 0; k < sizeof A.bus.workram; ++k) if (A.bus.workram[k] != B.bus.workram[k]) std::printf(" %04X(%02X/%02X)", 0x4C00 + k, A.bus.workram[k], B.bus.workram[k]);
                        std::printf("\n  SP A %04X B %04X\n", (unsigned)A.on_get_sp(), (unsigned)B.on_get_sp());
                        return 3;
                    }
                    ++steps;
                    if (B.ticks_in_frame() >= Machine::cycles_per_frame) break;
                }
            }
        }
        const uint32_t a0 = A.ticks_in_frame();
        const uint8_t lives_a = A.bus.read(0x4E14);
        const uint8_t hs0 = A.bus.read(0x4E88), hs1 = A.bus.read(0x4E89), hs2 = A.bus.read(0x4E8A);
        probe::run_frame(A, [&](uint16_t pc) { if (pc == 0x2A5A) ++entries; });
        const uint32_t a_cycles = A.ticks_in_frame() - a0;
        if (A.bus.read(0x4E14) > lives_a) ++extra_a;
        if (A.bus.read(0x4E88) != hs0 || A.bus.read(0x4E89) != hs1 || A.bus.read(0x4E8A) != hs2) ++highs_a;
        if (two && A.bus.read(0x4E09)) ++p2_a;
        B.run_frame();
        rules::poison = (0x9E3779B9u ^ (uint32_t)fr) | 1;
        C.run_frame();
        rules::poison = 0;
        if (bad_b < 0) {
            const char *w = differ(A, B, false);
            if (!w && a_cycles != B.ticks_in_frame()) w = "cycle count of the frame";
            if (w) {
                bad_b = fr, why_b = w;
                if (!std::strcmp(w, "work RAM"))
                    for (unsigned k = 0; k < sizeof A.bus.workram; ++k)
                        if (A.bus.workram[k] != B.bus.workram[k]) { std::printf("  SP original %04X readable %04X; stack A:", (unsigned)A.on_get_sp(), (unsigned)B.on_get_sp()); for (unsigned q = 0x3A0; q < 0x3F0; ++q) std::printf(" %02X", A.bus.workram[q]); std::printf("\n  stack B:"); for (unsigned q = 0x3A0; q < 0x3F0; ++q) std::printf(" %02X", B.bus.workram[q]); std::printf("\n"); std::printf("  work RAM %04X: original %02X, readable %02X\n", 0x4C00 + k, A.bus.workram[k], B.bus.workram[k]); break; }
            }
            else if (reg_b < 0 && differ(A, B, true)) reg_b = fr;
        }
        if (bad_c < 0) {
            const char *w = differ(A, C, false);
            if (!w && a_cycles != C.ticks_in_frame()) w = "cycle count of the frame";
            if (w) bad_c = fr, why_c = w;
        }
        if (bad_b >= 0 && bad_c >= 0) break;
    }
    std::printf("part 2: %ld frames, original machine entered 2A5A %lu times, readable scoring ran %lu times (hook gave way to the ROM %lu times); extra lives in game %lu frames, high score raised in %lu frames, score written %lu times\n",
                frames, entries, g_calls, rules::fallbacks, extra_a, highs_a, pokes);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, why_b);
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "differ (registers the rules leave alone; see the notes)");
    std::printf("part 2b: garbage in A, F, BC, DE, HL after every call: %s\n", bad_c < 0 ? "identical on every frame" : "DIFFERENT");
    if (bad_c >= 0) std::printf("  first difference at frame %ld: %s\n", bad_c, why_c);

    // ---------------- Part 3: wrong versions must be caught
    struct Wrong { const char *name; unsigned pc; Machine::RuleHook hook; bool needs_score_poke; };
    const Wrong wrongs[] = {{"energizer scored as a dot", 0x2A5A, wrong_score_energizer, false}, {"extra life one step early", 0x2A5A, wrong_score_limit, true}, {"high score routine one cycle too slow", 0x2A89, wrong_high_slow, false}};
    unsigned caught = 0;
    for (const Wrong &w : wrongs) {
        static Machine::RuleHook table_w[0x10000];
        for (int i = 0; i < 0x10000; ++i) table_w[i] = rules::table()[i];
        table_w[w.pc] = w.hook;
        Machine *a = new Machine(prog), *b = new Machine(prog);
        b->rules = table_w;
        bool found = false;
        for (long fr = 0; fr < 40000 && !found; ++fr) {
            long p = fr % 15000, game = fr / 15000;
            for (Machine *m : {a, b}) inputs(*m, fr, false, game == 0 ? 0 : game);
            if (p >= 430 && (fr % 17) == 0) { int d[4] = {1, 2, 4, 8}; dir = d[(fr / 17) % 4]; }
            if (p == 1200) for (Machine *m : {a, b}) { m->bus.write(0x4E80, 0x80); m->bus.write(0x4E81, 0x95); m->bus.write(0x4E82, 0x00); }
            a->run_frame();
            b->run_frame();
            if (differ(*a, *b, false) || a->ticks_in_frame() != b->ticks_in_frame()) found = true;
        }
        std::printf("part 3: wrong version \"%s\": %s\n", w.name, found ? "caught" : "NOT caught");
        caught += found;
        delete a;
        delete b;
    }
    const bool ok = iso_bad == 0 && bad_b < 0 && bad_c < 0 && caught == 3 && entries == g_calls + 0 && entries > 0;
    std::printf("%s test_rules_scoring: %ld isolated calls, %ld frames, %lu calls of the readable scoring, 0 differences required\n", ok ? "PASS" : "FAIL", isolated_cases, frames, g_calls);
    return ok ? 0 : 1;
}
