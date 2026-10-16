// T02f/T02h: ALL readable rules together (src/rules/rules.h table(): random numbers, scoring, dots/energizers/fruit, ghost targeting, movement, timers and the level
// start) against the original ROM: the machine with every hook on versus the original, frame by frame, for FRAMES frames of scripted play (many games, other
// levels, other dip switch settings). After EVERY frame: all memory, I/O writes, the cycle count of the frame and all CPU registers.
// Usage: test_rules_all PROG.BIN FRAMES [GARBAGE]   (GARBAGE = 1: the registers the hooks are allowed to leave alone are filled with garbage after every hook)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

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





static std::vector<unsigned> g_low_bytes; // ROM addresses whose byte is a valid level number (0-20)

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_all PROG.BIN FRAMES [GARBAGE]\n");
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
    if (argc > 3 && std::atoi(argv[3])) rules::poison = 0x9E3779B1u;
    static Machine A(prog), B(prog);
    B.rules = rules::table();
    unsigned hooked = 0;
    for (int i = 0; i < 0x10000; ++i) hooked += B.rules[i] != nullptr;
    rules::fallbacks = 0;
    static unsigned long entries[0x10000];
    uint32_t rng = 99;
    int dir = 0x04, hold = 0;
    long bad = -1, regs = -1;
    const char *why = nullptr;
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
            m->bus.dsw1 = (uint8_t)(0xC9 | ((game % 2) ? 0x04 : 0) | ((game % 5 == 3) ? 0x30 : 0));
            if (p == 1000 && game % 3 == 1) m->bus.write(0x4E13, (uint8_t)(game % 24));
            if (p == 1200 && game % 4 == 2) m->bus.write(0x4E0E, 200);
        }
        const uint32_t a0 = A.ticks_in_frame();
        probe::run_frame(A, [&](uint16_t pc) { if (B.rules[pc]) ++entries[pc]; });
        const uint32_t a_cycles = A.ticks_in_frame() - a0;
        B.run_frame();
        const char *w = differ(A, B, false);
        if (!w && a_cycles != B.ticks_in_frame()) w = "cycle count of the frame";
        if (w) { bad = fr, why = w; break; }
        if (!rules::poison && regs < 0 && differ(A, B, true)) regs = fr;
    }
    unsigned long total = 0, kinds = 0;
    for (int i = 0; i < 0x10000; ++i) if (entries[i]) total += entries[i], ++kinds;
    std::printf("%ld frames with all %u readable rules on: the original entered the replaced routines %lu times (%lu different routines); the hooks gave way to the ROM %lu times\n", frames, hooked, total, kinds, rules::fallbacks);
    std::printf("  all readable rules vs original: %s\n", bad < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad >= 0) std::printf("  first difference at frame %ld: %s\n", bad, why);
    std::printf("  CPU registers at the end of every frame: %s\n", regs < 0 ? "identical too" : "DIFFER");
    const bool ok = bad < 0 && regs < 0 && total > 0;
    std::printf("%s test_rules_all: %ld frames, 0 differences required\n", ok ? "PASS" : "FAIL", frames);
    return ok ? 0 : 1;
}
