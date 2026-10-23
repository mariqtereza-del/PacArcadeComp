// N08: the shared lockstep harness. FOR TESTS ONLY. Every step of Track D (and the T02 rule tests) proves "the game stays identical" with this one comparison.
//
// What it does: a REFERENCE machine runs the original ROM routines with the plain instruction loop (like tests/logic_probe.h, so a per-instruction callback is
// possible and the cycles of the frame are counted). One or more VARIANT machines run the real game loop (Machine::run_frame) with their own readable rules
// table (Machine::rules) or none. After EVERY frame the harness compares reference and variant: work RAM (the stack included), video RAM, colour RAM, sprite RAM
// and coordinates, sound registers, the output latches, interrupt vector, watchdog count, the list of I/O writes of the frame, and the cycle count of the
// frame; the CPU registers are compared too (the first register difference is reported; it fails the run only when strict_registers is set).
// Garbage mode: a variant marked garbage has rules::poison set while it runs, so every register that an adapter leaves alone is filled with garbage.
// A variant stops being compared after its first difference (the frame and the name of the difference are kept).
//
// The INPUT comes from any "player" that drives the reference machine as the coverage players of tests/coverage_player.h do: it sets the inputs (in0, in1, dsw1,
// dsw2) and may poke RAM between frames, and then calls probe::run_frame(machine, callback). The harness copies the inputs and the pokes (found by comparing the
// RAM with the end of the previous frame) to every variant, and starts the variants afresh when the player starts a new machine (a new DIP setting, a power-on).
//
// Use (include this header BEFORE coverage_player.h / logic_probe.h, because it replaces probe::run_frame):
//     #include "lockstep.h"
//     lockstep::Harness h(prog);                          h.add_variant("readable rules", rules::table(), false);
//     lockstep::g_harness = &h;                           ...run a player (it calls probe::run_frame for the reference)...   lockstep::g_harness = nullptr;
//     h.report(stdout);                                   h.all_identical() / h.first_difference_frame(0)
// Reports use addresses, counts and cycle numbers only (no ROM bytes). The PROGRESS line is the progress meter of Track D (routines replaced by the variant).
#pragma once
#include "../src/board/machine.h" // before the rename below: Machine::run_frame must keep its name
#define run_frame run_frame_plain // keep the plain template of logic_probe.h under another name; the replacement is defined below
#include "logic_probe.h"
#undef run_frame
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "../src/rules/rules.h"
#include "../src/game/output.h"

namespace lockstep {

// Compares two machines; returns a short name of the first difference or nullptr. regs = also compare the CPU registers.
inline const char *differ(Machine &a, Machine &b, bool regs) {
    const Bus &x = a.bus, &y = b.bus;
    if (std::memcmp(x.workram, y.workram, sizeof x.workram)) return "work RAM";
    // N53: the output is compared through the named structs, mapped back to the old layout (src/game/output.h)
    {
        static uint8_t ov[2][0x400], oc[2][0x400], os[2][16], oq[2][16], ow[2][32], ol[2][8];
        game::output_to_old_layout(game::output_view(const_cast<Bus &>(x)), ov[0], oc[0], os[0], oq[0], ow[0], ol[0]);
        game::output_to_old_layout(game::output_view(const_cast<Bus &>(y)), ov[1], oc[1], os[1], oq[1], ow[1], ol[1]);
        if (std::memcmp(ov[0], ov[1], sizeof ov[0])) return "video RAM";
        if (std::memcmp(oc[0], oc[1], sizeof oc[0])) return "colour RAM";
        if (std::memcmp(os[0], os[1], 16) || std::memcmp(oq[0], oq[1], 16)) return "sprites";
        if (std::memcmp(ow[0], ow[1], 32)) return "sound registers";
    }
    if (std::memcmp(x.latch, y.latch, sizeof x.latch) || x.irq_vector != y.irq_vector || x.watchdog_writes != y.watchdog_writes) return "latches";
    if (x.io_log.size() != y.io_log.size()) return "I/O writes";
    for (size_t i = 0; i < x.io_log.size(); ++i)
        if (x.io_log[i].frame != y.io_log[i].frame || x.io_log[i].address != y.io_log[i].address || x.io_log[i].value != y.io_log[i].value) return "I/O writes";
    if (regs && (a.on_get_af() != b.on_get_af() || a.on_get_bc() != b.on_get_bc() || a.on_get_de() != b.on_get_de() || a.on_get_hl() != b.on_get_hl() ||
                 a.on_get_ix() != b.on_get_ix() || a.on_get_iy() != b.on_get_iy() || a.on_get_sp() != b.on_get_sp() || a.on_get_pc() != b.on_get_pc() ||
                 a.on_get_ir() != b.on_get_ir() || a.on_get_iff1() != b.on_get_iff1() || a.on_get_iff2() != b.on_get_iff2() ||
                 a.on_get_int_mode() != b.on_get_int_mode() || a.on_is_halted() != b.on_is_halted() || a.on_is_int_disabled() != b.on_is_int_disabled() ||
                 a.get_alt_bc() != b.get_alt_bc() || a.get_alt_de() != b.get_alt_de() || a.get_alt_hl() != b.get_alt_hl()))   // N47: the interrupt state of the boot
        return "CPU registers";
    return nullptr;
}

struct Variant {
    std::string name;
    const Machine::RuleHook *rules = nullptr; // nullptr = the original ROM routines (a control run)
    bool garbage = false;
    std::unique_ptr<Machine> m;
    long bad = -1;            // frame of the first difference (not counting the registers), -1 = none yet
    const char *why = nullptr;
    long reg_bad = -1;        // frame of the first register difference
    uint64_t frames_compared = 0;
};

class Harness {
public:
    explicit Harness(const std::vector<uint8_t> &prog) : prog_(prog), init_(prog) {
        snap_ = take(init_);
        entry_flag_.assign(0x4000, 0);
        entry_count_.assign(0x4000, 0);
        seen_.assign(0x4000, 0);
    }

