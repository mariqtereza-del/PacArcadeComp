// N49c: every instruction of the native instruction list (src/game/attract_prog.h) against the Z80 running the same instruction of the ROM, from random states.
// For each listed instruction (address in the ROM), N random states (all registers incl. alternates, IX, IY, R, WZ, flags, memory) are given to two machines; one
// steps the ROM instruction with the Z80 core, the other runs the native code for that one instruction. Registers, flags, WZ, R, memory, I/O writes and cycles must be equal.
// Usage: test_native_ops PROG.BIN [CASES_PER_INSTRUCTION]     (a ROM instruction that jumps to a random address is fine: only the machine state is compared)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "board/machine.h"
#include "rules/rules.h"

static uint32_t g_rng = 4915;
static unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

static void native_one(Machine &m) {   // as rules::attract::hook_budget, for one instruction
    rules::helpers::BusMem mem{m};
    game::spp::Z<rules::helpers::BusMem> z(mem);
    const game::Regs r = rules::helpers::load(m);
    z.a = r.a; z.f = r.f; z.b = r.b; z.c = r.c; z.d = r.d; z.e = r.e; z.h = r.h; z.l = r.l;
    z.ix = (uint16_t)m.on_get_ix(); z.iy = (uint16_t)m.on_get_iy(); z.wz = (uint16_t)m.on_get_wz();
    uint16_t sp = (uint16_t)m.on_get_sp();
    z.abc = (uint16_t)m.get_alt_bc(); z.ade = (uint16_t)m.get_alt_de(); z.ahl = (uint16_t)m.get_alt_hl();
    const game::hnd::Out o = game::att::run(z, sp, (uint16_t)m.on_get_pc(), 1, true);
    game::Regs q = r;
    q.a = z.a; q.f = z.f; q.b = z.b; q.c = z.c; q.d = z.d; q.e = z.e; q.h = z.h; q.l = z.l;
    rules::helpers::store(m, q);
    m.on_set_ix(z.ix); m.on_set_iy(z.iy); m.on_set_sp(sp);
    rules::continue_at(m, o.next, z.cost.cycles, z.cost.fetches);
    m.on_set_wz(z.wz);
    m.set_alt_bc(z.abc); m.set_alt_de(z.ade); m.set_alt_hl(z.ahl);
    if (o.iff >= 0) { m.on_set_iff1(o.iff != 0); m.on_set_iff2(o.iff != 0); }
    if (o.im >= 0) m.on_set_int_mode((unsigned)o.im);
    if (o.ireg >= 0) m.on_set_i((uint8_t)o.ireg);
    if (o.port0 >= 0) m.on_output(0, (uint8_t)o.port0);
    if (o.halted) m.on_set_is_halted(true);
}

static unsigned g_t0x = 0, g_t0y = 0;
static const char *differ(Machine &a, Machine &b) {
    if (a.on_get_af() != b.on_get_af()) return "AF";
    if (a.on_get_bc() != b.on_get_bc()) return "BC";
    if (a.on_get_de() != b.on_get_de()) return "DE";
    if (a.on_get_hl() != b.on_get_hl()) return "HL";
    if (a.on_get_ix() != b.on_get_ix()) return "IX";
    if (a.on_get_iy() != b.on_get_iy()) return "IY";
    if (a.on_get_sp() != b.on_get_sp()) return "SP";
    if (a.on_get_pc() != b.on_get_pc()) return "PC";
    if (a.get_alt_bc() != b.get_alt_bc() || a.get_alt_de() != b.get_alt_de() || a.get_alt_hl() != b.get_alt_hl()) return "alternate registers";
    if (a.on_get_ir() != b.on_get_ir()) return "R/I";
    if (a.on_get_wz() != b.on_get_wz()) return "WZ";
    if (a.on_get_iff1() != b.on_get_iff1() || a.on_get_iff2() != b.on_get_iff2()) return "IFF";
    if (a.ticks_in_frame() - g_t0x != b.ticks_in_frame() - g_t0y) return "cycles";
    if (std::memcmp(a.bus.workram, b.bus.workram, sizeof a.bus.workram)) return "work RAM";
    if (std::memcmp(a.bus.videoram, b.bus.videoram, sizeof a.bus.videoram)) return "video RAM";
    if (std::memcmp(a.bus.colorram, b.bus.colorram, sizeof a.bus.colorram)) return "colour RAM";
    if (a.bus.io_log.size() != b.bus.io_log.size()) return "I/O writes";
    return nullptr;
}

