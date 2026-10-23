// N12: the native helper routines (src/game/helpers.h, adapters in src/rules/rules.h namespace helpers) against the ROM routines
// 0008 (fill), 0010 (table byte), 0018 (table word), 0020 (jump-table dispatch), 0028 (queue a task, inline data), 0030 (timed event, inline data), 0042 (queue a task).
// Usage: test_native_helpers PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]
//  part 1  isolated: random whole calls, ROM routine and native version from the same random state (all registers random): memory, I/O writes, every register
//          including R and WZ, and the exact cycle count must be equal
//  part 2  isolated from REAL entry states: the same comparison at every (or every n-th) entry of the routines in the coverage players A, B, C and D
//  part 3  lockstep (tests/lockstep.h): the original machine against (a) the helpers alone and (b) all readable rules plus the helpers, on a scripted player
//          and on the four coverage players; all memory, I/O writes, cycle count of every frame and the CPU registers after every frame
//  part 4  deliberately wrong versions must be caught (isolated and in lockstep)
//  part 5  progress meter: ROM instruction addresses still executed in the coverage runs, for the original ROM, the readable rules before N12 and after
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"

namespace {

const uint16_t kEntries[7] = {0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030, 0x0042};
int index_of(uint16_t pc) { for (int i = 0; i < 7; ++i) if (kEntries[i] == pc) return i; return -1; }

uint32_t g_rng = 20261016;
unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

Machine::RuleHook g_helpers_only[0x10000];
Machine::RuleHook g_before_n12[0x10000];   // all readable rules except the N12 helpers
Machine::RuleHook g_after_n12[0x10000];    // all readable rules including the helpers
unsigned long g_calls[7];

template <int I> void counted(Machine &m) { ++g_calls[I]; rules::table()[kEntries[I]](m); }

// deliberately wrong versions
void wrong_0010_slow(Machine &m) { rules::helpers::table_byte_0010(m); m.on_tick(1); }
void wrong_0008_short(Machine &m) { // the last byte is not written
    const uint16_t hl = (uint16_t)m.on_get_hl();
    const uint8_t b = (uint8_t)(m.on_get_bc() >> 8);
    const uint8_t keep = m.bus.read((uint16_t)(hl + (b ? b : 256) - 1));
    rules::helpers::fill_0008(m);
    m.bus.write((uint16_t)(hl + (b ? b : 256) - 1), keep);
}
void wrong_0018_hl(Machine &m) { rules::helpers::table_word_0018(m); m.on_set_hl((uint16_t)(m.on_get_hl() ^ 1)); }
void wrong_0020_flags(Machine &m) { rules::helpers::dispatch_0020(m); m.on_set_f((uint8_t)(m.on_get_af() ^ 0x10)); }
void wrong_0028_swap(Machine &m) { rules::helpers::queue_inline_0028(m); const uint16_t bc = (uint16_t)m.on_get_bc(); m.on_set_bc((uint16_t)(bc << 8 | bc >> 8)); }
void wrong_0030_de(Machine &m) { rules::helpers::timed_event_0030(m); m.on_set_de((uint16_t)(m.on_get_de() + 1)); } // DE one too far
void wrong_0042_wrap(Machine &m) { // wraps at the wrong place: the pointer is not reset to C0
    rules::helpers::queue_push_0042(m);
    if ((m.on_get_hl() & 255) == 0xC0) m.on_set_hl((uint16_t)(m.on_get_hl() + 1));
}

// ---- isolated runs --------------------------------------------------------------------------------------------------------------------------------
struct Setup { uint16_t entry; uint16_t ret; uint16_t sp0; int mode; uint16_t table_target; };

bool done(Machine &m, const Setup &s) {
    const uint16_t pc = (uint16_t)m.on_get_pc(), sp = (uint16_t)m.on_get_sp();
    switch (s.mode) {
        case 0: return pc == s.ret && sp == (uint16_t)(s.sp0 + 2);
        case 1: return pc == (uint16_t)(s.ret + 2) && sp == (uint16_t)(s.sp0 + 2);
        case 2: return (pc == s.ret || pc == (uint16_t)(s.ret + 3)) && sp == (uint16_t)(s.sp0 + 2);
        default: return !((pc >= 0x10 && pc <= 0x17) || (pc >= 0x20 && pc <= 0x27)); // 0020: leaves the routine
    }
}

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
    while (!done(X, s) && guard++ < 5000) X.on_step();
    guard = 0;
    while (!done(Y, s) && guard++ < 5000) {
        if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
        else Y.on_step();
    }
    return compare(X, Y, start, s);
}

unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, int which, long cases, unsigned long *skipped) {
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
        // the timed event slots: a random number of busy slots (first byte not 0), the rest free; sometimes all busy
        const unsigned busy = (rnd() % 5 == 0) ? 16 : rnd() % 17;
        for (unsigned i = 0; i < 16; ++i) W(0x4C90 + 3 * i) = i < busy ? (uint8_t)(1 + rnd() % 255) : 0;
        // the task queue pointer: mostly inside the ring, often near its end; sometimes anywhere
        const unsigned ql = (rnd() % 3 == 0) ? 0xF8 + rnd() % 8 : 0xC0 + rnd() % 0x40;
        W(0x4C80) = (uint8_t)ql;
        W(0x4C81) = (rnd() % 8) ? 0x4C : (uint8_t)(0x4C + (rnd() % 4) * 0 + (rnd() & 3));
        Setup s{kEntries[which], 0, sp0, 0, 0};
        s.mode = which == 4 ? 1 : which == 5 ? 2 : which == 3 ? 3 : 0;
        s.ret = (uint16_t)(0x0100 + rnd() % 0x3E00);
        if (which == 3) { // dispatch: the target of the table entry must lie outside the helper range
            const uint16_t a = (uint16_t)(rnd() & 255);
            const uint16_t t = s.ret;
            const uint16_t at = (uint16_t)(t + ((a * 2) & 255));
            const uint16_t target = (uint16_t)(X.bus.read(at) | (X.bus.read((uint16_t)(at + 1)) << 8));
            (void)target;
        }
        uint16_t af = (uint16_t)rnd(), bc = (uint16_t)rnd(), de = (uint16_t)rnd(), hl = (uint16_t)rnd();
        if (which == 0) { hl = (rnd() % 4) ? (uint16_t)(0x4000 + rnd() % 0xEB0) : (uint16_t)(0x4000 + rnd() % 0xEB0); }   // video, colour and work RAM below the stack
        if (which == 1 || which == 2) hl = (rnd() % 2) ? (uint16_t)(rnd() % 0x4000) : hl;
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
        if (which == 3) { // skip a dispatch whose target lies in the helper range (the end of the call could not be told)
            const uint16_t a = (uint16_t)(af >> 8);
            const uint16_t at = (uint16_t)(s.ret + ((a * 2) & 255));
            const uint16_t target = (uint16_t)(X.bus.read(at) | (X.bus.read((uint16_t)(at + 1)) << 8));
            if (target < 0x70) { ++*skipped; continue; }
        }
        const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (Machine::cycles_per_frame - 1 - (rnd() % 1500));
        const char *w = run_pair(X, Y, table, s, start);
        if (w && ++bad <= 4) std::printf("  ISOLATED MISMATCH (routine %04X) case %ld: %s (ROM %u cycles, native %u)\n", s.entry, c, w, X.ticks_in_frame() - start, Y.ticks_in_frame() - start);
    }
    return bad;
}

// ---- real entry states ---------------------------------------------------------------------------------------------------------------------------
struct Real { unsigned long entries[7] = {0}, checked[7] = {0}, bad[7] = {0}; unsigned long stride[7] = {1, 7, 3, 3, 1, 1, 1}; };
Real g_real;
Machine *g_X = nullptr, *g_Y = nullptr;
const Machine::RuleHook *g_real_table = nullptr;

