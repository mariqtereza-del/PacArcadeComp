// N48: how many ROM instructions does the machine with all readable rules + all native code still execute? (the central claim of Option A, measured)
// A ROM instruction is counted when the Z80 core fetches it from the program ROM (pc < 0x4000) because no native hook handles that address, or because the hook handed the
// instruction back to the ROM (rules::fallbacks went up). The runs: the S27 replay, the scripted player of T01a (FRAMES frames), the coverage players A, B, C (COVERAGE frames
// each) and D (a fifth). Every executed ROM address is listed with its count and the first frame it was seen in.
// Usage: test_no_rom_code PROG.BIN FRAMES COVERAGE_FRAMES [REPLAY_FILE]
#include <cstdio>
#include <cstdlib>
#include <array>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"
#include "../src/game/attract.h"

namespace {
std::map<uint16_t, unsigned long long> g_count;
std::map<uint16_t, std::string> g_first;
std::string g_phase;
uint16_t g_pending = 0; bool g_has_pending = false; unsigned long g_fb_at = 0;
const Machine::RuleHook *g_table = nullptr;
unsigned long long g_fetches_total = 0;   // every instruction fetch below 0x4000 that the Z80 core would do without any hook (for the percentage)

void note(uint16_t pc, Machine &m) {
    ++g_count[pc];
    if (!g_first.count(pc)) g_first[pc] = g_phase + " frame " + std::to_string((unsigned long long)m.frame);
}
void probe_fn(Machine &m, uint16_t pc) {
    if (g_has_pending && rules::fallbacks != g_fb_at && g_pending < 0x4000) note(g_pending, m);
    g_has_pending = false;
    if (pc >= 0x4000) return;
    ++g_fetches_total;
    if (g_table[pc]) { g_pending = pc; g_has_pending = true; g_fb_at = rules::fallbacks; return; }
    note(pc, m);
}

struct Player {   // the scripted player of T01a (as in test_rules_random.cpp)
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
} // namespace

int main(int argc, char **argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: test_no_rom_code PROG.BIN FRAMES COVERAGE_FRAMES [REPLAY_FILE]\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    const long frames = std::atol(argv[2]);
    const unsigned long cov = std::strtoul(argv[3], nullptr, 10);
    g_table = rules::table();
    // ---- part 1: the frame end between the prefix byte and the opcode of an indexed instruction (hook_prefix): every such address, random states, the ROM against the native hook
    {
        unsigned long long cases = 0, bad = 0, wrong_caught = 0, wrong_cases = 0;
        uint32_t rng = 777;
        auto rnd = [&]() { rng = rng * 1664525u + 1013904223u; return rng >> 8; };
        static Machine base(prog);
        const int per = argc > 5 ? std::atoi(argv[5]) : 300;
        for (int pass = 0; pass < 2; ++pass)   // pass 1: a deliberately wrong hook (one cycle too slow) must be caught
            for (int i = 0; i < game::att::kPrefixMidN; ++i) {
                const uint16_t mid = game::att::kPrefixMid[i];
                for (int c = 0; c < per; ++c) {
                    Machine X = base;
                    for (unsigned ad = 0x4000; ad < 0x4FF0; ++ad) X.bus.write((uint16_t)ad, (uint8_t)rnd());
                    X.on_set_af((uint16_t)rnd()); X.on_set_bc((uint16_t)rnd()); X.on_set_de((uint16_t)rnd()); X.on_set_hl((uint16_t)rnd());
                    X.on_set_ix((uint16_t)(0x4000 + (rnd() % 0xF00))); X.on_set_iy((uint16_t)(0x4000 + (rnd() % 0xF00))); X.on_set_sp((uint16_t)(0x4F00 + (rnd() % 0xE0) * 1));
                    X.on_set_pc((uint16_t)(mid - 1));
                    X.rules = nullptr;
                    Machine Y = X;
                    Y.rules = rules::table();
                    X.on_step(); Y.on_step();   // the prefix byte: a step of its own, in both
                    X.on_step();                // the ROM runs the opcode
                    rules::table()[mid](Y);     // the native hook finishes the instruction
                    if (pass == 1) Y.on_tick(1);
                    const char *w = lockstep::differ(X, Y, true);
                    if (!w && X.on_get_wz() != Y.on_get_wz()) w = "WZ";
                    if (!w && X.ticks_in_frame() != Y.ticks_in_frame()) w = "cycle count";
                    if (pass == 0) { ++cases; if (w) { if (++bad <= 5) std::printf("  PREFIX MISMATCH at %04X: %s\n", mid, w); } }
                    else { ++wrong_cases; if (w) ++wrong_caught; }
                }
            }
        std::printf("part 1: frame end between the prefix byte and the opcode (hook_prefix): %d addresses, %llu random cases, differences %llu\n", game::att::kPrefixMidN, cases, bad);
        std::printf("part 1: wrong version (one cycle too slow) caught in %llu of %llu cases\n", wrong_caught, wrong_cases);
        if (bad || wrong_caught != wrong_cases) { std::printf("FAIL test_no_rom_code (prefix hook)\n"); return 1; }
    }
    lockstep::g_pc_probe = probe_fn;
    lockstep::g_force_rules = g_table;
    unsigned long long total_frames = 0;
    if (argc > 4) {   // the S27 replay: "frame in0 in1" lines (a value holds until the next line), header "dip XX", "frames N"
        std::FILE *rf = std::fopen(argv[4], "r");
        if (!rf) { std::fprintf(stderr, "cannot read %s\n", argv[4]); return 2; }
        std::vector<std::array<unsigned, 3>> ent; unsigned dip = 0xC9; long nfr = 0; char line[256];
        while (std::fgets(line, sizeof line, rf)) {
            unsigned a, b, c;
            if (!std::strncmp(line, "dip ", 4)) dip = (unsigned)std::strtoul(line + 4, nullptr, 16);
            else if (!std::strncmp(line, "frames ", 7)) nfr = std::atol(line + 7);
            else if (line[0] != '#' && std::sscanf(line, "%u %x %x", &a, &b, &c) == 3) ent.push_back({a, b, c});
        }
        std::fclose(rf);
        g_phase = "replay";
        static Machine R(prog);
        size_t k = 0; unsigned in0 = 0xFF, in1 = 0xFF;
        for (long fr = 0; fr < nfr; ++fr) {
            while (k < ent.size() && ent[k][0] <= (unsigned)fr) { in0 = ent[k][1]; in1 = ent[k][2]; ++k; }
            R.bus.in0 = (uint8_t)in0; R.bus.in1 = (uint8_t)in1; R.bus.dsw1 = (uint8_t)dip;
            probe::run_frame(R, [](uint16_t) {});
        }
        total_frames += (unsigned long long)nfr;
        std::printf("replay: %ld frames run\n", nfr);
    }
    {
        g_phase = "scripted player";
        Player pl;
        static Machine A(prog);
        for (long fr = 0; fr < frames; ++fr) {
            pl.joystick(fr);
            pl.inputs(A, fr);
            probe::run_frame(A, [](uint16_t) {});
        }
        total_frames += (unsigned long long)frames;
        std::printf("scripted player (T01a): %ld frames run\n", frames);
    }
    {
        static Machine PA(prog);
        g_phase = "coverage player A"; covplayer::run(PA, cov, nullptr);
        g_phase = "coverage player B"; covplayer::run_b(prog, cov, nullptr);
        g_phase = "coverage player C"; covplayer::run_c(prog, cov, nullptr);
        g_phase = "coverage player D"; covplayer::run_d(prog, cov / 5 ? cov / 5 : 1, nullptr);
        total_frames += cov * 3 + cov / 5;
        std::printf("coverage players A, B, C: %lu frames each, D: %lu frames\n", cov, cov / 5);
    }
    lockstep::g_pc_probe = nullptr;
    lockstep::g_force_rules = nullptr;
    unsigned long long rom_instr = 0;
    for (const auto &kv : g_count) rom_instr += kv.second;
    std::printf("TOTAL FRAMES RUN: %llu\n", total_frames);
    std::printf("ROM instructions executed by the Z80 core (not handled by native code): %llu of %llu instruction starts below 4000 seen (%.6f percent)\n", rom_instr, g_fetches_total, g_fetches_total ? 100.0 * (double)rom_instr / (double)g_fetches_total : 0.0);
    std::printf("distinct ROM addresses executed: %zu\n", g_count.size());
    for (const auto &kv : g_count) std::printf("  ROM %04X executed %llu times, first at %s\n", kv.first, kv.second, g_first[kv.first].c_str());
    const bool ok = g_count.empty();
    std::printf("%s test_no_rom_code (%s)\n", ok ? "PASS" : "FAIL", ok ? "zero ROM instructions" : "ROM instructions are still executed");
    return ok ? 0 : 1;
}
