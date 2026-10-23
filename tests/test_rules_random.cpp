// T02a (moved onto the shared harness tests/lockstep.h in step N08): the readable random numbers IN the game (src/rules/rules.h, Machine::rules) against the
// original ROM routine, three machines in lockstep.
// Usage: test_rules_random PROG.BIN FRAMES [COVERAGE_FRAMES]   (COVERAGE_FRAMES: also run coverage players A and B through the harness, default 0)
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
#include "lockstep.h"
#include "coverage_player.h"

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

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_rules_random PROG.BIN FRAMES [COVERAGE_FRAMES]\n");
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
    const unsigned long cov_frames = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 0;
    static Machine::RuleHook counted[0x10000] = {}, wrong[0x10000] = {};
    for (int i = 0; i < 0x10000; ++i) counted[i] = rules::table()[i];
    counted[0x2A23] = counted_2A23;
    wrong[0x2A23] = wrong_2A23;

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

    // ---- parts 1 and 2: the original machine (reference), the readable rules (B), the readable rules with garbage in the scratch registers (C)
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("B: readable rules (2A23 counted)", counted, false);
    h.add_variant("C: all readable rules, garbage in F, DE, HL", rules::table(), true);
    h.watch_entry(0x2A23);
    lockstep::g_harness = &h;
    static Machine A(prog);
    unsigned long frightened_frames = 0;
    for (long fr = 0; fr < frames && !h.all_stopped(); ++fr) {
        joystick(fr);
        inputs(A, fr);
        probe::run_frame(A, [](uint16_t) {});
        if (A.bus.read(0x4DA6)) ++frightened_frames; // 4DA6: ghosts are blue
    }
    lockstep::g_harness = nullptr;
    const unsigned long entries = (unsigned long)h.entries(0x2A23), calls_part1 = g_hook_calls;
    const long bad_b = h.first_difference_frame(0), bad_c = h.first_difference_frame(1), reg_b = h.first_register_difference_frame(0);
    std::printf("part 1: %ld frames, original machine entered ROM 2A23 %lu times; the readable rule ran %lu times (frames with blue ghosts: %lu)\n", frames, entries,
                g_hook_calls, frightened_frames);
    std::printf("  readable rules vs original: %s\n", bad_b < 0 ? "identical on every frame (all memory, I/O writes, cycle count of every frame)" : "DIFFERENT");
    if (bad_b >= 0) std::printf("  first difference at frame %ld: %s\n", bad_b, h.first_difference_what(0));
    std::printf("  CPU registers at the end of every frame: %s\n", reg_b < 0 ? "identical too" : "differ (see below)");
    if (reg_b >= 0) std::printf("  first register difference at frame %ld (registers the adapter leaves alone)\n", reg_b);
    std::printf("part 2: garbage in F, DE, HL after every call: %s\n", bad_c < 0 ? "identical on every frame" : "DIFFERENT");
    if (bad_c >= 0) std::printf("  first difference at frame %ld: %s\n", bad_c, h.first_difference_what(1));
    h.report(stdout);

    // ---- part 3: a deliberately wrong adapter must be caught
    lockstep::Harness hw(prog);
    hw.add_variant("W: wrong random number (seed * 5 + 3)", wrong, false);
    lockstep::g_harness = &hw;
    static Machine W(prog);
    for (long fr = 0; fr < 40000 && !hw.all_stopped(); ++fr) {
        joystick(fr);
        inputs(W, fr);
        probe::run_frame(W, [](uint16_t) {});
    }
    lockstep::g_harness = nullptr;
    const long caught = hw.first_difference_frame(0);
    std::printf("part 3: a wrong adapter (seed * 5 + 3): %s\n", caught >= 0 ? "caught" : "NOT caught");
    if (caught >= 0) std::printf("  first difference at frame %ld\n", caught);

    // ---- part 4 (N08): the coverage players of N02 and N03 as input; they start new machines (other DIP settings) and poke RAM between frames
    bool cov_ok = true;
    if (cov_frames) {
        lockstep::Harness hc(prog);
        hc.load_routine_map("docs/notes/rom_routines.md");
        hc.add_variant("readable rules (2A23 counted)", counted, false);
        hc.watch_entry(0x2A23);
        lockstep::g_harness = &hc;
        static Machine PA(prog);
        covplayer::run(PA, cov_frames, nullptr);
        covplayer::run_b(prog, cov_frames, nullptr);
        lockstep::g_harness = nullptr;
        std::printf("part 4: coverage players A and B through the harness, %lu frames each\n", cov_frames);
        hc.report(stdout);
        cov_ok = hc.all_identical() && hc.entries(0x2A23) > 0 && hc.frames() >= 2 * cov_frames;
        std::printf("  coverage players in lockstep: %s\n", cov_ok ? "identical on every frame" : "DIFFERENT or too short");
    }
    const bool ok = bad_b < 0 && bad_c < 0 && caught >= 0 && entries == calls_part1 && entries > 0 && cov_ok;
    std::printf("%s test_rules_random: %ld frames, %lu calls of the readable rule, 0 differences required\n", ok ? "PASS" : "FAIL", frames, calls_part1);
    return ok ? 0 : 1;
}
