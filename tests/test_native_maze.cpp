// N18: the native maze drawing, dot placing / collecting, dot-bit reset and maze colours (src/game/maze.h, adapters in src/rules/rules.h namespace maze) against the ROM:
// 2419 (maze, loop head 241F), 2448 (put the dots on the screen, head 245C), 2487 (collect the dots, head 2498), 24C9 (reset the dot bits), 24D7 (maze colours, pass loop head 24E7).
// Same test structure as N18 (tests/test_native_maze.cpp).
// Usage: test_native_maze PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]
//  part 1  isolated: random whole calls (no interrupt): memory, I/O writes, every register and the exact cycle count equal
//  part 2  isolated from REAL entry states of the coverage players A, B, C, D
//  part 3  lockstep (tests/lockstep.h): original machine against (a) these routines alone and (b) all readable rules plus the helpers (N12 to N18)
//  part 3b CUT: the routine is started so that the frame ends inside it (the interrupt arrives in the middle), from real entry states and from random states; the original and the
//          native machine run 3 frames with the interrupt and are compared after each frame (all memory, I/O writes, registers, cycle count); the native one must really have cut
//  part 4  deliberately wrong versions must be caught (also in the cut test)
//  part 5  progress meter: ROM instruction addresses still executed in the coverage runs, before N18 and after
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"

