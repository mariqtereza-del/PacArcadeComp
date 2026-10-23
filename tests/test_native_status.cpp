// N16: the native score, life, credit and fruit pictures (src/game/status.h, adapters in src/rules/rules.h namespace status) against the ROM routines
// 2AE0 (redraw the score area), 2B33 (extra life), 2B4A (the life pictures), 2B6A (clear the life area), 2B7E / 2B80 / 2B8F (2x2 tile blocks), 2BA1 (credit display),
// 2BCD (fill a rectangle from the data after the CALL), 2BEA (the level fruit row).
// Usage: test_native_status PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]
//  part 1  isolated: random whole calls (all registers, R, WZ and the memory the routine reads random): memory, I/O writes, every register incl. IX and the exact cycle count equal
//  part 2  isolated from REAL entry states of the coverage players A, B, C, D
//  part 3  lockstep (tests/lockstep.h): original machine against (a) these routines alone and (b) all readable rules plus the helpers (N12 to N16)
//  part 4  deliberately wrong versions must be caught
//  part 5  progress meter: ROM instruction addresses still executed in the coverage runs, before N16 and after
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"

namespace {

const uint16_t kEntries[10] = {0x2AE0, 0x2B33, 0x2B4A, 0x2B6A, 0x2B7E, 0x2B80, 0x2B8F, 0x2BA1, 0x2BCD, 0x2BEA};
const int kN = 10;
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
void wrong_ix(Machine &m) { rules::status::credits_2ba1(m); m.on_set_ix((uint16_t)(m.on_get_ix() + 1)); }
void wrong_colour(Machine &m) { rules::status::block_2b8f(m); m.bus.write(0x4400, (uint8_t)(m.bus.read(0x4400) ^ 1)); }   // a byte wrong
void wrong_stack(Machine &m) { rules::status::fill_rect_2bcd(m); m.bus.write((uint16_t)(m.on_get_sp() - 6), 0x55); }      // a stack leftover wrong
void wrong_flags(Machine &m) { rules::status::extra_life_2b33(m); m.on_set_f((uint8_t)(m.on_get_f() ^ 0x08)); }          // an undocumented flag bit wrong

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

unsigned long g_all_native[16], g_cases_run[16];   // cases in which the hook never fell back to the ROM
unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, int which, long cases, unsigned long *skipped) {
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
        auto W = [&](unsigned a) -> uint8_t & { return ram[a - 0x4C00]; };
        Setup s{kEntries[which], 0, sp0, 0, 0};
        s.ret = (uint16_t)(0x0100 + rnd() % 0x3E00);
        uint16_t af = (uint16_t)rnd(), bc = (uint16_t)rnd(), de = (uint16_t)rnd(), hl = (uint16_t)rnd();
        for (unsigned a = 0x4E80; a < 0x4E90; ++a) { const unsigned k = rnd() % 4; W(a) = k == 0 ? 0 : k == 1 ? (uint8_t)(rnd() & 0x0F) : k == 2 ? (uint8_t)(rnd() & 0xF0) : (uint8_t)rnd(); }
        W(0x4E09) = (uint8_t)(rnd() % 2 ? 0 : 1 + rnd() % 3);
        W(0x4E00) = (uint8_t)(rnd() % 4 == 0 ? 1 : rnd() % 4);
        W(0x4E15) = (uint8_t)(rnd() % 10 ? rnd() % 8 : rnd() % 256);
        W(0x4E14) = (uint8_t)rnd();
        W(0x4E70) = (uint8_t)(rnd() % 3);
        W(0x4E6E) = (uint8_t)(rnd() % 6 == 0 ? 0xFF : (rnd() % 3 ? ((rnd() % 4) << 4 | (rnd() % 10)) : rnd()));
        W(0x4E13) = (uint8_t)(rnd() % 40 == 0 ? 0xFF : (rnd() % 3 ? rnd() % 9 : rnd() % 30));
        switch (which) {
        case 1: de = (uint16_t)(0x4C00 + rnd() % 0x2E0); if (rnd() % 2) W(de + 1) &= 0xFE; else W(de + 1) |= 1; break;
        case 2: bc = (uint16_t)(((rnd() % 4 ? rnd() % 9 : rnd() % 256) << 8) | (bc & 255)); break;
        case 4: case 5: case 6: hl = (uint16_t)(0x4000 + rnd() % 0x0780); break;
        case 8: {
            const uint16_t at = (uint16_t)(0x4D00 + rnd() % 0x80);
            const uint16_t dst = (uint16_t)(0x4000 + rnd() % 0x0700);
            W(at) = (uint8_t)dst; W(at + 1) = (uint8_t)(dst >> 8); W(at + 2) = (uint8_t)rnd();
            W(at + 3) = (uint8_t)(rnd() % 30 ? 1 + rnd() % 12 : 0); W(at + 4) = (uint8_t)(1 + rnd() % 4);
            s.ret = at; s.exit = (uint16_t)(at + 5);
            break; }
        default: break;
        }
        const uint16_t ix = (uint16_t)rnd(), iy = (uint16_t)rnd(), wz0 = (uint16_t)rnd();
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
        const char *w = run_pair(X, Y, table, s, start);
        if (table == g_helpers_only) { ++g_cases_run[which]; if (rules::fallbacks == fb_before) ++g_all_native[which]; }
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, native %u)\n", s.entry, c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
    }
    return bad;
}

