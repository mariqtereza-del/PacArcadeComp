// N39: two players: the turn changes and the per-player state. Audit test (no new routine): the code that switches between the players (ROM 0961-0971 the turn change,
// 0879-0A9C the per-player saved level state, 2448/2487/24C9 the saved dots, the PLAYER ONE / TWO messages and the cocktail flip) was made native in the earlier steps (N24, N31 and the
// T-series). This test proves that on two-player games: (1) lockstep of the original machine against the machine with all readable rules on the coverage players (which play two-player and
// cocktail games), counting the real turn changes; (2) a deliberately wrong version of the turn change is caught; (3) the list of ROM addresses of the per-player code that the ROM still executes.
// Usage: test_native_players PROG.BIN FRAMES [COVERAGE_FRAMES]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"
#include "../src/game/attract.h"

namespace {
Machine::RuleHook g_all[0x10000];
void wrong_turn(Machine &m) { rules::table()[0x0964](m); m.bus.write(0x4E09, (uint8_t)(m.bus.read(0x4E09) ^ 1)); }   // the new player number wrong
// ---- the scripted player of T01a (as in test_rules_random.cpp) ---------------------------------------------------------------------------------------
struct Player {
    uint32_t rng = 12345;
    int dir = 0x04, hold = 0;
    void joystick(long fr) {
        if (fr % 12000 >= 430) {
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
        }
    }
    void inputs(Machine &x, long fr) {
        long p = fr % 12000;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
        if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20;
        if (p >= 430) in0 &= (uint8_t)~dir;
        x.bus.in0 = in0;
        x.bus.in1 = in1;
        x.bus.dsw1 = 0xCD;
    }
};

std::set<uint16_t> g_seen;
// An address counts as executed by the ROM when no native hook handles it, or when its hook ran the ROM instruction itself (rules::fallbacks went up).
uint16_t g_pending = 0; bool g_has_pending = false; unsigned long g_fb_at = 0;
const Machine::RuleHook *g_meter_table = nullptr;
void seen_probe(Machine &, uint16_t pc) {
    if (g_has_pending && rules::fallbacks != g_fb_at && g_pending < 0x4000) g_seen.insert(g_pending);
    g_has_pending = false;
    if (pc >= 0x4000) return;
    if (g_meter_table && g_meter_table[pc]) { g_pending = pc; g_has_pending = true; g_fb_at = rules::fallbacks; return; }
    g_seen.insert(pc);
}

unsigned meter(const std::vector<uint8_t> &prog, const Machine::RuleHook *table, unsigned long frames) {
    g_seen.clear();
    g_meter_table = table;
    g_has_pending = false;
    lockstep::g_pc_probe = seen_probe;
    lockstep::g_force_rules = table;
    static Machine m(prog);
    m = Machine(prog);
    covplayer::run(m, frames, nullptr);
    covplayer::run_b(prog, frames, nullptr);
    covplayer::run_c(prog, frames, nullptr);
    covplayer::run_d(prog, frames / 5 ? frames / 5 : 1, nullptr);
    lockstep::g_pc_probe = nullptr;
    lockstep::g_force_rules = nullptr;
    return (unsigned)g_seen.size();
}


} // namespace

