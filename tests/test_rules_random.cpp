// T02a: the readable random numbers IN the game (src/rules/rules.h, Machine::rules) against the original ROM routine, three machines in lockstep.
// Usage: test_rules_random PROG.BIN FRAMES
//   A = the original machine (ROM routine 2A23 runs; its frame loop also counts every entry of 2A23)
//   B = the same machine with the readable rules on (Machine::run_frame, the real game loop, exactly as the window program runs it)
//   C = like B, but every register the adapter leaves alone (F, DE, HL) is filled with garbage after each call (rules::poison)
// The same scripted player as T01a drives all three (coins, start, a new random direction every 10 to 41 frames; five lives; the inputs do not depend
// on the machine). After EVERY frame the
// whole state is compared: video RAM, colour RAM, work RAM, sprite RAM and coordinates, sound registers, latches, interrupt vector, watchdog count,
// the list of I/O writes of the frame, the cycle count, and all CPU registers. Then a deliberately wrong adapter is run for a short while: it must be caught.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static unsigned long g_hook_calls = 0;
static void counted_2A23(Machine &m) {
    ++g_hook_calls;
    rules::random_byte_2A23(m);
}
static void wrong_2A23(Machine &m) { // deliberately wrong: the step is seed * 5 + 3 instead of + 1
    uint16_t seed = (uint16_t)(m.bus.read(0x4DC9) | (m.bus.read(0x4DCA) << 8));
    seed = (uint16_t)((seed * 5 + 3) & 0x1FFF);
    m.bus.write(0x4DC9, seed & 0xFF);
    m.bus.write(0x4DCA, seed >> 8);
    m.on_set_a(m.bus.rom[seed]);
    rules::return_like_ret(m, 111, 13);
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

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_random PROG.BIN FRAMES\n");
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
    static Machine::RuleHook counted[0x10000] = {}, wrong[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x2A23] = counted_2A23;
    wrong[0x2A23] = wrong_2A23;
    static Machine A(prog), B(prog), C(prog);
    B.rules = counted;
    C.rules = rules::table();
    uint32_t rng = 12345;
    int dir = 0x04, hold = 0;
    auto joystick = [&](long fr) { // T01a's player: a new random direction every 10 to 41 frames
        if (fr % 12000 >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
        }
    };
    auto inputs = [&](Machine &x, long fr) { // the scripted player: depends only on the frame number and its own random numbers
        long p = fr % 12000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20; // coin
        if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20; // start 1
        if (p >= 430) in0 &= (uint8_t)~dir;
        x.bus.in0 = in0;
        x.bus.in1 = in1;
        x.bus.dsw1 = 0xCD; // five lives
    };
    unsigned long entries = 0, frightened_frames = 0;
    long bad_b = -1, bad_c = -1, reg_b = -1;
    const char *why_b = nullptr, *why_c = nullptr;
    for (long fr = 0; fr < frames; ++fr) {
        joystick(fr);
        inputs(A, fr);
        inputs(B, fr);
        inputs(C, fr);
        const uint32_t a0 = A.ticks_in_frame();
        probe::run_frame(A, [&](uint16_t pc) {
            if (pc == 0x2A23) ++entries;
        });
        const uint32_t a_cycles = A.ticks_in_frame() - a0; // the probe's loop does not reset the counter; Machine::run_frame does
        B.run_frame();
        rules::poison = (0x9E3779B9u ^ (uint32_t)fr) | 1; // C: garbage in the scratch registers (a different pattern every frame)
        C.run_frame();
        rules::poison = 0;
        if (A.bus.read(0x4DA6)) ++frightened_frames; // 4DA6: ghosts are blue
        if (bad_b < 0) {
            const char *w = differ(A, B, false);
            if (!w && a_cycles != B.ticks_in_frame()) w = "cycle count of the frame";
            if (w) bad_b = fr, why_b = w;
            else if (reg_b < 0 && differ(A, B, true)) reg_b = fr;
        }
        if (bad_c < 0) {
            const char *w = differ(A, C, false);
            if (!w && a_cycles != C.ticks_in_frame()) w = "cycle count of the frame";
            if (w) bad_c = fr, why_c = w;
        }
        if (bad_b >= 0 && bad_c >= 0) break;
    }
    std::printf("part 1: %ld frames, original machine entered ROM 2A23 %lu times; the readable rule ran %lu times (frames with blue ghosts: %lu)\n", frames, entries,
                g_hook_calls, frightened_frames);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, why_b);
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "differ (see below)");
    if (reg_b >= 0) std::printf("  first register difference at frame %ld (registers the adapter leaves alone)\n", reg_b);
    std::printf("part 2: garbage in F, DE, HL after every call: %s\n", bad_c < 0 ? "identical on every frame" : "DIFFERENT");
    if (bad_c >= 0) std::printf("  first difference at frame %ld: %s\n", bad_c, why_c);
    // part 3: a deliberately wrong adapter must be caught
    static Machine W0(prog), W1(prog);
    W1.rules = wrong;
    long caught = -1;
    for (long fr = 0; fr < 40000 && caught < 0; ++fr) {
        joystick(fr);
        inputs(W0, fr);
        inputs(W1, fr);
        W0.run_frame();
        W1.run_frame();
        if (differ(W0, W1, false)) caught = fr;
    }
    std::printf("part 3: a wrong adapter (seed * 5 + 3): %s\n", caught >= 0 ? "caught" : "NOT caught");
    if (caught >= 0) std::printf("  first difference at frame %ld\n", caught);
    const bool ok = bad_b < 0 && bad_c < 0 && caught >= 0 && entries == g_hook_calls && entries > 0;
    std::printf("%s test_rules_random: %ld frames, %lu calls of the readable rule, 0 differences required\n", ok ? "PASS" : "FAIL", frames, g_hook_calls);
    return ok ? 0 : 1;
}