namespace {

const uint16_t kEntries[9] = {0x2419, 0x241F, 0x2448, 0x245C, 0x2487, 0x2498, 0x24C9, 0x24D7, 0x24E7};
const int kN = 9;
int index_of(uint16_t pc) { for (int i = 0; i < kN; ++i) if (kEntries[i] == pc) return i; return -1; }

uint32_t g_rng = 20261016;
unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

Machine::RuleHook g_helpers_only[0x10000];   // only the N16 entries
Machine::RuleHook g_before_n12[0x10000];     // all readable rules except the N16 status pictures (the N12 and N16 helpers are in)
Machine::RuleHook g_after_n12[0x10000];      // all readable rules including the status pictures
unsigned long g_calls[16];

template <int I> void counted(Machine &m) { ++g_calls[I]; rules::table()[kEntries[I]](m); }
template <int I> void wrong_cycles(Machine &m) { rules::table()[kEntries[I]](m); m.on_tick(1); }

// deliberately wrong versions
void wrong_cut_hl(Machine &m) { rules::maze::maze_241f(m); if (m.on_get_pc() == 0x241F) m.on_set_hl((uint16_t)(m.on_get_hl() + 1)); }       // HL one too far after a cut
void wrong_cut_iy(Machine &m) { rules::maze::place_245c(m); if (m.on_get_pc() == 0x245C) m.on_set_iy((uint16_t)(m.on_get_iy() + 1)); }      // IY one too far after a cut
void wrong_cut_c(Machine &m) { rules::maze::collect_2498(m); if (m.on_get_pc() == 0x2498) m.on_set_bc((uint16_t)(m.on_get_bc() + 1)); }     // C one too high after a cut
void wrong_stack(Machine &m) { rules::maze::reset_24c9(m); m.bus.write((uint16_t)(m.on_get_sp() - 2), 0x55); }                              // the RST's return address wrong
void wrong_pass_hl(Machine &m) { rules::maze::colours_24e7(m); if (m.on_get_pc() == 0x0008) m.on_set_hl((uint16_t)(m.on_get_hl() + 1)); }  // HL one too far after a cut
void wrong_dot(Machine &m) { rules::maze::place_2448(m); m.bus.write(0x4000, 0x11); }                                                       // a stray tile

// ---- isolated runs --------------------------------------------------------------------------------------------------------------------------------
struct Setup { uint16_t entry; uint16_t ret; uint16_t sp0; int mode; uint16_t table_target; uint16_t exit = 0; };

bool done(Machine &m, const Setup &s) { return (uint16_t)m.on_get_pc() == (s.exit ? s.exit : s.ret) && (uint16_t)m.on_get_sp() == (uint16_t)(s.sp0 + 2); }

const char *compare(Machine &X, Machine &Y, uint32_t start, const Setup &s) {
    const char *w = lockstep::differ(X, Y, true);
    if (!w && X.on_get_wz() != Y.on_get_wz()) { w = "WZ"; if (std::getenv("NH_DEBUG")) std::fprintf(stderr, "  [%04X] WZ ROM %04X native %04X (pc %04X)\n", (unsigned)s.entry, (unsigned)X.on_get_wz(), (unsigned)Y.on_get_wz(), (unsigned)X.on_get_pc()); }
    if (!w && X.ticks_in_frame() != Y.ticks_in_frame()) w = "cycle count";
    (void)start;
    if (w && std::getenv("NH_DEBUG") && !std::strcmp(w, "CPU registers"))
        std::fprintf(stderr, "  [%04X] regs ROM af %04X bc %04X de %04X hl %04X sp %04X pc %04X ir %04X | native af %04X bc %04X de %04X hl %04X sp %04X pc %04X ir %04X\n",
                     (unsigned)s.entry, (unsigned)X.on_get_af(), (unsigned)X.on_get_bc(), (unsigned)X.on_get_de(), (unsigned)X.on_get_hl(), (unsigned)X.on_get_sp(), (unsigned)X.on_get_pc(), (unsigned)X.on_get_ir(),
                     (unsigned)Y.on_get_af(), (unsigned)Y.on_get_bc(), (unsigned)Y.on_get_de(), (unsigned)Y.on_get_hl(), (unsigned)Y.on_get_sp(), (unsigned)Y.on_get_pc(), (unsigned)Y.on_get_ir());
    return w;
}

// Runs the ROM routine on X and the table's version on Y from the state both machines are in now; returns nullptr when equal.
const char *run_pair(Machine &X, Machine &Y, const Machine::RuleHook *table, const Setup &s, uint32_t start) {
    X.rules = nullptr;
    Y.rules = table;
    X.set_ticks_in_frame(start);
    Y.set_ticks_in_frame(start);
    unsigned guard = 0;
    while (!done(X, s) && guard++ < 100000) X.on_step();
    guard = 0;
    while (!done(Y, s) && guard++ < 100000) {
        if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
        else Y.on_step();
    }
    return compare(X, Y, start, s);
}

// ---- the cut test ---------------------------------------------------------------------------------------------------------------------------------
unsigned long g_cut_cases = 0, g_cut_used = 0;   // cases run, cases in which the native routine really ran natively up to the end of the frame
// Like Machine::run_frame, but the tick counter is not reset (the routine is started late in the frame), then the interrupt.
void first_frame(Machine &m, const Machine::RuleHook *table) {
    m.bus.io_log.clear();
    for (;;) {
        const uint16_t pc = (uint16_t)m.on_get_pc();
        if (table && table[pc]) table[pc](m); else m.on_step();
        if (m.ticks_in_frame() >= Machine::cycles_per_frame) break;
    }
    ++m.frame;
    m.bus.set_frame(m.frame);
    m.deliver_vblank_interrupt();
}
const char *run_cut(Machine &X, Machine &Y, const Machine::RuleHook *table, const Setup &s, uint32_t start) {
    X.rules = nullptr;
    Y.rules = table;
    X.set_ticks_in_frame(start);
    Y.set_ticks_in_frame(start);
    const unsigned long cuts0 = rules::sliced_cuts;
    first_frame(X, nullptr);
    first_frame(Y, table);
    ++g_cut_cases;
    if (rules::sliced_cuts != cuts0) ++g_cut_used;
    for (int f = 0; f < 3; ++f) {
        const char *w = lockstep::differ(X, Y, true);
        if (!w && X.on_get_wz() != Y.on_get_wz()) w = "WZ";
        if (!w && X.ticks_in_frame() != Y.ticks_in_frame()) w = "cycle count";
        if (w) {
            if (std::getenv("NH_DEBUG")) {
                std::fprintf(stderr, "  [cut %04X] frame %d: %s; X pc %04X sp %04X ticks %u | Y pc %04X sp %04X ticks %u; start %u\n", s.entry, f, w, (unsigned)X.on_get_pc(), (unsigned)X.on_get_sp(), X.ticks_in_frame(), (unsigned)Y.on_get_pc(), (unsigned)Y.on_get_sp(), Y.ticks_in_frame(), start);
                if (!std::strcmp(w, "CPU registers")) std::fprintf(stderr, "    regs ROM af %04X bc %04X de %04X hl %04X ix %04X iy %04X ir %04X | native af %04X bc %04X de %04X hl %04X ix %04X iy %04X ir %04X\n", (unsigned)X.on_get_af(), (unsigned)X.on_get_bc(), (unsigned)X.on_get_de(), (unsigned)X.on_get_hl(), (unsigned)X.on_get_ix(), (unsigned)X.on_get_iy(), (unsigned)X.on_get_ir(), (unsigned)Y.on_get_af(), (unsigned)Y.on_get_bc(), (unsigned)Y.on_get_de(), (unsigned)Y.on_get_hl(), (unsigned)Y.on_get_ix(), (unsigned)Y.on_get_iy(), (unsigned)Y.on_get_ir());
                for (unsigned a = 0x4000; a < 0x5000; ++a) if (X.bus.read((uint16_t)a) != Y.bus.read((uint16_t)a)) { std::fprintf(stderr, "    first memory difference at %04X: ROM %02X native %02X\n", a, X.bus.read((uint16_t)a), Y.bus.read((uint16_t)a)); break; }
            }
            return w;
        }
        if (f == 2) break;
        X.run_frame();
        Y.run_frame();
    }
    return nullptr;
}

unsigned long g_all_native[16], g_cases_run[16];   // cases in which the hook never fell back to the ROM
unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, int which, long cases, unsigned long *skipped, bool cut_mode = false) {
    (void)skipped;
    unsigned long bad = 0;
    static Machine *pristine = nullptr;
    if (!pristine) pristine = new Machine(X);   // the first call gets fresh machines (see main)
    const uint16_t sp0 = 0x4FB0;
    for (long c = 0; c < cases; ++c) {
        X = *pristine;   // every case starts from the same fresh machine, so that one difference cannot spill into the next case
        Y = *pristine;
        uint8_t ram[0x3F0];
        for (auto &b : ram) b = (uint8_t)rnd();
        uint8_t video[0x400], colour[0x400];
        for (auto &b : video) b = (uint8_t)rnd();
        for (auto &b : colour) b = (uint8_t)rnd();
        Setup s{kEntries[which], 0, sp0, 0, 0};
        s.ret = (uint16_t)(0x0100 + rnd() % 0x3E00);
        if (cut_mode) { s.ret = 0x4D00; ram[0x100] = 0x18; ram[0x101] = 0xFE; }   // after the routine: JR $ in RAM, so that both machines spin in the same place
        uint16_t af = (uint16_t)rnd(), bc = (uint16_t)rnd(), de = (uint16_t)rnd(), hl = (uint16_t)rnd();
        uint16_t ix = (uint16_t)rnd(), iy = (uint16_t)rnd();
        switch (which) {
        case 0: case 1: hl = (uint16_t)(0x4000 + rnd() % 0x0400); bc = (uint16_t)(0x3435 + rnd() % 0x017B); break;
        case 2: case 3: case 4: case 5: {   // a position inside the 30 bytes of dot bits: IX, IY, B (bytes left), C (bits left) belong together
            const unsigned j = rnd() % 30, bit = rnd() % 8;
            hl = (uint16_t)(0x4000 + rnd() % 0x0400);
            ix = (uint16_t)(0x4E16 + j); iy = (uint16_t)(0x35B5 + 8 * j + bit);
            bc = (uint16_t)(((30 - j) << 8) | (8 - bit));
            if (rnd() % 3 == 0) { bc = (uint16_t)(((rnd() % 3 ? 1 : 30 - j) << 8) | 1); if ((bc >> 8) == 1) { ix = 0x4E33; iy = 0x35B5 + 239; } else { iy = (uint16_t)(0x35B5 + 8 * j + 7); } }
            de = 0;
            break;
        }
        case 7: bc = (uint16_t)((rnd() % 4 ? (rnd() % 4) : (rnd() % 256)) << 8 | (bc & 255)); break;
        case 8: {
            const unsigned pass = rnd() % 4;    // 0: the first pass (B = 80h, C = 4), then B = 0
            if (pass == 0) { bc = 0x8004; hl = 0x4440; } else { bc = (uint16_t)(4 - pass); hl = (uint16_t)(0x4440 + 0x80 + 0x100 * (pass - 1)); }
            de = (uint16_t)((de & 0xFF00) | (rnd() % 4));
            break;
        }
        default: break;
        }
        const uint16_t wz0 = (uint16_t)rnd();
        const uint8_t r0 = (uint8_t)rnd();
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            std::memcpy(m->bus.videoram, video, sizeof video);
            std::memcpy(m->bus.colorram, colour, sizeof colour);
            m->bus.write(sp0, s.ret & 255);
            m->bus.write((uint16_t)(sp0 + 1), s.ret >> 8);
            m->on_set_sp(sp0);
            m->on_set_pc(s.entry);
            m->on_set_af(af); m->on_set_bc(bc); m->on_set_de(de); m->on_set_hl(hl);
            m->on_set_ix(ix); m->on_set_iy(iy);
            m->on_set_wz(wz0);
            m->on_set_r(r0);
            m->on_set_is_halted(false);
            m->on_set_is_int_disabled(false);
            m->bus.io_log.clear();
        }
        const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (Machine::cycles_per_frame - 1 - (rnd() % 1500));
        const unsigned long fb_before = rules::fallbacks;
        const char *w = cut_mode ? run_cut(X, Y, table, s, Machine::cycles_per_frame - 1 - (rnd() % 30000)) : run_pair(X, Y, table, s, start);
        if (!cut_mode && table == g_helpers_only) { ++g_cases_run[which]; if (rules::fallbacks == fb_before) ++g_all_native[which]; }
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, native %u)\n", s.entry, c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
    }
    return bad;
}