    // options
    bool strict_registers = false; // true: a register difference counts as a failure too

    void add_variant(const std::string &name, const Machine::RuleHook *rules, bool garbage) {
        Variant v;
        v.name = name;
        v.rules = rules;
        v.garbage = garbage;
        variants_.push_back(std::move(v));
        for (unsigned a = 0; rules && a < 0x4000; ++a)
            if (rules[a]) entry_flag_[a] = 1;
    }
    // Count the entries of one more ROM address in the reference run (routines of interest that are not in a rules table).
    void watch_entry(uint16_t pc) { if (pc < 0x4000) entry_flag_[pc] = 1; }
    // The routine map of step N01 (docs/notes/rom_routines.md): lets the report say how much of the program the variant has replaced.
    void load_routine_map(const char *path) {
        std::ifstream f(path);
        std::string line;
        bool in_routines = false;
        while (std::getline(f, line)) {
            if (line.rfind("## ", 0) == 0) in_routines = line.rfind("## Routines", 0) == 0;
            if (!in_routines || line.size() < 8 || line[0] != '|' || line[1] != ' ') continue;
            char *end = nullptr;
            const unsigned long a = std::strtoul(line.c_str() + 2, &end, 16);
            if (!end || end != line.c_str() + 6 || *end != ' ') continue;
            const size_t k1 = line.find('|', 6), k2 = k1 == std::string::npos ? k1 : line.find('|', k1 + 1), k3 = k2 == std::string::npos ? k2 : line.find('|', k2 + 1);
            if (k3 == std::string::npos) continue;
            const unsigned n = (unsigned)std::strtoul(line.c_str() + k2 + 1, nullptr, 10);
            if (n) map_[(unsigned)a] = n, map_total_ += n;
        }
    }

