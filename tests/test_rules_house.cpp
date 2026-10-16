// T02h: the readable ghost house release IN the game (src/rules/rules.h: house::eaten_1066, release<0..2>, inactivity_13DD)
// against the original ROM routines 1066, 2069, 208C, 20AF and 13DD.
// Usage: test_rules_house PROG.BIN FRAMES [ISOLATED_CASES_PER_ROUTINE]
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
static void c0(Machine &m) { ++g_calls[0]; rules::house::eaten_1066(m); }
static void c1(Machine &m) { ++g_calls[1]; rules::house::release<0>(m); }
static void c2(Machine &m) { ++g_calls[2]; rules::house::release<1>(m); }
static void c3(Machine &m) { ++g_calls[3]; rules::house::release<2>(m); }
static void c4(Machine &m) { ++g_calls[4]; rules::house::inactivity_13DD(m); }
static void w0(Machine &m) { rules::house::eaten_1066(m); m.on_tick(1); }
static void w1(Machine &m) { rules::house::release<0>(m); m.bus.workram[0x4DA1 - 0x4C00] ^= 1; }
static void w2(Machine &m) { rules::house::release<1>(m); m.bus.workram[0x4D9F - 0x4C00] ^= 1; }
static void w3(Machine &m) { rules::house::release<2>(m); m.on_tick(1); }
static void w4(Machine &m) { rules::house::inactivity_13DD(m); m.bus.workram[0x4D97 - 0x4C00] ^= 1; }
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

enum Routine { kEaten = 0, kPinky, kInky, kClyde, kInactive };
static const uint16_t kEntry[5] = {0x1066, 0x2069, 0x208C, 0x20AF, 0x13DD};

static unsigned long g_isolated_fallbacks = 0;
static unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, Routine which, long cases) {
    Y.rules = table;
    X.rules = nullptr;
    unsigned long bad = 0;
    const uint16_t kRet = 0x3F00, sp = 0x4FC0;
    for (long c = 0; c < cases; ++c) {
        uint8_t ram[0x3F0];
        for (auto &b : ram) b = (uint8_t)rnd();
        auto W = [&](unsigned a) -> uint8_t & { return ram[a - 0x4C00]; };
        auto word = [&](unsigned a, unsigned v) { W(a) = (uint8_t)(v & 255); W(a + 1) = (uint8_t)(v >> 8); };
        // ---- the ghost house memory, structured so every branch is reached often
        if (rnd() % 2) W(0x4DAB) = (uint8_t)(rnd() % 7);
        for (int g = 0; g < 3; ++g) {
            if (rnd() % 3) W(0x4DA1 + g) = 0;
            if (rnd() % 2) W(0x4DB8 + g) = (uint8_t)(rnd() % 12);
            if (rnd() % 2) W(0x4E0F + g) = (uint8_t)(W(0x4DB8 + g) + (int)(rnd() % 3) - 1);
        }
        W(0x4E12) = (rnd() % 3) ? 0 : (uint8_t)(1 + rnd() % 2);
        { static const uint8_t kc[3] = {7, 0x11, 0x20}; W(0x4D9F) = (rnd() % 2) ? kc[rnd() % 3] : (uint8_t)rnd(); }
        if (rnd() % 2) W(0x4E0E) = W(0x4D9E);
        { const unsigned cur = W(0x4D97) | (W(0x4D98) << 8); if (rnd() % 2) word(0x4D95, (cur + 1) & 0xFFFF); else if (rnd() % 2) word(0x4D95, cur); }
        uint8_t video[0x400], colour[0x400];
        for (auto &b : video) b = (uint8_t)rnd();
        for (auto &b : colour) b = (uint8_t)rnd();
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            std::memcpy(m->bus.videoram, video, sizeof video);
            std::memcpy(m->bus.colorram, colour, sizeof colour);
            m->bus.write((uint16_t)(sp - 2), kRet & 255);
            m->bus.write((uint16_t)(sp - 1), kRet >> 8);
        }
        const uint16_t bc = (uint16_t)rnd(), hl = (uint16_t)rnd(), de = (uint16_t)rnd(), af = (uint16_t)rnd(), ix = (uint16_t)rnd(), iy = (uint16_t)rnd();
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
        std::fprintf(stderr, "usage: test_rules_timers PROG.BIN FRAMES [ISOLATED_CASES]\n");
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
    std::printf("part 1: %ld isolated whole calls of each routine with garbage in all registers: 1066 differences %lu; Pinky 2069 %lu; Inky 208C %lu; Clyde 20AF %lu; inactivity 13DD %lu\n", cases, bad[0], bad[1], bad[2], bad[3], bad[4]);
    std::printf("  (the hook gave way to the ROM in %lu / %lu / %lu / %lu / %lu of those calls)\n", fb[0], fb[1], fb[2], fb[3], fb[4]);

    // ---------------- Part 2
    static Machine::RuleHook counted[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x1066] = c0;
    counted[0x2069] = c1;
    counted[0x208C] = c2;
    counted[0x20AF] = c3;
    counted[0x13DD] = c4;
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
    std::printf("part 2: %ld frames; the original machine entered 1066 %lu, 2069 %lu, 208C %lu, 20AF %lu, 13DD %lu times; the hooks ran %lu / %lu / %lu / %lu / %lu times (the hooks gave way to the ROM %lu times)\n",
                frames, entries[0], entries[1], entries[2], entries[3], entries[4], g_calls[0], g_calls[1], g_calls[2], g_calls[3], g_calls[4], rules::fallbacks);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, why_b);
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "DIFFER");

    // ---------------- Part 3
    struct Wrong { const char *name; Routine r; unsigned pc; Machine::RuleHook hook; };
    const Wrong wrongs[] = {{"1066 one cycle too slow", kEaten, 0x1066, w0}, {"Pinky: out flag flipped", kPinky, 0x2069, w1}, {"Inky: common counter changed", kInky, 0x208C, w2},
                            {"Clyde one cycle too slow", kClyde, 0x20AF, w3}, {"inactivity counter changed", kInactive, 0x13DD, w4}};
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
    bool counts = true;
    for (int r = 0; r < 5; ++r) counts = counts && entries[r] == g_calls[r] && entries[r] > 0;
    const bool ok = total_bad == 0 && bad_b < 0 && reg_b < 0 && caught == 5 && counts;
    std::printf("%s test_rules_house: %ld isolated calls per routine, %ld frames, 0 differences required\n", ok ? "PASS" : "FAIL", cases, frames);
    return ok ? 0 : 1;
}