// ---- real entry states ---------------------------------------------------------------------------------------------------------------------------
struct Real { unsigned long entries[16] = {0}, checked[16] = {0}, bad[16] = {0}; unsigned long stride[16] = {1, 4001, 1, 3001, 1, 3001, 1, 1, 3, 1, 1, 1, 1, 1, 1, 1}; };
Real g_real;
Machine *g_X = nullptr, *g_Y = nullptr;
const Machine::RuleHook *g_real_table = nullptr;

void real_probe(Machine &ref, uint16_t pc) {
    const int i = index_of(pc);
    if (i < 0) return;
    if (g_real.entries[i]++ % g_real.stride[i]) return;
    // the machine must be in the middle of a frame with room for the routine; the hook falls back near the end of the frame (also compared)
    Setup s{pc, 0, (uint16_t)ref.on_get_sp(), 0, 0};
    s.ret = (uint16_t)(ref.bus.read(s.sp0) | (ref.bus.read((uint16_t)(s.sp0 + 1)) << 8));
    {   // the cut test from the same real state: the frame ends inside the routine
        *g_X = ref;
        *g_Y = ref;
        const char *wc = run_cut(*g_X, *g_Y, g_real_table, s, Machine::cycles_per_frame - 1 - (rnd() % 30000));
        if (wc) { ++g_real.bad[i]; if (g_real.bad[i] <= 3) std::printf("  REAL-STATE CUT MISMATCH (routine %04X): %s\n", pc, wc); }
    }
    *g_X = ref;
    *g_Y = ref;
    ++g_real.checked[i];
    // the plain loop of the probe does not reset the tick counter per frame, so the position inside the frame is chosen here (mostly mid-frame, sometimes near its end)
    const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (Machine::cycles_per_frame - 1 - (rnd() % 1500));
    const char *w = run_pair(*g_X, *g_Y, g_real_table, s, start);
    if (w && ++g_real.bad[i] <= 3) std::printf("  REAL-STATE MISMATCH (routine %04X): %s\n", pc, w);
}

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
void seen_probe(Machine &, uint16_t pc) { if (pc < 0x4000) g_seen.insert(pc); }