int main(int argc, char **argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: test_native_players PROG.BIN FRAMES [COVERAGE_FRAMES]\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    const long frames = std::atol(argv[2]);
    const unsigned long cov = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 20000;
    for (int i = 0; i < 0x10000; ++i) g_all[i] = rules::table()[i];
    bool ok = true;
    static const uint16_t kSites[] = {0x0961, 0x0964, 0x0969, 0x0972, 0x0879, 0x08C3, 0x092B, 0x09C8, 0x0A94, 0x0AA8, 0x0344, 0x0712, 0x125B, 0x12DB, 0x1423, 0x1494, 0x2448, 0x2487, 0x24C9, 0x2AAF, 0x2B0B, 0x184D};
    for (uint16_t a : kSites) if (a != 0x184D && !g_all[a] && a != 0x0969 && a != 0x08C3 && a != 0x092B && a != 0x09C8 && a != 0x0AA8 && a != 0x0344 && a != 0x0712 && a != 0x125B && a != 0x12DB && a != 0x1423 && a != 0x1494 && a != 0x2AAF && a != 0x2B0B) { std::printf("note: no hook starts at %04X (the address is inside a longer hooked routine)\n", a); }

    // ---- part 1: lockstep, with a wrong turn change as a third variant
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("A: all readable rules", g_all, false);
    static Machine::RuleHook wt[0x10000];
    for (int i = 0; i < 0x10000; ++i) wt[i] = nullptr;
    wt[0x0964] = wrong_turn;
    h.add_variant("W: the turn change 0964 wrong (must be caught)", wt, false);
    for (uint16_t a : {0x0961, 0x0964, 0x0972, 0x0A94}) h.watch_entry(a);
    lockstep::g_harness = &h;
    unsigned long switches = 0, two_player_frames = 0; uint8_t last = 0;
    {
        Player pl;
        static Machine A(prog);
        for (long fr = 0; fr < frames && !h.all_stopped(); ++fr) { pl.joystick(fr); pl.inputs(A, fr); probe::run_frame(A, [](uint16_t) {}); }
    }
    {
        static Machine PA(prog);
        covplayer::run(PA, cov, nullptr);
        covplayer::run_b(prog, cov, nullptr);
        covplayer::run_c(prog, cov, nullptr);
        covplayer::run_d(prog, cov / 5 ? cov / 5 : 1, nullptr);
    }
    lockstep::g_harness = nullptr;
    (void)switches; (void)two_player_frames; (void)last;
    std::printf("part 1: lockstep, scripted player %ld frames, then the coverage players A, B, C (%lu frames each, B and the cocktail and DIP variants play two-player games) and D (a fifth): %llu frames compared\n", frames, cov, (unsigned long long)h.frames());
    h.report(stdout);
    const long wrong_at = h.first_difference_frame(1);
    const long right_at = h.first_difference_frame(0);
    std::printf("  entries seen in the original machine: turn change 0961 %lu, 0964 %lu, level swap 0972 %lu, 0A94 %lu\n", (unsigned long)h.entries(0x0961), (unsigned long)h.entries(0x0964), (unsigned long)h.entries(0x0972), (unsigned long)h.entries(0x0A94));
    std::printf("part 1: all readable rules vs the original machine: %s\n", right_at < 0 ? "identical on every frame" : "DIFFERENT");
    if (right_at >= 0) ok = false;
    std::printf("part 2: wrong turn change (0964 flips the player number): %s%s\n", wrong_at >= 0 ? "caught at frame " : "NOT caught", wrong_at >= 0 ? std::to_string(wrong_at).c_str() : "");
    if (wrong_at < 0) ok = false;
    if (h.entries(0x0964) == 0) { std::printf("FAIL: no turn change was played\n"); ok = false; }

    // ---- part 3: the ROM addresses of the per-player code that the ROM still executes
    const unsigned long mf = cov < 20000 ? cov : 20000;
    const unsigned m_rom = meter(prog, nullptr, mf);
    const unsigned m_all = meter(prog, g_all, mf);
    meter(prog, g_all, mf);
    std::printf("part 3: progress meter (as in the earlier steps; coverage players of %lu frames): original ROM %u; readable rules + N12 to N38 %u (no routine is added by this step, so no drop is expected)\n", mf, m_rom, m_all);
    struct R { uint16_t lo, hi; const char *what; } regs[] = {
        {0x0961, 0x0972, "the turn change (toggles the player number 4E09)"}, {0x0972, 0x0988, "back to one player at game over"}, {0x0879, 0x0961, "the level start: per-player level numbers, dot counters, lives"},
        {0x0A94, 0x0ABF, "the per-player counters swapped at the turn change"}, {0x2448, 0x2487, "restoring the saved dots of the maze"}, {0x2487, 0x24C9, "saving the dots of the maze"},
        {0x24C9, 0x24D7, "clearing the saved dots"}, {0x2AAF, 0x2B17, "the score area of the player in play"}, {0x1423, 0x14A0, "the player in play: lives and flip"}, {0x184D, 0x1851, "the controls of player 2 (cocktail), inside Pac-Man's movement 1806"}};
    g_seen.clear(); g_meter_table = g_all;
    meter(prog, g_all, mf);
    for (const R &r : regs) {
        unsigned n = 0;
        for (uint16_t a : g_seen) if (a >= r.lo && a < r.hi) ++n;
        std::printf("  ROM %04X-%04X %s: %u ROM instruction addresses still executed\n", r.lo, r.hi, r.what, n);
    }
    std::printf("%s test_native_players\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
