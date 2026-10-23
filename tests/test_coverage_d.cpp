// N02: runs service player D twice on fresh machines (the two runs must give the same checksum and the same coverage) and writes the coverage.
// Usage: test_coverage_d PROG.BIN FRAMES OUT_COVERAGE.txt
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "coverage_player.h"

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: test_coverage_d PROG.BIN FRAMES OUT_COVERAGE.txt\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    const unsigned long frames = std::strtoul(argv[2], nullptr, 10);
    romcov::Coverage c1, c2;
    const covplayer::Stats s1 = covplayer::run_d(prog, frames, &c1);
    const covplayer::Stats s2 = covplayer::run_d(prog, frames, &c2);
    std::printf("run 1: %lu frames, %u games started, %u lives lost, %u game overs, %u rounds cleared, highest level %d, %u cut short\n", s1.frames, s1.games, s1.deaths,
                s1.game_overs, s1.rounds_cleared, s1.max_level, s1.timeouts);
    std::printf("  checksum run 1 %s\n  checksum run 2 %s\n", s1.checksum.c_str(), s2.checksum.c_str());
    const bool same = s1.checksum == s2.checksum && c1.count() == c2.count();
    std::printf("  deterministic: %s; distinct instruction addresses run: %u\n", same ? "YES (identical frames and coverage)" : "NO", c1.count());
    c1.write(argv[3]);
    return same ? 0 : 1;
}