    // ----- the frame step; called from probe::run_frame while the harness is installed ---------------------------------------------------------------
    template <class Before> void frame(Machine &ref, Before &&before) {
        const bool fresh = &ref != last_ref_ || ref.frame < last_frame_;
        if (fresh) start_variants(ref);
        // pokes made by the player since the end of the last frame, and the inputs
        poke_variants(ref);
        // the reference frame: the plain loop, counting entries and cycles
        const uint32_t t0 = ref.ticks_in_frame();
        ref.bus.io_log.clear();
        for (;;) {
            const uint16_t pc = (uint16_t)ref.on_get_pc();
            before(pc);
            if (pc < 0x4000) {
                seen_[pc] = 1;
                if (entry_flag_[pc]) ++entry_count_[pc];
            }
            ref.on_step();
            if (ref.ticks_in_frame() - t0 >= Machine::cycles_per_frame) break;
        }
        const uint32_t ref_cycles = ref.ticks_in_frame() - t0;
        ++ref.frame;
        ref.bus.set_frame(ref.frame);
        ref.deliver_vblank_interrupt();
        ++frames_;
        // the variants: the real game loop, then the comparison
        for (Variant &v : variants_) {
            if (v.bad >= 0) continue;
            Machine &m = *v.m;
            if (v.garbage) rules::poison = (0x9E3779B9u ^ (uint32_t)frames_) | 1;
            m.run_frame();
            rules::poison = 0;
            ++v.frames_compared;
            // the reference ran its interrupt after the frame; the variant too (run_frame does): the I/O log and the registers are compared as they stand now
            const char *w = differ(ref, m, false);
            if (!w && ref_cycles != m.ticks_in_frame()) w = "cycle count of the frame";
            if (w) {
                v.bad = (long)frames_ - 1;
                v.why = w;
            } else if (v.reg_bad < 0 && differ(ref, m, true)) {
                v.reg_bad = (long)frames_ - 1;
                if (strict_registers) {
                    v.bad = v.reg_bad;
                    v.why = "CPU registers";
                }
            }
        }
        last_ref_ = &ref;
        last_frame_ = ref.frame;
        snap_ = take(ref);
    }

    // ----- results ------------------------------------------------------------------------------------------------------------------------------------
    uint64_t frames() const { return frames_; }
    bool all_identical() const {
        for (const Variant &v : variants_)
            if (v.bad >= 0) return false;
        return !variants_.empty();
    }
    long first_difference_frame(size_t i) const { return variants_[i].bad; }
    const char *first_difference_what(size_t i) const { return variants_[i].why; }
    long first_register_difference_frame(size_t i) const { return variants_[i].reg_bad; }
    uint64_t entries(uint16_t pc) const { return pc < 0x4000 ? entry_count_[pc] : 0; }
    size_t variant_count() const { return variants_.size(); }
    // true when every variant has stopped (a difference was found in each): the player can end early
    bool all_stopped() const {
        for (const Variant &v : variants_)
            if (v.bad < 0) return false;
        return !variants_.empty();
    }

    // The report. Stable format, quoted by the evidence files of later steps:
    //   LOCKSTEP <frames> frames compared; <n> variants
    //   VARIANT <name>: IDENTICAL on every frame | DIFFERENT at frame F in <what>
    //     registers: identical on every frame | first difference at frame F
    //   PROGRESS <name>: routines replaced R of M (instructions I of T = x percent); entered in this run E; calls C
    //   REFERENCE distinct ROM instruction addresses run: D
    void report(FILE *o) const {
        std::fprintf(o, "LOCKSTEP %llu frames compared; %zu variant(s)\n", (unsigned long long)frames_, variants_.size());
        for (const Variant &v : variants_) {
            if (v.bad < 0) std::fprintf(o, "VARIANT %s: IDENTICAL on every frame (%llu frames: all memory, I/O writes, cycle count of every frame)%s\n", v.name.c_str(),
                                        (unsigned long long)v.frames_compared, v.garbage ? "; garbage in the scratch registers after every call" : "");
            else std::fprintf(o, "VARIANT %s: DIFFERENT at frame %ld in %s\n", v.name.c_str(), v.bad, v.why);
            if (v.reg_bad < 0) std::fprintf(o, "  registers at the end of every frame: identical too\n");
            else std::fprintf(o, "  registers at the end of a frame: first difference at frame %ld%s\n", v.reg_bad, strict_registers ? "" : " (not counted as a failure)");
            // progress meter
            unsigned replaced = 0, entered = 0;
            unsigned long long calls = 0, instr = 0;
            for (unsigned a = 0; v.rules && a < 0x4000; ++a)
                if (v.rules[a]) {
                    ++replaced;
                    if (entry_count_[a]) ++entered;
                    calls += entry_count_[a];
                    auto it = map_.find(a);
                    if (it != map_.end()) instr += it->second;
                }
            std::fprintf(o, "PROGRESS %s: routines replaced %u of %zu in the map (instructions %llu of %llu = %.2f percent); entered in this run %u; calls %llu\n", v.name.c_str(),
                         replaced, map_.size(), instr, (unsigned long long)map_total_, map_total_ ? 100.0 * (double)instr / (double)map_total_ : 0.0, entered, calls);
        }
        unsigned d = 0;
        for (uint8_t s : seen_) d += s;
        std::fprintf(o, "REFERENCE distinct ROM instruction addresses run: %u\n", d);
    }

private:
    struct Snap {
        std::vector<uint8_t> ram;
    };
    static Snap take(const Machine &m) {
        Snap s;
        const Bus &b = m.bus;
        s.ram.insert(s.ram.end(), b.workram, b.workram + sizeof b.workram);
        s.ram.insert(s.ram.end(), b.videoram, b.videoram + sizeof b.videoram);
        s.ram.insert(s.ram.end(), b.colorram, b.colorram + sizeof b.colorram);
        s.ram.insert(s.ram.end(), b.spriteram, b.spriteram + sizeof b.spriteram);
        return s;
    }

