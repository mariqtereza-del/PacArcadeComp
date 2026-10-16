// T02j: every example mod in mods/ is loaded and must change what its comment says it changes.
// Usage: test_mods PROG.BIN FRAMES FILE...    (the files are mods/slow_ghosts.cfg, mods/long_blue_time.cfg, mods/double_points.cfg and any other .cfg in mods/)
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static int fails = 0, checks = 0;
static void chk(const std::string &what, bool ok) {
    ++checks;
    if (!ok) {
        ++fails;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}
static std::vector<uint8_t> g_prog;

struct Result {
    unsigned long ghost_steps = 0;
    unsigned frightened_time = 0;  // the level's frightened time (4DBD) after the level has started
    std::set<unsigned> dot_deltas; // score changes on the frames when exactly one dot or energizer was eaten
    unsigned long dots = 0;
};
static unsigned bcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }
static Result play(const rules::Tuning &tune, long frames) {
    static Machine M(g_prog);
    M.~Machine();
    new (&M) Machine(g_prog);
    rules::tuning() = tune;
    M.rules = rules::table();
    Result r;
    uint32_t g = 5;
    int dir = 4, hold = 0;
    unsigned prev[4][2] = {{0}}, pdots = 0, pscore = 0;
    bool first = true;
    for (long fr = 0; fr < frames; ++fr) {
        const long p = fr % 15000;
        if (p >= 430 && hold <= 0) {
            g = g * 1664525u + 1013904223u;
            static const int d[4] = {1, 2, 4, 8};
            dir = d[(g >> 24) & 3];
            hold = 10 + (int)((g >> 16) & 31);
        }
        --hold;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
        if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20;
        if (p >= 430) in0 &= (uint8_t)~dir;
        M.bus.in0 = in0;
        M.bus.in1 = in1;
        M.bus.dsw1 = 0xC9;
        M.run_frame();
        const uint8_t *w = M.bus.workram;
        auto R = [&](unsigned a) { return w[a - 0x4C00]; };
        for (int k = 0; k < 4; ++k) {
            const unsigned y = R(0x4D00 + 2 * k), x = R(0x4D01 + 2 * k);
            if (!first && (y != prev[k][0] || x != prev[k][1])) ++r.ghost_steps;
            prev[k][0] = y, prev[k][1] = x;
        }
        if (fr == 3000) r.frightened_time = R(0x4DBD) | (R(0x4DBE) << 8);
        const unsigned dots = R(0x4E0E), score = bcd(R(0x4E80)) + 100 * bcd(R(0x4E81)) + 10000 * bcd(R(0x4E82));
        if (!first && R(0x4E00) == 3 && dots == pdots + 1 && score > pscore) r.dot_deltas.insert(score - pscore);
        if (!first && dots == pdots + 1) ++r.dots;
        pdots = dots, pscore = score;
        first = false;
    }
    rules::tuning().reset();
    return r;
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: test_mods PROG.BIN FRAMES FILE...\n");
        return 2;
    }
    g_prog.resize(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(g_prog.data(), 1, g_prog.size(), f) != g_prog.size()) return 2;
    std::fclose(f);
    const long frames = std::atol(argv[2]);
    const Result base = play(rules::Tuning(), frames);
    std::printf("original rules, %ld frames: ghost steps %lu, frightened time of the level %u, dot score changes {", frames, base.ghost_steps, base.frightened_time);
    for (unsigned d : base.dot_deltas) std::printf(" %u", d);
    std::printf(" }, dots %lu\n", base.dots);
    chk("the original dots are worth 10 or 50", base.dot_deltas == std::set<unsigned>({10, 50}) || base.dot_deltas == std::set<unsigned>({10}));
    chk("the original frightened time is not 0", base.frightened_time > 0);
    for (int i = 3; i < argc; ++i) {
        const std::string path = argv[i];
        const std::string name = path.substr(path.find_last_of("/\\") + 1);
        rules::Tuning t;
        std::string problems, error;
        const bool loaded = rules::load_file(path, t, &problems, &error);
        chk(name + ": loads", loaded);
        chk(name + ": no wrong lines", problems.empty());
        chk(name + ": it is a mod (some value differs from the standard)", t.active);
        if (!loaded) continue;
        const Result r = play(t, frames);
        std::printf("%s: ghost steps %lu, frightened time %u, dot score changes {", name.c_str(), r.ghost_steps, r.frightened_time);
        for (unsigned d : r.dot_deltas) std::printf(" %u", d);
        std::printf(" }, dots %lu\n", r.dots);
        if (name == "slow_ghosts.cfg") {
            chk(name + ": ghosts make clearly fewer steps (under 80 percent of the original)", r.ghost_steps * 100 < base.ghost_steps * 80);
            chk(name + ": ghosts are not frozen (over 30 percent)", r.ghost_steps * 100 > base.ghost_steps * 30);
            chk(name + ": the frightened time is unchanged", r.frightened_time == base.frightened_time);
        } else if (name == "long_blue_time.cfg") {
            chk(name + ": the frightened time is twice the original", r.frightened_time == 2 * base.frightened_time);
            chk(name + ": the ghosts' speed is not changed (same ghost steps within 25 percent)", r.ghost_steps * 100 < base.ghost_steps * 125 && r.ghost_steps * 100 > base.ghost_steps * 75);
        } else if (name == "double_points.cfg") {
            chk(name + ": every dot or energizer is worth double (20 or 100)", !r.dot_deltas.empty() && std::all_of(r.dot_deltas.begin(), r.dot_deltas.end(), [](unsigned d) { return d == 20 || d == 100; }));
            chk(name + ": the four ghost values are doubled", t.points[logic::kGhost1] == 400 && t.points[logic::kGhost2] == 800 && t.points[logic::kGhost3] == 1600 && t.points[logic::kGhost4] == 3200);
            chk(name + ": the extra life comes at 20000", t.extra_life_score == 20000);
        } else {
            std::printf("  (no special check for %s: only loading was checked)\n", name.c_str());
        }
    }
    std::printf("%s test_mods (game part): %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails ? 1 : 0;
}