unsigned meter(const std::vector<uint8_t> &prog, const Machine::RuleHook *table, unsigned long frames) {
    g_seen.clear();
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
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_native_maze PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    const long frames = std::atol(argv[2]);
    const unsigned long cov = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 20000;
    const long cases = argc > 4 ? std::atol(argv[4]) : 100000;

    for (int i = 0; i < 0x10000; ++i) { g_after_n12[i] = rules::table()[i]; g_before_n12[i] = rules::table()[i]; }
    for (uint16_t e : kEntries) { g_helpers_only[e] = rules::table()[e]; g_before_n12[e] = nullptr; }
    bool ok = true;
    for (uint16_t e : kEntries) if (!g_after_n12[e]) { std::printf("FAIL: no hook at %04X\n", e); ok = false; }
    static Machine::RuleHook counted_table[0x10000];
    for (int i = 0; i < 0x10000; ++i) counted_table[i] = nullptr;
    counted_table[0x2419] = counted<0>; counted_table[0x2448] = counted<2>; counted_table[0x2487] = counted<4>; counted_table[0x24C9] = counted<6>; counted_table[0x24D7] = counted<7>;

    static Machine X(prog), Y(prog);
    g_X = &X; g_Y = &Y;

    // ---- part 1
    unsigned long bad1[16], skipped = 0, total_bad1 = 0;
    for (int w = 0; w < kN; ++w) { bad1[w] = isolated(X, Y, g_helpers_only, w, w < 2 ? (cases / 20 ? cases / 20 : 1) : cases, &skipped); total_bad1 += bad1[w]; }
    std::printf("part 1: %ld random whole calls of each routine (all registers incl. IX and IY, R, WZ and memory random): differences",
                cases);
    for (int w = 0; w < kN; ++w) std::printf(" %04X %lu%s", kEntries[w], bad1[w], w + 1 < kN ? "," : "\n");
    std::printf("  cases in which the native version ran the whole routine (no fall back to the ROM near the end of the frame):");
    for (int w = 0; w < kN; ++w) std::printf(" %04X %lu of %lu%s", kEntries[w], g_all_native[w], g_cases_run[w], w + 1 < kN ? "," : "\n");
    if (total_bad1) ok = false;
    {   // part 3b: the cut test from random states: the frame ends inside the routine, 3 frames with the interrupt compared
        unsigned long badc[16];
        unsigned long tot = 0;
        g_cut_cases = g_cut_used = 0;
        for (int w = 0; w < kN; ++w) { badc[w] = isolated(X, Y, g_helpers_only, w, w < 2 ? (cases / 40 ? cases / 40 : 1) : (cases / 4 ? cases / 4 : 1), &skipped, true); tot += badc[w]; }
        std::printf("part 3b: cut test from random states, %ld cases of each routine, the frame ends at a random place inside it, 3 frames compared (memory, I/O writes, registers, cycles): differences", cases / 4 ? cases / 4 : 1);
        for (int w = 0; w < kN; ++w) std::printf(" %04X %lu%s", kEntries[w], badc[w], w + 1 < kN ? "," : "\n");
        std::printf("  cases in which the native routine really ran up to the end of the frame and left the rest to the ROM (a cut): %lu of %lu\n", g_cut_used, g_cut_cases);
        if (tot || g_cut_used == 0) ok = false;
        if (std::getenv("NH_STOP_3B")) return ok ? 0 : 1;   // (debugging aid: only parts 1 and 3b)
    }

    // ---- part 2
    g_real_table = g_helpers_only;
    lockstep::g_pc_probe = real_probe;
    {
        static Machine m(prog);
        covplayer::run(m, cov, nullptr);
    }
    covplayer::run_b(prog, cov, nullptr);
    covplayer::run_c(prog, cov, nullptr);
    covplayer::run_d(prog, cov / 5 ? cov / 5 : 1, nullptr);
    lockstep::g_pc_probe = nullptr;
    unsigned long total_bad2 = 0;
    std::printf("part 2: real entry states from the coverage players A, B, C, D (%lu frames each, D a fifth): entries / compared / differences\n", cov);
    for (int i = 0; i < kN; ++i) {
        std::printf("  %04X: %lu / %lu / %lu%s\n", kEntries[i], g_real.entries[i], g_real.checked[i], g_real.bad[i], g_real.entries[i] == 0 ? "   (never entered in these runs)" : "");
        total_bad2 += g_real.bad[i];
        if (!g_real.checked[i]) std::printf("  note: %04X was never entered with the real game states in these runs (proved by the random cases and the lockstep only)\n", kEntries[i]);
    }
    if (total_bad2) ok = false;

    // ---- part 3
    const unsigned long cuts_before_lockstep = rules::sliced_cuts;
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("A: native starts 2419, 2448, 2487, 24C9, 24D7 only (calls counted)", counted_table, false);
    h.add_variant("B: all readable rules + N12 to N18", g_after_n12, false);
    static Machine::RuleHook c_table[0x10000];
    for (int i = 0; i < 0x10000; ++i) c_table[i] = nullptr;
    c_table[0x241F] = counted<1>; c_table[0x245C] = counted<3>; c_table[0x2498] = counted<5>; c_table[0x24E7] = counted<8>;
    h.add_variant("C: only the loop heads 241F, 245C, 2498, 24E7 (the starts run in the ROM), calls counted", c_table, false);
    for (uint16_t e : kEntries) h.watch_entry(e);
    lockstep::g_harness = &h;
    {
        Player pl;
        static Machine A(prog);
        for (long fr = 0; fr < frames && !h.all_stopped(); ++fr) {
            pl.joystick(fr);
            pl.inputs(A, fr);
            probe::run_frame(A, [](uint16_t) {});
        }
    }
    const long scripted_frames = (long)h.frames();
    {
        static Machine PA(prog);
        covplayer::run(PA, cov, nullptr);
        covplayer::run_b(prog, cov, nullptr);
        covplayer::run_c(prog, cov, nullptr);
        covplayer::run_d(prog, cov / 5 ? cov / 5 : 1, nullptr);
    }
    lockstep::g_harness = nullptr;
    std::printf("part 3: native cuts in the lockstep runs (a routine run up to the end of the frame, the ROM does the rest, all variants together): %lu\n", rules::sliced_cuts - cuts_before_lockstep);
    std::printf("part 3: lockstep, scripted player %ld frames, then the coverage players A, B, C (%lu frames each) and D (a fifth): %llu frames compared\n", frames, cov, (unsigned long long)h.frames());
    h.report(stdout);
    // the routines marked exact are not contained in another replaced routine, so the calls are compared one to one;
    // (and entered by JR from it): the native versions do not call each other, so those counts differ by design.
    bool calls_ok = true;
    for (int i = 0; i < kN; ++i) {
        const bool exact = i == 0 || i == 2 || i == 4 || i == 6 || i == 7;   // the starts are entered once per call; the loop heads are entered once per round in the ROM, only after a cut here
        const unsigned long ref_n = (unsigned long)h.entries(kEntries[i]);
        std::printf("  %04X: original machine entered it %lu times; native version (A or C) ran %lu times%s\n", kEntries[i], ref_n, g_calls[i], exact ? (ref_n == g_calls[i] ? "  (equal)" : "  (NOT EQUAL)") : "  (not compared: see above)");
        if (exact && ref_n != g_calls[i]) calls_ok = false;
        if (g_calls[i] == 0 && ref_n != 0) calls_ok = false;
    }
    const bool lock_ok = h.all_identical() && calls_ok;
    std::printf("  maze, dots and colours in lockstep: %s (scripted part %ld frames)\n", lock_ok ? "identical on every frame, all variants; the native versions ran in every routine" : "DIFFERENT or counts wrong", scripted_frames);
    if (!lock_ok) ok = false;

    // ---- part 4
    struct W { const char *name; int which; Machine::RuleHook fn; } wrongs[] = {
        {"2419 one cycle too slow", 0, wrong_cycles<0>}, {"241F HL one too far after a cut", 1, wrong_cut_hl}, {"2448 a stray tile written", 2, wrong_dot},
        {"245C IY one too far after a cut", 3, wrong_cut_iy}, {"2487 one cycle too slow", 4, wrong_cycles<4>}, {"2498 C one too high after a cut", 5, wrong_cut_c},
        {"24C9 the RST's return address wrong", 6, wrong_stack}, {"24D7 one cycle too slow", 7, wrong_cycles<7>}, {"24E7 HL one too far after a cut", 8, wrong_pass_hl}};
    static Machine::RuleHook wt[0x10000];
    for (const W &w : wrongs) {
        for (int i = 0; i < 0x10000; ++i) wt[i] = nullptr;
        wt[kEntries[w.which]] = w.fn;
        const unsigned long b = isolated(X, Y, wt, w.which, 3000, &skipped, true);   // in the cut test
        std::printf("part 4: wrong version \"%s\": %s (%lu of 3000 isolated cases differ)\n", w.name, b ? "caught" : "NOT caught", b);
        if (!b) ok = false;
    }
    {
        lockstep::Harness hw(prog);
        static Machine::RuleHook t[0x10000];
        for (int i = 0; i < 0x10000; ++i) t[i] = nullptr;
        t[0x2419] = wrong_cycles<0>;
        hw.add_variant("W: the maze drawing 2419 one cycle too slow", t, false);
        lockstep::g_harness = &hw;
        Player pl;
        static Machine WM(prog);
        for (long fr = 0; fr < 3000 && !hw.all_stopped(); ++fr) { pl.joystick(fr); pl.inputs(WM, fr); probe::run_frame(WM, [](uint16_t) {}); }
        lockstep::g_harness = nullptr;
        const long c = hw.first_difference_frame(0);
        std::printf("part 4: lockstep with the maze drawing (one cycle too slow): %s%s\n", c >= 0 ? "caught at frame " : "NOT caught", c >= 0 ? std::to_string(c).c_str() : "");
        if (c < 0) ok = false;
    }

    // ---- part 5
    const unsigned long mf = cov < 20000 ? cov : 20000;
    const unsigned m_rom = meter(prog, nullptr, mf);
    const unsigned m_before = meter(prog, g_before_n12, mf);
    const unsigned m_after = meter(prog, g_after_n12, mf);
    std::printf("part 5: progress meter (coverage players A, B, C of %lu frames and D of a fifth; distinct ROM instruction addresses executed): original ROM %u; readable rules + N12 to N18 (before N18) %u; with the N18 maze routines %u\n",
                mf, m_rom, m_before, m_after);
    if (!(m_after < m_before)) { std::printf("FAIL: the progress meter did not go down\n"); ok = false; }

    std::printf("%s test_native_maze\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