    void start_variants(Machine &ref) {
        (void)ref;
        for (Variant &v : variants_) {
            v.m.reset(new Machine(prog_));
            v.m->rules = v.rules;
        }
        snap_ = take(init_); // a fresh machine's RAM: the player's first pokes are found against it
    }

    void poke_variants(Machine &ref) {
        const Snap now = take(ref);
        for (Variant &v : variants_) {
            if (v.bad >= 0 || !v.m) continue;
            Bus &b = v.m->bus;
            b.in0 = ref.bus.in0;
            b.in1 = ref.bus.in1;
            b.dsw1 = ref.bus.dsw1;
            b.dsw2 = ref.bus.dsw2;
            size_t base = 0;
            uint8_t *regions[4] = {b.workram, b.videoram, b.colorram, b.spriteram};
            const size_t sizes[4] = {sizeof b.workram, sizeof b.videoram, sizeof b.colorram, sizeof b.spriteram};
            for (int r = 0; r < 4; ++r) {
                for (size_t i = 0; i < sizes[r]; ++i)
                    if (now.ram[base + i] != snap_.ram[base + i]) regions[r][i] = now.ram[base + i];
                base += sizes[r];
            }
        }
    }

    std::vector<uint8_t> prog_;
    Machine init_;
    Snap snap_;
    std::vector<Variant> variants_;
    std::vector<uint8_t> entry_flag_, seen_;
    std::vector<uint64_t> entry_count_;
    std::map<unsigned, unsigned> map_;
    unsigned long long map_total_ = 0;
    Machine *last_ref_ = nullptr;
    uint32_t last_frame_ = 0;
    uint64_t frames_ = 0;
};

inline Harness *g_harness = nullptr; // when set, probe::run_frame below runs the reference frame and compares the variants

} // namespace lockstep

namespace lockstep {
// N12 additions (tests only):
//  g_pc_probe: called with the machine and the program counter before every instruction of every frame run through probe::run_frame (the reference
//              machine when a harness is installed). Used to catch the real entry states of ROM routines and to count executed addresses.
//  g_force_rules: when set (and no harness is installed), probe::run_frame runs the frame like Machine::run_frame with this rules table, whatever
//              the machine's own `rules` is (the coverage players build their own machines): the progress meter of N12 uses it.
inline void (*g_pc_probe)(Machine &, uint16_t) = nullptr;
inline const Machine::RuleHook *g_force_rules = nullptr;
//  g_frame_done (N49a): called with the machine after every frame run through probe::run_frame (the frame fingerprints of tests/test_native_machine.cpp).
inline void (*g_frame_done)(Machine &) = nullptr;
} // namespace lockstep

namespace probe {
template <class Before> inline void run_frame_inner(Machine &m, Before &&before) {
    auto probed = [&](uint16_t pc) {
        if (lockstep::g_pc_probe) lockstep::g_pc_probe(m, pc);
        before(pc);
    };
    if (lockstep::g_harness) return lockstep::g_harness->frame(m, probed);
    if (lockstep::g_force_rules) {
        m.bus.io_log.clear();
        m.set_ticks_in_frame(0); // like Machine::run_frame: the hooks check how much of the frame is left
        for (;;) {
            const uint16_t pc = (uint16_t)m.on_get_pc();
            probed(pc);
            if (lockstep::g_force_rules[pc]) lockstep::g_force_rules[pc](m);
            else m.on_step();
            if (m.ticks_in_frame() >= Machine::cycles_per_frame) break;
        }
        ++m.frame;
        m.bus.set_frame(m.frame);
        m.deliver_vblank_interrupt();
        return;
    }
    return run_frame_plain(m, probed);
}
template <class Before> inline void run_frame(Machine &m, Before &&before) {
    run_frame_inner(m, before);
    if (lockstep::g_frame_done) lockstep::g_frame_done(m);
}
} // namespace probe
