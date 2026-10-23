// N01: runs the real machine under two situations and writes which ROM instruction addresses ran.
// Usage: rom_map PROG.BIN REPLAY PLAYER_FRAMES OUT_PREFIX
//   situation 1 = the S27 replay (a one-player game: attract mode, coin, start, play, death);
//   situation 2 = the T01a scripted player (coin, start, a new random direction every 10 to 41 frames, five lives), PLAYER_FRAMES frames.
// Writes OUT_PREFIX_replay.txt, OUT_PREFIX_player.txt and OUT_PREFIX_all.txt (see tests/rom_coverage.h) and prints the counts.
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "logic_probe.h"
#include "rom_coverage.h"
#include "../src/app/replay.h"

int main(int argc, char **argv) {
    if (argc < 5) {
        std::fprintf(stderr, "usage: rom_map PROG.BIN REPLAY PLAYER_FRAMES OUT_PREFIX\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    Replay rp;
    std::string err;
    if (!rp.load(argv[2], err)) {
        std::fprintf(stderr, "replay: %s\n", err.c_str());
        return 2;
    }
    const long pframes = std::atol(argv[3]);
    const std::string prefix = argv[4];
    romcov::Coverage c1, c2, all;
    {
        static Machine m(prog);
        m.bus.dsw1 = rp.dip;
        for (uint32_t fr = 0; fr < rp.frames; ++fr) {
            uint8_t a, b;
            rp.inputs_at(fr, a, b);
            m.bus.in0 = a;
            m.bus.in1 = b;
            probe::run_frame(m, [&](uint16_t pc) { c1.mark(pc); });
        }
    }
    {
        static Machine m(prog);
        uint32_t rng = 12345;
        int dir = 0x04, hold = 0;
        for (long fr = 0; fr < pframes; ++fr) {
            const long p = fr % 12000;
            if (p >= 430) {
                if (hold <= 0) {
                    rng = rng * 1664525u + 1013904223u;
                    static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                    dir = d[(rng >> 24) & 3];
                    hold = 10 + (int)((rng >> 16) & 31);
                }
                --hold;
            }
            uint8_t in0 = 0xFF, in1 = 0xFF;
            if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
            if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20;
            if (p >= 430) in0 &= (uint8_t)~dir;
            m.bus.in0 = in0;
            m.bus.in1 = in1;
            m.bus.dsw1 = 0xCD;
            probe::run_frame(m, [&](uint16_t pc) { c2.mark(pc); });
        }
    }
    all.merge(c1);
    all.merge(c2);
    c1.write((prefix + "_replay.txt").c_str());
    c2.write((prefix + "_player.txt").c_str());
    all.write((prefix + "_all.txt").c_str());
    std::printf("distinct ROM instruction addresses executed: replay (%u frames) %u, player (%ld frames) %u, together %u\n", rp.frames, c1.count(), pframes, c2.count(),
                all.count());
    return 0;
}
