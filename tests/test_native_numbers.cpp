// N14: the native number helpers (src/game/numbers.h, adapters in src/rules/rules.h namespace numbers) against the ROM routines
// 2ACE (BCD digit to tile), 2ABE (a row of BCD bytes to tiles), 2AAF (the player's score to the screen), 2B0B (address of the player's score), 02DF (coin counter, BCD add).
// Usage: test_native_numbers PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]
//  part 1  isolated: random whole calls (all registers, R, WZ and memory random): memory, I/O writes, every register and the exact cycle count equal
//  part 2  isolated from REAL entry states of the coverage players A, B, C, D
//  part 3  lockstep (tests/lockstep.h): original machine against (a) the number helpers alone and (b) all readable rules plus the helpers (N12, N13, N14)
//  part 4  deliberately wrong versions must be caught
//  part 5  progress meter: ROM instruction addresses still executed in the coverage runs, before N14 and after
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"

namespace {

const uint16_t kEntries[5] = {0x2ACE, 0x2ABE, 0x2AAF, 0x2B0B, 0x02DF};
const int kN = 5;
int index_of(uint16_t pc) { for (int i = 0; i < kN; ++i) if (kEntries[i] == pc) return i; return -1; }

uint32_t g_rng = 20261016;
unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

Machine::RuleHook g_helpers_only[0x10000];   // only the N14 entries
Machine::RuleHook g_before_n12[0x10000];     // all readable rules except the N14 number helpers (the N12 and N14 helpers are in)
Machine::RuleHook g_after_n12[0x10000];      // all readable rules including the number helpers
unsigned long g_calls[6];
unsigned long g_calls_c = 0;   // variant C: 2ACE alone (it is only ever entered from 2ABE, which variant A replaces as a whole)
void counted_c(Machine &m) { ++g_calls_c; rules::table()[0x2ACE](m); }

template <int I> void counted(Machine &m) { ++g_calls[I]; rules::table()[kEntries[I]](m); }

// deliberately wrong versions
void wrong_2ace_slow(Machine &m) { rules::numbers::digit_tile_2ace(m); m.on_tick(1); }
void wrong_2abe_de(Machine &m) { rules::numbers::digit_row_2abe(m); m.on_set_de((uint16_t)(m.on_get_de() + 1)); }
void wrong_2aaf_stack(Machine &m) { rules::numbers::score_draw_2aaf(m); m.bus.write((uint16_t)(m.on_get_sp() - 2), 0x55); } // a stack leftover wrong
void wrong_2b0b_ptr(Machine &m) { rules::numbers::score_pointer_2b0b(m); m.on_set_hl((uint16_t)(m.on_get_hl() + 1)); }
void wrong_02df_limit(Machine &m) { rules::numbers::coin_counter_02df(m); if (m.bus.read(0x4E6E) == 0x99) m.bus.write(0x4E6E, 0x98); }

// ---- isolated runs --------------------------------------------------------------------------------------------------------------------------------
struct Setup { uint16_t entry; uint16_t ret; uint16_t sp0; int mode; uint16_t table_target; };

bool done(Machine &m, const Setup &s) { return (uint16_t)m.on_get_pc() == s.ret && (uint16_t)m.on_get_sp() == (uint16_t)(s.sp0 + 2); }

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
        // the digits: many zero nibbles so that the blanking of leading zeros is exercised
        for (unsigned a = 0x4E80; a < 0x4E90; ++a) { const unsigned k = rnd() % 4; W(a) = k == 0 ? 0 : k == 1 ? (uint8_t)(rnd() & 0x0F) : k == 2 ? (uint8_t)(rnd() & 0xF0) : (uint8_t)rnd(); }
        W(0x4E09) = (uint8_t)(rnd() % 2 ? 0 : 1 + rnd() % 3);
        if (which == 0) { hl = (uint16_t)(0x4000 + rnd() % 0xB00); bc = (uint16_t)((bc & 0xFF00) | (rnd() % 3 ? rnd() % 5 : 0)); }
        if (which == 1) {
            hl = (uint16_t)(0x4100 + rnd() % 0xA00);
            de = (uint16_t)(0x4E80 + rnd() % 16);
            bc = (uint16_t)(((rnd() % 20 ? 1 + rnd() % 8 : 0) << 8) | (rnd() % 3 ? rnd() % 6 : 0));
        }
        if (which == 2) de = (uint16_t)(0x4E82 + rnd() % 8);
        if (which == 4) {
            W(0x4E6B) = (uint8_t)(rnd() % 4); W(0x4E6C) = (rnd() % 2) ? (uint8_t)(W(0x4E6B) - 1) : (uint8_t)rnd();
            W(0x4E6D) = (rnd() % 2) ? (uint8_t)((rnd() % 10) << 4 | (rnd() % 10)) : (uint8_t)rnd();
            W(0x4E6E) = (rnd() % 3) ? (uint8_t)((rnd() % 10) << 4 | (rnd() % 10)) : (uint8_t)rnd();
            if (rnd() % 4 == 0) W(0x4E6E) = 0x99;
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
        const char *w = run_pair(X, Y, table, s, start);
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, native %u)\n", s.entry, c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
    }
    return bad;
}