int main(int argc, char **argv) {
    if (argc < 2) { std::printf("usage: test_native_ops PROG.BIN [CASES]\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::printf("cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    const int cases = argc > 2 ? std::atoi(argv[2]) : 200;
    static Machine X(prog), Y(prog);
    std::map<std::string, int> bad_by_what;
    int bad_instr = 0;
    long total = 0, bad = 0;
    for (int i = 0; i < game::att::kProgN; ++i) {
        const auto &in = game::att::kProg[i];
        int bad_here = 0; const char *first = nullptr; unsigned first_case = 0;
        for (int c = 0; c < cases; ++c) {
            uint8_t ram[sizeof X.bus.workram], vid[sizeof X.bus.videoram], col[sizeof X.bus.colorram];
            for (auto &v : ram) v = (uint8_t)rnd();
            for (auto &v : vid) v = (uint8_t)rnd();
            for (auto &v : col) v = (uint8_t)rnd();
            const unsigned af = rnd() & 0xFFFF, bc = rnd() & 0xFFFF, de = rnd() & 0xFFFF, hl = rnd() & 0xFFFF, ix = rnd() & 0xFFFF, iy = rnd() & 0xFFFF;
            const unsigned abc = rnd() & 0xFFFF, ade = rnd() & 0xFFFF, ahl = rnd() & 0xFFFF;
            const uint16_t sp = (uint16_t)(0x4C20 + (rnd() % 0x3A0));
            const uint16_t wz = (uint16_t)rnd(); const uint8_t r0 = (uint8_t)rnd();
            // memory the instruction may point at: usually inside the work RAM / video RAM, sometimes anywhere
            const unsigned mode = rnd() % 4;
            auto addr = [&](unsigned v) { return mode == 0 ? v & 0xFFFF : (mode == 1 ? 0x4C00 + (v % 0x3F0) : (mode == 2 ? 0x4000 + (v % 0x800) : 0x4C00 + (v % 0x3F0))); };
            for (Machine *m : {&X, &Y}) {
                std::memcpy(m->bus.workram, ram, sizeof ram); std::memcpy(m->bus.videoram, vid, sizeof vid); std::memcpy(m->bus.colorram, col, sizeof col);
                m->on_set_af(af); m->on_set_bc((mode == 1 ? addr(bc) : bc)); m->on_set_de((mode == 1 ? addr(de) : de)); m->on_set_hl((mode ? addr(hl) : hl));
                m->on_set_ix((uint16_t)(mode == 3 ? addr(ix) - 0x20 : ix)); m->on_set_iy((uint16_t)(mode == 3 ? addr(iy) - 0x20 : iy));
                m->set_alt_bc((uint16_t)abc); m->set_alt_de((uint16_t)ade); m->set_alt_hl((uint16_t)ahl);
                m->on_set_sp(sp); m->on_set_wz(wz); m->on_set_r(r0); m->on_set_pc(in.addr);
                m->on_set_is_halted(false); m->on_set_is_int_disabled(false);
                m->on_set_iff1(false); m->on_set_iff2(false);
                m->bus.io_log.clear();
                m->on_tick(0);
            }
            const unsigned t0 = X.ticks_in_frame(); g_t0x = t0; g_t0y = Y.ticks_in_frame();
            X.on_step();
            if (prog[in.addr] == 0xDD || prog[in.addr] == 0xFD) X.on_step();   // the core takes the prefix byte as a step of its own
            native_one(Y);
            ++total;
            const char *w = differ(X, Y);
            if (w && getenv("OPSDBG") && in.addr==strtoul(getenv("OPSDBG"),0,16) && c<3) std::printf("DBG %04X %s: X cyc %u pc %04X sp %04X af %04X | Y cyc %u pc %04X sp %04X af %04X (start %u)\n", in.addr, w, X.ticks_in_frame(), (unsigned)X.on_get_pc(), (unsigned)X.on_get_sp(), (unsigned)X.on_get_af(), Y.ticks_in_frame(), (unsigned)Y.on_get_pc(), (unsigned)Y.on_get_sp(), (unsigned)Y.on_get_af(), t0);
            if (w) { ++bad; ++bad_here; if (!first) { first = w; first_case = (unsigned)c; } }
        }
        if (bad_here) { ++bad_instr; ++bad_by_what[std::string(first)]; if (bad_instr <= 40) std::printf("  MISMATCH %04X (%d of %d cases differ; first: %s, case %u)\n", in.addr, bad_here, cases, first, first_case); }
    }
    std::printf("%d instructions x %d random states = %ld cases; instructions with a difference: %d (%ld cases)\n", game::att::kProgN, cases, total, bad_instr, bad);
    std::printf("%s test_native_ops\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
