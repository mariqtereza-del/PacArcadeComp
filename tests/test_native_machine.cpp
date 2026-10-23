// N49a: the machine without the Z80 core against the Z80 machine. This one source is built twice (tests/test_native_machine.sh):
//   - without PAC_NO_Z80: the Z80 machine runs the ORIGINAL ROM (no hooks at all);
//   - with PAC_NO_Z80: the machine whose CPU is only a register file (src/board/cpuregs.h) runs the native routine table (rules::table()).
// Both play the same inputs (the S27 replay, the scripted player of T01a, the coverage players A, B, C, D) and write one line per frame: a 64-bit
// fingerprint of all memory, the I/O writes of the frame, the cycle count of the frame and the CPU registers (the same things tests/lockstep.h compares).
// The two output files must be identical. Usage: test_native_machine PROG.BIN OUT.txt FRAMES COVERAGE_FRAMES REPLAY_FILE [WRONG]
// WRONG = a deliberately wrong version (the native machine takes one extra cycle in frame 1000): the files must then differ.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"

namespace {
std::FILE *g_out = nullptr;
const char *g_phase = "";
unsigned long g_phase_frame = 0;
bool g_wrong = false;

struct Fnv {
    uint64_t h = 1469598103934665603ULL;
    void b(uint8_t v) { h = (h ^ v) * 1099511628211ULL; }
    void bytes(const void *p, size_t n) { const uint8_t *q = (const uint8_t *)p; for (size_t i = 0; i < n; ++i) b(q[i]); }
    void u(uint32_t v) { for (int i = 0; i < 4; ++i) b((uint8_t)(v >> (8 * i))); }
};

void digest(Machine &m) {
    Fnv mem, io, cyc, reg;
    const Bus &x = m.bus;
    mem.bytes(x.workram, sizeof x.workram); mem.bytes(x.videoram, sizeof x.videoram); mem.bytes(x.colorram, sizeof x.colorram);
    mem.bytes(x.spriteram, sizeof x.spriteram); mem.bytes(x.sprite_coords, sizeof x.sprite_coords); mem.bytes(x.sound_regs, sizeof x.sound_regs);
    mem.bytes(x.latch, sizeof x.latch); mem.u(x.irq_vector); mem.u((uint32_t)x.watchdog_writes);
    io.u((uint32_t)x.io_log.size());
    for (const auto &e : x.io_log) { io.u((uint32_t)e.frame); io.u(e.address); io.u(e.value); }
    cyc.u(m.ticks_in_frame());
    reg.u(m.on_get_af()); reg.u(m.on_get_bc()); reg.u(m.on_get_de()); reg.u(m.on_get_hl()); reg.u(m.on_get_ix()); reg.u(m.on_get_iy()); reg.u(m.on_get_sp()); reg.u(m.on_get_pc());
    reg.u(m.on_get_ir()); reg.u(m.on_get_iff1()); reg.u(m.on_get_iff2()); reg.u(m.on_get_int_mode()); reg.u(m.on_is_halted()); reg.u(m.on_is_int_disabled());
    reg.u(m.get_alt_bc()); reg.u(m.get_alt_de()); reg.u(m.get_alt_hl());
    ++g_phase_frame;
    std::fprintf(g_out, "%s %lu mem %016llx io %016llx cycles %016llx regs %016llx\n", g_phase, g_phase_frame, (unsigned long long)mem.h, (unsigned long long)io.h, (unsigned long long)cyc.h, (unsigned long long)reg.h);
}
void frame_done(Machine &m) {
#ifdef PAC_NO_Z80
    if (g_wrong && std::strcmp(g_phase, "scripted") == 0 && g_phase_frame == 1000) m.on_tick(1);   // the deliberately wrong version
#endif
    digest(m);
}
void phase(const char *n) { g_phase = n; g_phase_frame = 0; }

struct Player {   // the scripted player of T01a (as in test_no_rom_code.cpp)
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
    if (argc < 6) { std::fprintf(stderr, "usage: test_native_machine PROG.BIN OUT.txt FRAMES COVERAGE_FRAMES REPLAY_FILE [WRONG]\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    g_out = std::fopen(argv[2], "w");
    if (!g_out) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 2; }
    const long frames = std::atol(argv[3]);
    const unsigned long cov = std::strtoul(argv[4], nullptr, 10);
    g_wrong = argc > 6;
#ifdef PAC_NO_Z80
    const Machine::RuleHook *table = rules::table();
    std::printf("machine: the register file only (no Z80 core), native routine table\n");
#else
    static Machine::RuleHook empty[0x10000];   // no hooks at all: every instruction is a ROM instruction run by the Z80 core (this table only selects the frame loop that restarts the cycle count every frame, the same as the native build's)
    const Machine::RuleHook *table = empty;
    std::printf("machine: the Z80 core running the original ROM\n");
#endif
    lockstep::g_force_rules = table;
    lockstep::g_frame_done = frame_done;
    {
        std::FILE *rf = std::fopen(argv[5], "r");
        if (!rf) { std::fprintf(stderr, "cannot read %s\n", argv[5]); return 2; }
        std::vector<std::array<unsigned, 3>> ent; unsigned dip = 0xC9; long nfr = 0; char line[256];
        while (std::fgets(line, sizeof line, rf)) {
            unsigned a, b, c;
            if (!std::strncmp(line, "dip ", 4)) dip = (unsigned)std::strtoul(line + 4, nullptr, 16);
            else if (!std::strncmp(line, "frames ", 7)) nfr = std::atol(line + 7);
            else if (line[0] != '#' && std::sscanf(line, "%u %x %x", &a, &b, &c) == 3) ent.push_back({a, b, c});
        }
        std::fclose(rf);
        phase("replay");
        static Machine R(prog);
        size_t k = 0; unsigned in0 = 0xFF, in1 = 0xFF;
        for (long fr = 0; fr < nfr; ++fr) {
            while (k < ent.size() && ent[k][0] <= (unsigned)fr) { in0 = ent[k][1]; in1 = ent[k][2]; ++k; }
            R.bus.in0 = (uint8_t)in0; R.bus.in1 = (uint8_t)in1; R.bus.dsw1 = (uint8_t)dip;
            probe::run_frame(R, [](uint16_t) {});
        }
        std::printf("replay: %ld frames\n", nfr);
    }
    {
        phase("scripted");
        Player pl;
        static Machine A(prog);
        for (long fr = 0; fr < frames; ++fr) { pl.joystick(fr); pl.inputs(A, fr); probe::run_frame(A, [](uint16_t) {}); }
        std::printf("scripted player: %ld frames\n", frames);
    }
    {
        static Machine PA(prog);
        phase("covA"); covplayer::run(PA, cov, nullptr);
        phase("covB"); covplayer::run_b(prog, cov, nullptr);
        phase("covC"); covplayer::run_c(prog, cov, nullptr);
        phase("covD"); covplayer::run_d(prog, cov / 5 ? cov / 5 : 1, nullptr);
        std::printf("coverage players A, B, C: %lu frames each, D: %lu frames\n", cov, cov / 5);
    }
    std::fclose(g_out);
    return 0;
}