// ---- real entry states ---------------------------------------------------------------------------------------------------------------------------
struct Real { unsigned long entries[6] = {0}, checked[6] = {0}, bad[6] = {0}; unsigned long stride[6] = {7, 1, 1, 1, 1, 1}; };
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
        std::fprintf(stderr, "usage: test_native_numbers PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]\n");
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
    counted_table[0x2ACE] = counted<0>; counted_table[0x2ABE] = counted<1>; counted_table[0x2AAF] = counted<2>; counted_table[0x2B0B] = counted<3>; counted_table[0x02DF] = counted<4>;

    static Machine X(prog), Y(prog);
    g_X = &X; g_Y = &Y;

    // ---- part 1
    unsigned long bad1[5], skipped = 0, total_bad1 = 0;
    for (int w = 0; w < kN; ++w) { bad1[w] = isolated(X, Y, g_helpers_only, w, cases, &skipped); total_bad1 += bad1[w]; }
    std::printf("part 1: %ld random whole calls of each routine (all registers incl. IX and IY, R, WZ and memory random): differences 2ACE %lu, 2ABE %lu, 2AAF %lu, 2B0B %lu, 02DF %lu\n",
                cases, bad1[0], bad1[1], bad1[2], bad1[3], bad1[4]);
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
        if (!g_real.checked[i]) { std::printf("FAIL: a routine was never compared in the coverage runs\n"); ok = false; }
    }
    if (total_bad2) ok = false;

    // ---- part 3
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("A: native number helpers only (calls counted)", counted_table, false);
    h.add_variant("B: all readable rules + N12, N13 and N14 helpers", g_after_n12, false);
    static Machine::RuleHook c_table[0x10000];
    for (int i = 0; i < 0x10000; ++i) c_table[i] = nullptr;
    for (int i = 0; i < 0x10000; ++i) c_table[i] = (i == 0x2ACE) ? nullptr : (Machine::RuleHook)nullptr;
    c_table[0x2ACE] = counted_c;
    h.add_variant("C: native 2ACE alone (calls counted)", c_table, false);
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
    // 2AAF, 2B0B and 02DF are not contained in another replaced routine, so the calls are compared one to one; 2ACE is called inside 2ABE and 2ABE inside 2AAF
    // (and entered by JR from it): the native versions do not call each other, so those counts differ by design.
    bool calls_ok = true;
    for (int i = 0; i < kN; ++i) {
        const bool exact = i == 2 || i == 3 || i == 4;
        if (i == 0) {   // 2ACE: variant A never reaches it (2ABE does the whole row), so it is compared one to one in variant C
            const unsigned long rn = (unsigned long)h.entries(0x2ACE);
            std::printf("  2ACE: original machine entered it %lu times; native version C ran %lu times%s\n", rn, g_calls_c, rn == g_calls_c ? "  (equal)" : "  (NOT EQUAL)");
            if (rn != g_calls_c || g_calls_c == 0) calls_ok = false;
            continue;
        }
        const unsigned long ref_n = (unsigned long)h.entries(kEntries[i]);
        std::printf("  %04X: original machine entered it %lu times; native version A ran %lu times%s\n", kEntries[i], ref_n, g_calls[i], exact ? (ref_n == g_calls[i] ? "  (equal)" : "  (NOT EQUAL)") : "  (not compared: see above)");
        if (exact && ref_n != g_calls[i]) calls_ok = false;
        if (g_calls[i] == 0) calls_ok = false;
    }
    const bool lock_ok = h.all_identical() && calls_ok;
    std::printf("  number helpers in lockstep: %s (scripted part %ld frames)\n", lock_ok ? "identical on every frame, all variants; the native versions ran in every routine" : "DIFFERENT or counts wrong", scripted_frames);
    if (!lock_ok) ok = false;

    // ---- part 4
    struct W { const char *name; int which; Machine::RuleHook fn; } wrongs[] = {
        {"2ACE one cycle too slow", 0, wrong_2ace_slow}, {"2ABE DE one too far", 1, wrong_2abe_de}, {"2AAF a stack leftover wrong", 2, wrong_2aaf_stack},
        {"2B0B pointer one too far", 3, wrong_2b0b_ptr}, {"02DF credits limit 98", 4, wrong_02df_limit}};
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
        t[0x2ACE] = wrong_2ace_slow;
        hw.add_variant("W: 2ACE one cycle too slow", t, false);
        lockstep::g_harness = &hw;
        Player pl;
        static Machine WM(prog);
        for (long fr = 0; fr < 3000 && !hw.all_stopped(); ++fr) { pl.joystick(fr); pl.inputs(WM, fr); probe::run_frame(WM, [](uint16_t) {}); }
        lockstep::g_harness = nullptr;
        const long c = hw.first_difference_frame(0);
        std::printf("part 4: lockstep with the wrong 2ACE (one cycle too slow): %s%s\n", c >= 0 ? "caught at frame " : "NOT caught", c >= 0 ? std::to_string(c).c_str() : "");
        if (c < 0) ok = false;
    }

    // ---- part 5
    const unsigned long mf = cov < 20000 ? cov : 20000;
    const unsigned m_rom = meter(prog, nullptr, mf);
    const unsigned m_before = meter(prog, g_before_n12, mf);
    const unsigned m_after = meter(prog, g_after_n12, mf);
    std::printf("part 5: progress meter (coverage players A, B, C of %lu frames and D of a fifth; distinct ROM instruction addresses executed): original ROM %u; readable rules + N12 and N13 helpers (before N14) %u; with the N14 number helpers %u\n",
                mf, m_rom, m_before, m_after);
    if (!(m_after < m_before)) { std::printf("FAIL: the progress meter did not go down\n"); ok = false; }

    std::printf("%s test_native_numbers\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