void real_probe(Machine &ref, uint16_t pc) {
    const int i = index_of(pc);
    if (i < 0) return;
    if (g_real.entries[i]++ % g_real.stride[i]) return;
    // the machine must be in the middle of a frame with room for the routine; the hook falls back near the end of the frame (also compared)
    Setup s{pc, 0, (uint16_t)ref.on_get_sp(), i == 4 ? 1 : i == 5 ? 2 : i == 3 ? 3 : 0, 0};
    s.ret = (uint16_t)(ref.bus.read(s.sp0) | (ref.bus.read((uint16_t)(s.sp0 + 1)) << 8));
    if (i == 3) { // target of the table entry
        const uint16_t a = (uint16_t)(ref.on_get_af() >> 8);
        const uint16_t at = (uint16_t)(s.ret + ((a * 2) & 255));
        const uint16_t target = (uint16_t)(ref.bus.read(at) | (ref.bus.read((uint16_t)(at + 1)) << 8));
        if (target < 0x70) return;
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
        std::fprintf(stderr, "usage: test_native_helpers PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    const long frames = std::atol(argv[2]);
    const unsigned long cov = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 20000;
    const long cases = argc > 4 ? std::atol(argv[4]) : 100000;

    for (int i = 0; i < 0x10000; ++i) {
        g_after_n12[i] = rules::table()[i];
        g_before_n12[i] = rules::table()[i];
    }
    for (uint16_t e : kEntries) { g_helpers_only[e] = rules::table()[e]; g_before_n12[e] = nullptr; }
    bool ok = true;
    for (uint16_t e : kEntries) if (!g_after_n12[e]) { std::printf("FAIL: no hook at %04X\n", e); ok = false; }
    static Machine::RuleHook counted_table[0x10000];
    for (int i = 0; i < 0x10000; ++i) counted_table[i] = g_helpers_only[i];
    counted_table[0x0008] = counted<0>; counted_table[0x0010] = counted<1>; counted_table[0x0018] = counted<2>; counted_table[0x0020] = counted<3>;
    counted_table[0x0028] = counted<4>; counted_table[0x0030] = counted<5>; counted_table[0x0042] = counted<6>;

    static Machine X(prog), Y(prog);
    g_X = &X; g_Y = &Y;

    // ---- part 1
    unsigned long bad1[7], skipped = 0, total_bad1 = 0;
    for (int w = 0; w < 7; ++w) { bad1[w] = isolated(X, Y, g_helpers_only, w, cases, &skipped); total_bad1 += bad1[w]; }
    std::printf("part 1: %ld random whole calls of each routine (all registers, R, WZ and memory random; %lu dispatch cases skipped): differences 0008 %lu, 0010 %lu, 0018 %lu, 0020 %lu, 0028 %lu, 0030 %lu, 0042 %lu\n",
                cases, skipped, bad1[0], bad1[1], bad1[2], bad1[3], bad1[4], bad1[5], bad1[6]);
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
    for (int i = 0; i < 7; ++i) {
        std::printf("  %04X: %lu / %lu / %lu%s\n", kEntries[i], g_real.entries[i], g_real.checked[i], g_real.bad[i], g_real.entries[i] == 0 ? "   (never entered in these runs)" : "");
        total_bad2 += g_real.bad[i];
    }
    if (total_bad2) ok = false;
    if (!g_real.checked[1] || !g_real.checked[2] || !g_real.checked[3] || !g_real.checked[4] || !g_real.checked[5] || !g_real.checked[6]) { std::printf("FAIL: a routine was never compared in the coverage runs\n"); ok = false; }

    // ---- part 3
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("A: native helpers only (calls counted)", counted_table, false);
    h.add_variant("B: all readable rules + helpers", g_after_n12, false);
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
    // Calls can be compared one to one for the routines that no other replaced routine contains: 0018, 0020, 0028 and 0030. 0008 is also entered at its loop
    // head (DJNZ 0008) by the ROM, 0010 is called inside 0018 and 0020, 0042 inside 0028: the native versions do not call each other, so those counts differ by design.
    bool calls_ok = true;
    for (int i = 0; i < 7; ++i) {
        const bool exact = i == 2 || i == 3 || i == 4 || i == 5;
        const unsigned long ref_n = (unsigned long)h.entries(kEntries[i]);
        std::printf("  %04X: original machine entered it %lu times; native version A ran %lu times%s\n", kEntries[i], ref_n, g_calls[i], exact ? (ref_n == g_calls[i] ? "  (equal)" : "  (NOT EQUAL)") : "  (not compared: see above)");
        if (exact && ref_n != g_calls[i]) calls_ok = false;
        if (g_calls[i] == 0) calls_ok = false;
    }
    const bool lock_ok = h.all_identical() && calls_ok;
    std::printf("  helpers in lockstep: %s (scripted part %ld frames)\n", lock_ok ? "identical on every frame, both variants; the native versions ran in every routine" : "DIFFERENT or counts wrong", scripted_frames);
    if (!lock_ok) ok = false;

    // ---- part 4
    struct W { const char *name; int which; Machine::RuleHook fn; } wrongs[] = {
        {"0010 one cycle too slow", 1, wrong_0010_slow}, {"0008 last byte not written", 0, wrong_0008_short}, {"0018 HL wrong", 2, wrong_0018_hl},
        {"0020 a flag wrong", 3, wrong_0020_flags}, {"0028 B and C swapped", 4, wrong_0028_swap}, {"0030 DE one too far", 5, wrong_0030_de}, {"0042 pointer wrap wrong", 6, wrong_0042_wrap}};
    unsigned caught = 0;
    static Machine::RuleHook wt[0x10000];
    for (const W &w : wrongs) {
        for (int i = 0; i < 0x10000; ++i) wt[i] = nullptr;
        wt[kEntries[w.which]] = w.fn;
        const unsigned long b = isolated(X, Y, wt, w.which, 3000, &skipped);
        std::printf("part 4: wrong version \"%s\": %s (%lu of 3000 isolated cases differ)\n", w.name, b ? "caught" : "NOT caught", b);
        if (b) ++caught; else ok = false;
    }
    {
        lockstep::Harness hw(prog);
        hw.add_variant("W: 0010 one cycle too slow", ({ static Machine::RuleHook t[0x10000]; for (int i = 0; i < 0x10000; ++i) t[i] = nullptr; t[0x0010] = wrong_0010_slow; t; }), false);
        lockstep::g_harness = &hw;
        Player pl;
        static Machine WM(prog);
        for (long fr = 0; fr < 3000 && !hw.all_stopped(); ++fr) { pl.joystick(fr); pl.inputs(WM, fr); probe::run_frame(WM, [](uint16_t) {}); }
        lockstep::g_harness = nullptr;
        const long c = hw.first_difference_frame(0);
        std::printf("part 4: lockstep with the wrong 0010 (one cycle too slow): %s%s\n", c >= 0 ? "caught at frame " : "NOT caught", c >= 0 ? std::to_string(c).c_str() : "");
        if (c < 0) ok = false;
    }

    // ---- part 5
    const unsigned long mf = cov < 20000 ? cov : 20000;
    const unsigned m_rom = meter(prog, nullptr, mf);
    const unsigned m_before = meter(prog, g_before_n12, mf);
    const unsigned m_after = meter(prog, g_after_n12, mf);
    std::printf("part 5: progress meter (coverage players A, B, C of %lu frames and D of a fifth; distinct ROM instruction addresses executed): original ROM %u; readable rules before N12 %u; readable rules with the N12 helpers %u\n",
                mf, m_rom, m_before, m_after);
    if (!(m_after < m_before)) { std::printf("FAIL: the progress meter did not go down\n"); ok = false; }

    std::printf("%s test_native_helpers\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