// ---- real entry states ---------------------------------------------------------------------------------------------------------------------------
struct Real { unsigned long entries[16] = {0}, checked[16] = {0}, bad[16] = {0}; unsigned long stride[16] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}; };
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
    if (pc == 0x2BCD) s.exit = (uint16_t)(s.ret + 5);
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
        std::fprintf(stderr, "usage: test_native_status PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]\n");
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
    for (int i = 0; i < 0x10000; ++i) counted_table[i] = g_helpers_only[i];
    for (int i : {0, 1, 3, 7, 9}) counted_table[kEntries[i]] = i == 0 ? counted<0> : i == 1 ? counted<1> : i == 3 ? counted<3> : i == 7 ? counted<7> : counted<9>;
    for (int i : {2, 4, 5, 6, 8}) counted_table[kEntries[i]] = nullptr;   // inside the others: variants C and D run them (the parents are in the ROM there)

    static Machine X(prog), Y(prog);
    g_X = &X; g_Y = &Y;

    // ---- part 1
    unsigned long bad1[16], skipped = 0, total_bad1 = 0;
    for (int w = 0; w < kN; ++w) { bad1[w] = isolated(X, Y, g_helpers_only, w, cases, &skipped); total_bad1 += bad1[w]; }
    std::printf("part 1: %ld random whole calls of each routine (all registers incl. IX and IY, R, WZ and memory random): differences",
                cases);
    for (int w = 0; w < kN; ++w) std::printf(" %04X %lu%s", kEntries[w], bad1[w], w + 1 < kN ? "," : "\n");
    std::printf("  cases in which the native version ran the whole routine (no fall back to the ROM near the end of the frame):");
    for (int w = 0; w < kN; ++w) std::printf(" %04X %lu of %lu%s", kEntries[w], g_all_native[w], g_cases_run[w], w + 1 < kN ? "," : "\n");
    if (total_bad1) ok = false;

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
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("A: native status pictures only (calls counted)", counted_table, false);
    h.add_variant("B: all readable rules + N12 to N16 helpers", g_after_n12, false);
    static Machine::RuleHook c_table[0x10000], d_table[0x10000];
    for (int i = 0; i < 0x10000; ++i) c_table[i] = d_table[i] = nullptr;
    c_table[0x2B7E] = counted<4>; c_table[0x2B80] = counted<5>; c_table[0x2B8F] = counted<6>; c_table[0x2BCD] = counted<8>;
    d_table[0x2B4A] = counted<2>;
    h.add_variant("C: only the tile blocks 2B7E, 2B80, 2B8F and the rectangle fill 2BCD (inside the others), calls counted", c_table, false);
    h.add_variant("D: only the life pictures 2B4A (inside 2B33 and 2B6A), calls counted", d_table, false);
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
    std::printf("part 3: lockstep, scripted player %ld frames, then the coverage players A, B, C (%lu frames each) and D (a fifth): %llu frames compared\n", frames, cov, (unsigned long long)h.frames());
    h.report(stdout);
    // the routines marked exact are not contained in another replaced routine, so the calls are compared one to one;
    // (and entered by JR from it): the native versions do not call each other, so those counts differ by design.
    bool calls_ok = true;
    for (int i = 0; i < kN; ++i) {
        const bool exact = true;   // variant A runs the routines that are not inside another one, C and D the ones that are
        const unsigned long ref_n = (unsigned long)h.entries(kEntries[i]) - (i == 5 ? (unsigned long)h.entries(0x2B7E) : 0);   // 2B7E runs on into 2B80: those are not separate entries
        std::printf("  %04X: original machine entered it %lu times; native version (A, C or D) ran %lu times%s\n", kEntries[i], ref_n, g_calls[i], exact ? (ref_n == g_calls[i] ? "  (equal)" : "  (NOT EQUAL)") : "  (not compared: see above)");
        if (exact && ref_n != g_calls[i]) calls_ok = false;
        if (exact && g_calls[i] == 0 && ref_n != 0) calls_ok = false;
    }
    const bool lock_ok = h.all_identical() && calls_ok;
    std::printf("  status pictures in lockstep: %s (scripted part %ld frames)\n", lock_ok ? "identical on every frame, all variants; the native versions ran in every routine" : "DIFFERENT or counts wrong", scripted_frames);
    if (!lock_ok) ok = false;

    // ---- part 4
    struct W { const char *name; int which; Machine::RuleHook fn; } wrongs[] = {
        {"2AE0 one cycle too slow", 0, wrong_cycles<0>}, {"2B33 an undocumented flag bit wrong", 1, wrong_flags}, {"2B4A one cycle too slow", 2, wrong_cycles<2>},
        {"2B6A one cycle too slow", 3, wrong_cycles<3>}, {"2B7E one cycle too slow", 4, wrong_cycles<4>}, {"2B80 one cycle too slow", 5, wrong_cycles<5>},
        {"2B8F a byte wrong", 6, wrong_colour}, {"2BA1 IX one too far", 7, wrong_ix}, {"2BCD a stack leftover wrong", 8, wrong_stack}, {"2BEA one cycle too slow", 9, wrong_cycles<9>}};
    static Machine::RuleHook wt[0x10000];
    for (const W &w : wrongs) {
        for (int i = 0; i < 0x10000; ++i) wt[i] = nullptr;
        wt[kEntries[w.which]] = w.fn;
        const unsigned long b = isolated(X, Y, wt, w.which, 3000, &skipped);
        std::printf("part 4: wrong version \"%s\": %s (%lu of 3000 isolated cases differ)\n", w.name, b ? "caught" : "NOT caught", b);
        if (!b) ok = false;
    }
    {
        lockstep::Harness hw(prog);
        static Machine::RuleHook t[0x10000];
        for (int i = 0; i < 0x10000; ++i) t[i] = nullptr;
        t[0x2B8F] = wrong_cycles<6>;
        hw.add_variant("W: 2B8F one cycle too slow", t, false);
        lockstep::g_harness = &hw;
        Player pl;
        static Machine WM(prog);
        for (long fr = 0; fr < 3000 && !hw.all_stopped(); ++fr) { pl.joystick(fr); pl.inputs(WM, fr); probe::run_frame(WM, [](uint16_t) {}); }
        lockstep::g_harness = nullptr;
        const long c = hw.first_difference_frame(0);
        std::printf("part 4: lockstep with the wrong 2B8F (one cycle too slow): %s%s\n", c >= 0 ? "caught at frame " : "NOT caught", c >= 0 ? std::to_string(c).c_str() : "");
        if (c < 0) ok = false;
    }

    // ---- part 5
    const unsigned long mf = cov < 20000 ? cov : 20000;
    const unsigned m_rom = meter(prog, nullptr, mf);
    const unsigned m_before = meter(prog, g_before_n12, mf);
    const unsigned m_after = meter(prog, g_after_n12, mf);
    std::printf("part 5: progress meter (coverage players A, B, C of %lu frames and D of a fifth; distinct ROM instruction addresses executed): original ROM %u; readable rules + N12 to N15 (before N16) %u; with the N16 pictures %u\n",
                mf, m_rom, m_before, m_after);
    if (!(m_after < m_before)) { std::printf("FAIL: the progress meter did not go down\n"); ok = false; }

    std::printf("%s test_native_status\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
