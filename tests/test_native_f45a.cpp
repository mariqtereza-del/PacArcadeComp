// N45a: the per-frame game update, part 1 (ROM 0AC3-0C41, 0E23-0E35) (src/game/attract.h with the instruction list src/game/attract_prog.h, hook rules::attract::hook) against the ROM.
// Same test structure as N28 (tests/test_native_f45a.cpp), for the new entry addresses of this step. A run starts at one of the addresses where the ROM can enter the code and ends at the first instruction that
// leaves the straight-line code (CALL, RST, RET, JP); the test stops both machines there and compares everything (registers, memory, stack bytes, cycle count).
// Usage: test_native_f45a PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES_PER_ENTRY]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
#include "lockstep.h"
#include "coverage_player.h"
#include "../src/game/attract.h"

namespace {

const uint16_t (&kEntries)[game::att::kStarts45aN] = game::att::kStarts45a;
const int kN = game::att::kStarts45aN;
uint16_t g_fin_pc; uint16_t g_fin_sp;   // where the ROM run ended
int index_of(uint16_t pc) { for (int i = 0; i < kN; ++i) if (kEntries[i] == pc) return i; return -1; }

uint32_t g_rng = 20261016;
unsigned rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }

Machine::RuleHook g_helpers_only[0x10000];   // only the N16 entries
Machine::RuleHook g_before_n12[0x10000];     // all readable rules except the N16 status pictures (the N12 and N16 helpers are in)
Machine::RuleHook g_after_n12[0x10000];      // all readable rules including the status pictures
unsigned long g_calls[128];

template <int I> void counted(Machine &m) { ++g_calls[I]; rules::table()[kEntries[I]](m); }
void wrong_cyc_a(Machine &m) { rules::table()[0x0AC3](m); m.on_tick(1); }
void wrong_cyc_l(Machine &m) { rules::table()[0x0AC3](m); m.on_tick(1); }   // for the lockstep run

void counted_loop(Machine &m) { const int i = index_of((uint16_t)m.on_get_pc()); if (i >= 0) ++g_calls[i]; rules::table()[(uint16_t)m.on_get_pc()](m); }
// deliberately wrong versions
void wrong_flag(Machine &m) { rules::table()[0x0BD6](m); m.on_set_af((uint16_t)(m.on_get_af() ^ 0x0008)); }
void wrong_block(Machine &m) { rules::table()[0x0C0D](m); m.bus.write(0x4E91, (uint8_t)(m.bus.read(0x4E91) ^ 1)); }   // the sound register byte 4E91 wrong
void wrong_ret(Machine &m) { rules::table()[0x0E23](m); m.bus.write((uint16_t)m.on_get_sp(), (uint8_t)(m.bus.read((uint16_t)m.on_get_sp()) ^ 1)); }
void wrong_hl(Machine &m) { rules::table()[0x0B68](m); m.on_set_bc((uint16_t)(m.on_get_bc() + 1)); }

// ---- isolated runs --------------------------------------------------------------------------------------------------------------------------------
struct Setup { uint16_t entry; uint16_t ret; uint16_t sp0; int mode; uint16_t table_target; uint16_t exit = 0; bool stay = false; };

bool done(Machine &m, const Setup &) { return (uint16_t)m.on_get_pc() == g_fin_pc && (uint16_t)m.on_get_sp() == g_fin_sp; }

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
    bool fin = false;
    while (!fin && guard++ < 100000) {
        const game::att::Ins *in = game::att::find((uint16_t)X.on_get_pc());
        X.on_step();
        fin = (in && game::att::ends_at(*in, (uint16_t)X.on_get_pc())) || game::att::is_start((uint16_t)X.on_get_pc());   // (19CD: the dot-eating code, where the fruit code falls out; the core runs a prefix byte DD / ED as its own step: then the address is in the middle of an instruction and the run goes on)
    }
    g_fin_pc = (uint16_t)X.on_get_pc(); g_fin_sp = (uint16_t)X.on_get_sp();
    if (std::getenv("NH_DEBUG2")) { Machine Z0 = Y; if (Z0.rules[Z0.on_get_pc()]) Z0.rules[Z0.on_get_pc()](Z0); std::fprintf(stderr, "  [%04X] ROM end pc %04X sp %04X ticks %u | native end pc %04X sp %04X ticks %u\n", (unsigned)s.entry, g_fin_pc, g_fin_sp, X.ticks_in_frame(), (unsigned)Z0.on_get_pc(), (unsigned)Z0.on_get_sp(), Z0.ticks_in_frame()); }
    guard = 0;
    do {   // (at least one step: the loop entry 26A6 ends at its own address)
        if (Y.rules[Y.on_get_pc()]) Y.rules[Y.on_get_pc()](Y);
        else Y.on_step();
    } while (!done(Y, s) && guard++ < 100000);
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

