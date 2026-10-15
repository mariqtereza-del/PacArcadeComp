// T01a: the readable random number generator (src/logic/random_number.h) against the ROM routine at 2A23, side by side on the real machine.
// Usage: test_logic_random PROG.BIN FRAMES
// A scripted player (coins, start, random joystick moves; five lives so the games last) plays many games. Every time the ROM enters 2A23 the readable
// version is given the same seed; when the ROM reaches the RET at 2A34 the answer (register A) and the new seed (RAM 4DC9/4DCA, register HL) must be
// identical. Every frame the seed in RAM must also be the one the readable version expects. Part of the run (the first REF_FRAMES frames) is also
// run on a second machine with Machine::run_frame, to prove that the probe's own frame loop is the real one.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "logic_probe.h"
#include "../src/logic/random_number.h"

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_logic_random PROG.BIN FRAMES\n");
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
    const long REF_FRAMES = 300000;
    static Machine m(prog), ref(prog);
    unsigned long calls = 0, mismatches = 0, outside = 0, resets = 0, strange = 0, ref_bad = 0, games = 0, pills = 0, frames_playing = 0;
    std::set<unsigned> seeds, answers;
    unsigned long answer_hist[256] = {0};
    bool pending = false;
    uint16_t entry_seed = 0, expect_seed = 0;
    uint8_t expect_a = 0;
    unsigned shadow = 0;
    bool shadow_known = false;
    uint32_t rng = 12345;
    int hold = 0;
    int dir = 0x04;
    long cyc = 0;
    int last_state = -1, last_pill = 0;
    auto set_inputs = [&](Machine &x, long fr) {
        long p = fr % 12000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20; // coin
        if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20; // start 1
        if (p >= 430) { in0 &= (uint8_t)~dir; }
        x.bus.in0 = in0;
        x.bus.in1 = in1;
        x.bus.dsw1 = 0xCD; // DIP: five lives
    };
    auto before = [&](uint16_t pc) {
        if (pc == 0x2A23) {
            entry_seed = (uint16_t)probe::rd16(m, logic::kRandomSeedAddress);
            if (shadow_known && entry_seed != shadow) {
                ++outside;
                if (entry_seed == 0) ++resets;
                else ++strange;
            } // somebody else wrote the seed since the last call (the ROM clears it to 0 when a game starts)
            uint16_t s = entry_seed;
            expect_a = logic::get_random_byte(s, prog.data());
            expect_seed = s;
            pending = true;
        } else if (pc == 0x2A34 && pending) { // the RET: the routine is finished
            pending = false;
            ++calls;
            unsigned a = m.on_get_a(), hl = m.on_get_hl(), ram = probe::rd16(m, logic::kRandomSeedAddress);
            if (a != expect_a || hl != expect_seed || ram != expect_seed) {
                if (++mismatches <= 5)
                    std::printf("  MISMATCH at frame %u call %lu: seed in %04X; ROM gave A=%02X HL=%04X RAM seed=%04X; readable gave A=%02X seed=%04X\n", m.frame, calls,
                                entry_seed, a, hl, ram, expect_a, expect_seed);
            }
            shadow = expect_seed;
            shadow_known = true;
            seeds.insert(entry_seed);
            answers.insert(a);
            ++answer_hist[a & 255];
        }
    };
    for (long fr = 0; fr < frames; ++fr) {
        set_inputs(m, fr);
        // the scripted player's joystick: a new random direction every 10 to 41 frames
        if (fr % 12000 >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
        }
        set_inputs(m, fr);
        probe::run_frame(m, before);
        if (fr < REF_FRAMES) {
            set_inputs(ref, fr);
            ref.run_frame();
            if (std::memcmp(ref.bus.workram, m.bus.workram, sizeof m.bus.workram) || std::memcmp(ref.bus.videoram, m.bus.videoram, sizeof m.bus.videoram) ||
                std::memcmp(ref.bus.spriteram, m.bus.spriteram, sizeof m.bus.spriteram)) {
                if (++ref_bad <= 3) std::printf("  probe loop differs from Machine::run_frame at frame %ld\n", fr + 1);
            }
        }
        // memory compared every frame: the seed in RAM must be the one the readable version expects (it changes only in the calls)
        unsigned ram = probe::rd16(m, logic::kRandomSeedAddress);
        if (shadow_known && !pending && ram != shadow) {
            ++outside;
            if (ram == 0) ++resets;
            else if (++strange <= 5) std::printf("  the seed was changed by other code to %04X (not a reset to 0) at frame %ld\n", ram, fr + 1);
            shadow = ram;
        }
        int state = (int)probe::rd(m, 0x4E00);
        if (state == 3) ++frames_playing;
        if (state == 3 && last_state != 3) ++games;
        last_state = state;
        int pill = (int)probe::rd(m, 0x4DA6);
        if (pill && !last_pill) ++pills;
        last_pill = pill;
        (void)cyc;
    }
    std::printf("frames run %ld (%.1f hours of game time); frames with the maze running %lu; maze starts %lu; energizers eaten %lu\n", frames, frames / 60.606 / 3600.0,
                frames_playing, games, pills);
    std::printf("ROM calls of 2A23 %lu; distinct seeds %zu of 8192; distinct answers %zu of 256; times other code changed the seed %lu (%lu resets to 0000 by the "
                "start-of-game clear, %lu anything else)\n",
                calls, seeds.size(), answers.size(), outside, resets, strange);
    std::printf("probe loop versus Machine::run_frame: %s over the first %ld frames\n", ref_bad ? "DIFFERENT" : "identical RAM every frame",
                frames < REF_FRAMES ? frames : REF_FRAMES);
    // Part B: the routine alone, for EVERY possible 16-bit seed (65,536 of them, also the ones a game never reaches): call 2A23 directly on a
    // fresh machine, step until it returns, compare with the readable version.
    unsigned long b_bad = 0;
    {
        static Machine c(prog);
        for (unsigned seed = 0; seed < 0x10000; ++seed) {
            c.bus.write(logic::kRandomSeedAddress, (uint8_t)(seed & 255));
            c.bus.write(logic::kRandomSeedAddress + 1, (uint8_t)(seed >> 8));
            const uint16_t sp = 0x4FC0, ret = 0x3F00;
            c.bus.write((uint16_t)(sp - 2), (uint8_t)(ret & 255));
            c.bus.write((uint16_t)(sp - 1), (uint8_t)(ret >> 8));
            c.on_set_sp((uint16_t)(sp - 2));
            c.on_set_pc(0x2A23);
            int guard = 0;
            while (c.on_get_pc() != ret && ++guard < 100) c.on_step();
            uint16_t s = (uint16_t)seed;
            uint8_t want_a = logic::get_random_byte(s, prog.data());
            unsigned a = c.on_get_a(), ram = probe::rd16(c, logic::kRandomSeedAddress);
            if (guard >= 100 || a != want_a || ram != s || c.on_get_hl() != s) {
                if (++b_bad <= 5) std::printf("  part B mismatch for seed %04X: ROM A=%02X seed=%04X, readable A=%02X seed=%04X\n", seed, a, ram, want_a, s);
            }
        }
    }
    std::printf("part B: all 65,536 starting seeds run through the ROM routine alone: %lu differences\n", b_bad);
    bool ok = b_bad == 0 && calls > 0 && mismatches == 0 && ref_bad == 0 && strange == 0;
    std::printf("%s: %lu calls compared, %lu differences\n", ok ? "PASS test_logic_random" : "FAIL test_logic_random", calls, mismatches);
    return ok ? 0 : 1;
}