unsigned long g_all_native[128], g_cases_run[128];   // cases in which the hook never fell back to the ROM
unsigned long isolated(Machine &X, Machine &Y, const Machine::RuleHook *table, int which, long cases, unsigned long *skipped, bool cut_mode = false) {
    (void)skipped;
    unsigned long bad = 0;
    static Machine *pristine = nullptr;
    if (!pristine) pristine = new Machine(X);   // the first call gets fresh machines (see main)
    const uint16_t sp0 = 0x4FB0;
    for (long c = 0; c < cases; ++c) {
        X = *pristine;   // every case starts from the same fresh machine, so that one difference cannot spill into the next case
        Y = *pristine;
        uint8_t in0 = 0;
        uint8_t ram[0x3F0];
        for (auto &b : ram) b = (uint8_t)rnd();
        uint8_t video[0x400], colour[0x400];
        for (auto &b : video) b = (uint8_t)rnd();
        for (auto &b : colour) b = (uint8_t)rnd();
        Setup s{kEntries[which], 0, sp0, 0, 0};
        s.ret = (uint16_t)(0x0100 + rnd() % 0x3E00);
        if (cut_mode) { s.ret = 0x4D00; ram[0x100] = 0x18; ram[0x101] = 0xFE; }   // after the routine: JR $ in RAM, so that both machines spin in the same place
        uint16_t af = (uint16_t)rnd(), bc = (uint16_t)rnd(), de = (uint16_t)rnd(), hl = (uint16_t)rnd();
        bool hl_at = false;
        static const uint16_t chx[] = {0x4E9C, 0x4EAC, 0x4EBC, 0x4ECC, 0x4EDC, 0x4EEC}, chy[] = {0x4E8C, 0x4E92, 0x4E97};
        const bool chan = rnd() % 4 != 0;   // mostly a real voice (IX = a voice record, IY = its register block), sometimes garbage
        const uint16_t ix = chan ? chx[rnd() % 6] : (uint16_t)rnd(), iy = chan ? chy[rnd() % 3] : (uint16_t)rnd();
        if (chan) {   // the voice records: small counts, single bits and the bytes the routine tests
            static const uint8_t pick[] = {0, 0, 1, 2, 3, 4, 7, 8, 0x0F, 0x10, 0x20, 0x40, 0x7F, 0x80, 0xFF, 0xF1};
            for (unsigned vch = 0; vch < 6; ++vch) for (unsigned k = 0; k < 16; ++k) ram[chx[vch] - 0x4C00 + k] = (rnd() % 3) ? pick[rnd() % 16] : (uint8_t)rnd();
            for (unsigned vch = 0; vch < 6; ++vch) if (rnd() % 4) { ram[chx[vch] - 0x4C00 + 6] = (uint8_t)rnd(); ram[chx[vch] - 0x4C00 + 7] = (uint8_t)(0x3B + rnd() % 5); }   // the tune data pointer into the ROM tune tables
            ram[0x4C84 - 0x4C00] = (uint8_t)rnd();
            if (rnd() % 2) { ram[ix - 0x4C00 + 2] = (uint8_t)(1u << (rnd() % 8)); ram[ix - 0x4C00] = (uint8_t)(ram[ix - 0x4C00 + 2] | (rnd() % 2 ? rnd() : 0)); }
        }
        {   // the bytes the code reads: the credits (zero half of the time), the demo / game states, the text offset
            auto set = [&](uint16_t at, uint8_t v) { ram[at - 0x4C00] = v; };
            set(0x4E6E, (uint8_t)(rnd() % 2 ? 0 : rnd()));
            set(0x4E02, (uint8_t)(rnd() % 40)); set(0x4E75, (uint8_t)(rnd() % 3));
            { static const uint16_t th[] = {0x77, 0xB3, 0xC2, 0xD1, 0xE0, 0xEF, 0xFE, 0x10D, 0x11C, 0x12B, 0x13A, 0x158, 0x1B7}; const uint16_t v = (uint16_t)(rnd() % 2 ? th[rnd() % 13] : rnd() % 0x200); set(0x4DC5, (uint8_t)v); set(0x4DC6, (uint8_t)(v >> 8)); }
            set(0x4DA5, (uint8_t)(rnd() % 16)); set(0x4E72, (uint8_t)(rnd() % 2 ? 0xFF : rnd())); set(0x4E09, (uint8_t)(rnd() % 2)); set(0x4E14, (uint8_t)rnd() ); set(0x4E15, (uint8_t)rnd()); set(0x4E04, (uint8_t)(rnd() % 40)); set(0x4E08, (uint8_t)(rnd() % 7)); { static const uint8_t v[] = {0x25, 0x2D, 0x1E, 0xFF, 0xFE, 0x00}; const bool hit = rnd() % 2; set(0x4D3A, hit ? v[rnd() % 6] : (uint8_t)rnd()); set(0x4D01, hit ? v[rnd() % 6] : (uint8_t)rnd()); set(0x4D32, hit ? v[rnd() % 6] : (uint8_t)rnd()); }
            in0 = (uint8_t)rnd();
        }
        if (rnd() % 4) {   // the bytes the frame update tests: the counters, the ghost flags, the compare value, the flash counters
            auto S2 = [&](uint16_t at, uint8_t v) { ram[at - 0x4C00] = v; };
            S2(0x4DA4, (uint8_t)(rnd() % 3 ? 0 : rnd())); S2(0x4DC8, (uint8_t)(rnd() % 3 ? 0 : rnd() % 20));
            for (uint16_t a2 = 0x4DA6; a2 <= 0x4DAA; ++a2) S2(a2, (uint8_t)(rnd() % 2));
            for (uint16_t a2 = 0x4DAC; a2 <= 0x4DAF; ++a2) S2(a2, (uint8_t)(rnd() % 2));
            const uint16_t w = (uint16_t)(rnd() % 2 ? rnd() % 0x200 : 0x0100 + (int)(rnd() % 3) - 1); S2(0x4DCB, (uint8_t)w); S2(0x4DCC, (uint8_t)(w >> 8));
            S2(0x4E02, (uint8_t)(rnd() % 3 ? 0x22 : rnd())); S2(0x4E04, (uint8_t)(rnd() % 3 ? 3 : rnd() % 8));
            S2(0x4DCF, (uint8_t)(rnd() % 3 ? 9 : rnd() % 12)); S2(0x4DC4, (uint8_t)(rnd() % 3 ? 7 : rnd() % 10)); S2(0x4DC0, (uint8_t)rnd());
            for (uint16_t a2 = 0x4C0B; a2 <= 0x4C0B; ++a2) S2(a2, (uint8_t)(rnd() % 3 ? 9 : rnd()));
            for (uint16_t a2 = 0x4C03; a2 <= 0x4C09; a2 = (uint16_t)(a2 + 2)) S2(a2, (uint8_t)(rnd() % 3 ? (rnd() % 2 ? 0x11 : 0x12) : rnd()));
            S2(0x4EAC, (uint8_t)rnd());
            for (unsigned k = 0; k < 8; ++k) colour[(0x464 + k * 0x20) & 0x3FF] = (uint8_t)(rnd() % 2 ? 0x10 : rnd());
        }
        if (hl_at && rnd() % 2) hl = 0x8094;
        if (chan && rnd() % 2) { static const uint16_t tb[] = {0x3BC8, 0x3BCC, 0x3BD0, 0x3BB0, 0x3BB8, 0x3BC0}; hl = tb[rnd() % 6]; }
        const uint8_t in1v = (uint8_t)(rnd() % 2 ? 0xFF : rnd());
        const bool iffv = rnd() % 2;
        for (Machine *m : {&X, &Y}) { m->bus.in0 = in0; m->bus.in1 = in1v; m->on_set_iff1(iffv); m->on_set_iff2(iffv); }
        const uint16_t wz0 = (uint16_t)rnd();
        const uint8_t r0 = (uint8_t)rnd();
        for (Machine *m : {&X, &Y}) {
            std::memcpy(m->bus.workram, ram, sizeof ram);
            std::memcpy(m->bus.videoram, video, sizeof video);
            std::memcpy(m->bus.colorram, colour, sizeof colour);
            m->bus.write(sp0, s.ret & 255);
            m->bus.write((uint16_t)(sp0 + 1), s.ret >> 8);
            if (s.entry == 0x01D4) { m->bus.write((uint16_t)(sp0 + 2), s.ret & 255); m->bus.write((uint16_t)(sp0 + 3), s.ret >> 8); }   // POP AF then RET: the return address is the second word
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
struct Real { unsigned long entries[128] = {0}, checked[128] = {0}, bad[128] = {0}; unsigned long stride[128]; Real() { for (auto &s : stride) s = 7; } };
Real g_real;
Machine *g_X = nullptr, *g_Y = nullptr;
const Machine::RuleHook *g_real_table = nullptr;

void real_probe(Machine &ref, uint16_t pc) {
    const int i = index_of(pc);
    if (i < 0) return;
    if (g_real.entries[i]++ % g_real.stride[i]) return;
    // the machine must be in the middle of a frame with room for the routine; the hook falls back near the end of the frame (also compared)
    Setup s{pc, 0, (uint16_t)ref.on_get_sp(), 0, 0};
    { const uint16_t at = (uint16_t)(s.sp0 + (pc == 0x01D4 ? 2 : 0)); s.ret = (uint16_t)(ref.bus.read(at) | (ref.bus.read((uint16_t)(at + 1)) << 8)); }
    *g_X = ref;
    *g_Y = ref;
    ++g_real.checked[i];
    // the plain loop of the probe does not reset the tick counter per frame, so the position inside the frame is chosen here (mostly mid-frame, sometimes near its end)
    const uint32_t start = (rnd() % 5) ? (rnd() % 20000) : (Machine::cycles_per_frame - 1 - (rnd() % 1500));
    const char *w = run_pair(*g_X, *g_Y, g_real_table, s, start);
    if (w && std::getenv("NH_DEBUG")) { for (unsigned ad = 0x4C00; ad < 0x5000; ++ad) if (g_X->bus.read((uint16_t)ad) != g_Y->bus.read((uint16_t)ad)) { std::fprintf(stderr, "  [%04X] diff at %04X: ROM %02X native %02X (sp %04X)\n", pc, ad, g_X->bus.read((uint16_t)ad), g_Y->bus.read((uint16_t)ad), (unsigned)ref.on_get_sp()); break; } }
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
    if (argc < 3) {
        std::fprintf(stderr, "usage: test_native_f45a PROG.BIN FRAMES [COVERAGE_FRAMES] [ISOLATED_CASES]\n");
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
    for (uint16_t e : kEntries) { g_helpers_only[e] = rules::table()[e]; g_before_n12[e] = nullptr; }   // (g_before_n12: the demo hooks off, everything else on)
    bool ok = true;
    for (uint16_t e : kEntries) if (!g_after_n12[e]) { std::printf("FAIL: no hook at %04X\n", e); ok = false; }
    static Machine::RuleHook counted_table[0x10000];
    for (int i = 0; i < 0x10000; ++i) counted_table[i] = nullptr;
    for (int i = 0; i < kN; ++i) counted_table[kEntries[i]] = counted_loop;

    static Machine X(prog), Y(prog);
    g_X = &X; g_Y = &Y;

    // ---- part 1
    unsigned long bad1[128], skipped = 0, total_bad1 = 0;
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
    const unsigned long cuts_before_lockstep = rules::sliced_cuts;
    lockstep::Harness h(prog);
    h.load_routine_map("docs/notes/rom_routines.md");
    h.add_variant("A: native frame-update code only (calls counted)", counted_table, false);
    h.add_variant("B: all readable rules + N12 to N45a", g_after_n12, false);
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
        const bool exact = true;   // every block is reached once per call in the ROM too (the chain runs straight through the entry addresses)   // the starts are entered once per call; the loop heads are entered once per round in the ROM, only after a cut here
        const unsigned long ref_n = (unsigned long)h.entries(kEntries[i]);
        std::printf("  %04X: original machine entered it %lu times; native version (A or C) ran %lu times%s\n", kEntries[i], ref_n, g_calls[i], exact ? (ref_n == g_calls[i] ? "  (equal)" : "  (NOT EQUAL)") : "  (not compared: see above)");
        if (exact && ref_n != g_calls[i]) calls_ok = false;
        if (g_calls[i] == 0 && ref_n != 0) calls_ok = false;
    }
    const bool lock_ok = h.all_identical() && calls_ok;
    std::printf("  frame-update code in lockstep: %s (scripted part %ld frames)\n", lock_ok ? "identical on every frame, all variants; the native versions ran in every routine" : "DIFFERENT or counts wrong", scripted_frames);
    if (!lock_ok) ok = false;

    // ---- part 4
    auto at = [&](uint16_t a) { for (int i = 0; i < kN; ++i) if (kEntries[i] == a) return i; return 0; };
    struct W { const char *name; int which; Machine::RuleHook fn; } wrongs[] = {
        {"0AC3 one cycle too slow", at(0x0AC3), wrong_cyc_a}, {"0BD6 an undocumented flag bit wrong", at(0x0BD6), wrong_flag}, {"0C0D the byte 4E91 wrong", at(0x0C0D), wrong_block},
        {"0E23 the return address wrong", at(0x0E23), wrong_ret}, {"0B68 register BC wrong", at(0x0B68), wrong_hl}};
    static Machine::RuleHook wt[0x10000];
    for (const W &w : wrongs) {
        for (int i = 0; i < 0x10000; ++i) wt[i] = nullptr;
        wt[kEntries[w.which]] = w.fn;
        const unsigned long b = isolated(X, Y, wt, w.which, 3000, &skipped, false);
        std::printf("part 4: wrong version \"%s\": %s (%lu of 3000 isolated cases differ)\n", w.name, b ? "caught" : "NOT caught", b);
        if (!b) ok = false;
    }
    {
        lockstep::Harness hw(prog);
        static Machine::RuleHook t[0x10000];
        for (int i = 0; i < 0x10000; ++i) t[i] = nullptr;
        t[0x0AC3] = wrong_cyc_l;
        hw.add_variant("W: the effect driver 0AC3 one cycle too slow", t, false);
        lockstep::g_harness = &hw;
        Player pl;
        static Machine WM(prog);
        for (long fr = 0; fr < 12000 && !hw.all_stopped(); ++fr) { pl.joystick(fr); pl.inputs(WM, fr); probe::run_frame(WM, [](uint16_t) {}); }
        { static Machine PW(prog); covplayer::run(PW, cov, nullptr); covplayer::run_b(prog, cov, nullptr); covplayer::run_c(prog, cov, nullptr); covplayer::run_d(prog, cov / 5 ? cov / 5 : 1, nullptr); }
        lockstep::g_harness = nullptr;
        const long c = hw.first_difference_frame(0);
        std::printf("part 4: lockstep with the ghost sprite routine 0AC3 (one cycle too slow): %s%s\n", c >= 0 ? "caught at frame " : "NOT caught", c >= 0 ? std::to_string(c).c_str() : "");
        if (c < 0) ok = false;
    }

    // ---- part 5
    const unsigned long mf = cov < 20000 ? cov : 20000;
    const unsigned m_rom = meter(prog, nullptr, mf);
    const unsigned m_before = meter(prog, g_before_n12, mf);
    const unsigned m_after = meter(prog, g_after_n12, mf);
    std::printf("part 5: progress meter (coverage players A, B, C of %lu frames and D of a fifth; distinct ROM instruction addresses executed; an address handled by a native hook does not count unless the hook ran the ROM instruction): original ROM %u; readable rules + N12 to N45a (before N45a) %u; with the N45a frame-update code %u\n",
                mf, m_rom, m_before, m_after);
    if (!(m_after < m_before)) { std::printf("FAIL: the progress meter did not go down\n"); ok = false; }

    std::printf("%s test_native_f45a\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
