// N06: timing study. Runs the four coverage players (A, B, C, D of tests/coverage_player.h) and measures, frame by frame, what the machine does at the
// moment the vertical-blank interrupt arrives: where the program counter is (the idle polling loop of the task queue, or inside a running task), how many
// cycles the frame really has, what the interrupt handler costs, how deep the task queue and the timed-event table are, and what the stack holds.
// FOR TESTS ONLY. The frame loop below is the same as probe::run_frame (tests/logic_probe.h) with a measuring hook added; the players call it unchanged.
// Output: addresses, counts and cycle numbers only (no ROM bytes).
// Usage: timing_study PROG.BIN ROUTINES.md OUT.txt [framesA framesB framesC framesD]   (routine map: docs/notes/rom_routines.md)
#define run_frame run_frame_plain // keep the original template of logic_probe.h out of the way; ours is defined below
#include "logic_probe.h"
#undef run_frame

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace study {

constexpr unsigned POLL_FIRST = 0x238D, POLL_LAST = 0x2392; // the task queue polling loop
constexpr unsigned GLUE_FIRST = 0x2395, GLUE_LAST = 0x23A7; // take the task off the queue and jump into it
constexpr unsigned HANDLER = 0x008D;                         // interrupt handler entry

struct PcInfo {
    uint64_t cyc_handler = 0, cyc_task = 0; // cycles spent at this address (instruction cost added up over the whole run)
    uint32_t last_frame = 0xFFFFFFFFu, last_t = 0;
    uint32_t turn_min = 0xFFFFFFFFu, turn_max = 0, turn_n = 0; // cycles between two runs of this address inside one frame (loop turn)
};

struct Landing {
    uint64_t frames = 0;
    std::map<int, uint64_t> tasks; // task id -> frames
    uint32_t turn_min = 0xFFFFFFFFu, turn_max = 0;
    uint64_t same_frame_prev = 0;
    uint64_t by_player[4] = {0, 0, 0, 0};
};

struct Totals {
    uint64_t frames = 0, delivered = 0, not_delivered = 0, halted = 0;
    uint64_t poll = 0, glue = 0, mid = 0, other = 0;
    std::map<unsigned, uint64_t> poll_pc;                 // landing address in the polling loop
    std::map<unsigned, Landing> mid_places;               // landing address inside a task
    std::map<unsigned, uint64_t> glue_pc;
    std::map<unsigned, uint64_t> overshoot;               // cycles beyond 50,688 at the end of the frame
    uint32_t frame_min = 0xFFFFFFFFu, frame_max = 0;
    uint64_t frame_sum = 0;
    uint32_t h_min = 0xFFFFFFFFu, h_max = 0;
    uint64_t h_sum = 0, h_n = 0, h_cut = 0;               // handler cost; frames where the handler did not finish (should be 0)
    std::map<unsigned, uint64_t> qdepth_land, qdepth_poll; // task queue depth at the interrupt / at the polling loop
    std::map<unsigned, uint64_t> slots_land;               // busy timed-event slots at the interrupt
    unsigned qdepth_max = 0, slots_max = 0;
    uint64_t run_len_hist[8] = {0};                       // runs of consecutive mid-task frames: 1, 2, 3, 4, 5-8, 9-16, 17-64, longer
    unsigned run_len_max = 0;
    std::set<std::string> cut_pcs, halt_pcs;
    std::set<std::string> regs_seen;                      // distinct pushed register sets at the interrupt (AF BC DE HL IX IY + SP)
    std::set<std::string> regs_seen_mid;
    uint64_t sp_min = 0xFFFF, sp_max = 0;
    uint64_t per_player_frames[4] = {0, 0, 0, 0};
    uint64_t per_player_mid[4] = {0, 0, 0, 0};
    int player = 0;
} T;

std::vector<PcInfo> info(0x10000);
uint32_t gframe = 0;
int last_task = -1;
unsigned run_len = 0;

unsigned qdepth(Machine &m) {
    const unsigned w = probe::rd16(m, 0x4C80), r = probe::rd16(m, 0x4C82);
    return ((w - r) & 0x3F) / 2;
}
unsigned busy_slots(Machine &m) {
    unsigned n = 0;
    for (unsigned i = 0; i < 16; ++i) n += probe::rd(m, 0x4C90 + 3 * i) != 0;
    return n;
}
void note_run(unsigned len) {
    if (!len) return;
    int b = len <= 4 ? (int)len - 1 : len <= 8 ? 4 : len <= 16 ? 5 : len <= 64 ? 6 : 7;
    ++T.run_len_hist[b];
    T.run_len_max = std::max(T.run_len_max, len);
}

} // namespace study

// The measuring frame loop (same as probe::run_frame, hook added). The players call probe::run_frame(m, callback); this is the version they get.
namespace probe {
template <class Before> inline void run_frame(Machine &m, Before &&before) {
    using namespace study;
    m.bus.io_log.clear();
    const uint32_t t0 = m.ticks_in_frame();
    ++gframe;
    bool in_handler = false;
    uint16_t entry_sp = 0;
    uint32_t h_start = 0;
    bool h_done = false;
    for (;;) {
        const uint16_t pc = (uint16_t)m.on_get_pc();
        before(pc);
        const uint32_t tb = m.ticks_in_frame() - t0;
        PcInfo &pi = info[pc];
        if (pi.last_frame == gframe) {
            const uint32_t d = tb - pi.last_t;
            if (d < pi.turn_min) pi.turn_min = d;
            if (d > pi.turn_max) pi.turn_max = d;
            ++pi.turn_n;
        }
        pi.last_frame = gframe;
        pi.last_t = tb;
        if (pc == HANDLER && !h_done) {
            in_handler = true;
            entry_sp = (uint16_t)m.on_get_sp();
            h_start = tb;
        }
        if (pc == 0x23A7) last_task = (int)(m.on_get_af() >> 8); // RST 20h with the task number in A
        if (pc == 0x238D) {
            const unsigned q = qdepth(m);
            ++T.qdepth_poll[q];
        }
        const bool was_handler = in_handler;
        m.on_step();
        const uint32_t ta = m.ticks_in_frame() - t0;
        if (was_handler) pi.cyc_handler += ta - tb;
        else pi.cyc_task += ta - tb;
        if (in_handler && (uint16_t)m.on_get_sp() == (uint16_t)(entry_sp + 2) && pc != HANDLER) {
            in_handler = false;
            h_done = true;
            const uint32_t hc = ta - h_start;
            T.h_min = std::min(T.h_min, hc);
            T.h_max = std::max(T.h_max, hc);
            T.h_sum += hc;
            ++T.h_n;
        }
        if (ta >= Machine::cycles_per_frame) break;
    }
    const uint32_t total = m.ticks_in_frame() - t0;
    if (in_handler) {
        ++T.h_cut;
        char b[16];
        std::snprintf(b, sizeof b, "%04X", (unsigned)m.on_get_pc());
        T.cut_pcs.insert(b);
    }
    ++T.frames;
    ++T.per_player_frames[T.player];
    T.frame_min = std::min(T.frame_min, total);
    T.frame_max = std::max(T.frame_max, total);
    T.frame_sum += total;
    ++T.overshoot[total - Machine::cycles_per_frame];

    // the moment of the interrupt
    const unsigned lpc = m.on_get_pc();
    const bool delivered = m.bus.irq_enabled() && m.on_get_iff1() && !m.on_is_int_disabled();
    bool mid = false;
    if (!delivered) {
        ++T.not_delivered;
    } else {
        ++T.delivered;
        char rb[96];
        std::snprintf(rb, sizeof rb, "%04X %04X %04X %04X %04X %04X", (unsigned)m.on_get_af(), (unsigned)m.on_get_bc(), (unsigned)m.on_get_de(), (unsigned)m.on_get_hl(),
                      (unsigned)m.on_get_ix(), (unsigned)m.on_get_iy());
        T.regs_seen.insert(rb);
        if (!m.on_is_halted()) {
            T.sp_min = std::min<uint64_t>(T.sp_min, m.on_get_sp());
            T.sp_max = std::max<uint64_t>(T.sp_max, m.on_get_sp());
        }
        if (m.on_is_halted()) {
            char b[16];
            std::snprintf(b, sizeof b, "%04X", lpc);
            T.halt_pcs.insert(b);
        }
        const unsigned q = qdepth(m), sl = busy_slots(m);
        ++T.qdepth_land[q];
        ++T.slots_land[sl];
        T.qdepth_max = std::max(T.qdepth_max, q);
        T.slots_max = std::max(T.slots_max, sl);
        if (m.on_is_halted()) {
            ++T.halted;
        } else if (lpc >= POLL_FIRST && lpc <= POLL_LAST) {
            ++T.poll;
            ++T.poll_pc[lpc];
        } else if (lpc >= GLUE_FIRST && lpc <= GLUE_LAST) {
            ++T.glue;
            ++T.glue_pc[lpc];
        } else if (lpc < 0x4000) {
            ++T.mid;
            ++T.per_player_mid[T.player];
            mid = true;
            T.regs_seen_mid.insert(rb);
            Landing &L = T.mid_places[lpc];
            ++L.frames;
            ++L.by_player[T.player];
            ++L.tasks[last_task];
            const PcInfo &p = info[lpc];
            if (p.last_frame == gframe) {
                ++L.same_frame_prev;
                const uint32_t d = total - p.last_t;
                L.turn_min = std::min(L.turn_min, d);
                L.turn_max = std::max(L.turn_max, d);
            }
        } else {
            ++T.other;
        }
    }
    if (mid) ++run_len;
    else {
        note_run(run_len);
        run_len = 0;
    }
    ++m.frame;
    m.bus.set_frame(m.frame);
    m.deliver_vblank_interrupt();
}
} // namespace probe

#include "coverage_player.h"

namespace {

struct Routine {
    unsigned entry;
    std::string kind;
};
std::vector<Routine> load_routines(const char *path) {
    std::vector<Routine> r;
    std::ifstream f(path);
    std::string line;
    bool in_routines = false;
    while (std::getline(f, line)) {
        if (line.rfind("## ", 0) == 0) in_routines = line.rfind("## Routines", 0) == 0;
        if (!in_routines) continue;
        if (line.size() < 8 || line[0] != '|' || line[1] != ' ') continue;
        char *end = nullptr;
        const unsigned long a = std::strtoul(line.c_str() + 2, &end, 16);
        if (!end || end != line.c_str() + 6 || *end != ' ') continue;
        const size_t k1 = line.find('|', 6);
        const size_t k2 = line.find('|', k1 + 1);
        if (k1 == std::string::npos || k2 == std::string::npos) continue;
        std::string kind = line.substr(k1 + 2, k2 - k1 - 3);
        r.push_back({(unsigned)a, kind});
    }
    std::sort(r.begin(), r.end(), [](const Routine &a, const Routine &b) { return a.entry < b.entry; });
    return r;
}
unsigned routine_of(const std::vector<Routine> &r, unsigned pc) {
    unsigned best = 0;
    for (const Routine &x : r) {
        if (x.entry <= pc) best = x.entry;
        else break;
    }
    return best;
}
const char *kind_of(const std::vector<Routine> &r, unsigned entry) {
    for (const Routine &x : r)
        if (x.entry == entry) return x.kind.c_str();
    return "?";
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: timing_study PROG.BIN ROUTINES.md OUT.txt [framesA framesB framesC framesD]\n");
        return 2;
    }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::fclose(f);
    const std::vector<Routine> routines = load_routines(argv[2]);
    if (routines.size() < 200) {
        std::fprintf(stderr, "cannot read the routine map %s\n", argv[2]);
        return 2;
    }
    unsigned long fr[4] = {300000, 300000, 300000, 54000};
    for (int i = 0; i < 4 && 4 + i < argc; ++i) fr[i] = std::strtoul(argv[4 + i], nullptr, 10);

    using namespace study;
    const char *names[4] = {"A", "B", "C", "D"};
    {
        T.player = 0;
        static Machine m(prog);
        covplayer::run(m, fr[0], nullptr);
        note_run(run_len);
        run_len = 0;
    }
    T.player = 1;
    covplayer::run_b(prog, fr[1], nullptr);
    note_run(run_len);
    run_len = 0;
    T.player = 2;
    covplayer::run_c(prog, fr[2], nullptr);
    note_run(run_len);
    run_len = 0;
    T.player = 3;
    covplayer::run_d(prog, fr[3], nullptr);
    note_run(run_len);
    run_len = 0;

    FILE *o = std::fopen(argv[3], "wb");
    if (!o) return 2;
    auto pct = [](uint64_t a, uint64_t b) { return b ? 100.0 * (double)a / (double)b : 0.0; };
    std::fprintf(o, "N06 timing study (tests/timing_study.cpp). Players A, B, C, D of tests/coverage_player.h; addresses, counts and cycles only.\n\n");
    std::fprintf(o, "== 1. Frames\n");
    for (int i = 0; i < 4; ++i)
        std::fprintf(o, "player %s: %llu frames, %llu with the interrupt inside a task (%.2f percent)\n", names[i], (unsigned long long)T.per_player_frames[i],
                     (unsigned long long)T.per_player_mid[i], pct(T.per_player_mid[i], T.per_player_frames[i]));
    std::fprintf(o, "all: %llu frames; interrupt delivered in %llu, not delivered (interrupts off: power-on test and start-up) in %llu\n", (unsigned long long)T.frames,
                 (unsigned long long)T.delivered, (unsigned long long)T.not_delivered);
    std::fprintf(o, "where the program counter is when the interrupt arrives (of the %llu delivered):\n", (unsigned long long)T.delivered);
    std::fprintf(o, "  task queue polling loop 238D-2392 : %llu (%.2f percent)\n", (unsigned long long)T.poll, pct(T.poll, T.delivered));
    std::fprintf(o, "  taking a task off the queue 2395-23A7 : %llu (%.2f percent)\n", (unsigned long long)T.glue, pct(T.glue, T.delivered));
    std::fprintf(o, "  INSIDE A RUNNING TASK (any other ROM address) : %llu (%.2f percent of delivered; %.2f percent of all frames)\n", (unsigned long long)T.mid,
                 pct(T.mid, T.delivered), pct(T.mid, T.frames));
    std::fprintf(o, "  halted : %llu (at:%s); outside the ROM : %llu\n", (unsigned long long)T.halted, [&] { std::string r; for (auto &x : T.halt_pcs) r += " " + x; return r; }().c_str(), (unsigned long long)T.other);
    std::fprintf(o, "polling loop: one turn (238D to its jump back) costs %u cycles (shortest repeat of 238D inside a frame)\n", info[POLL_FIRST].turn_min);
    std::fprintf(o, "polling loop, address where the interrupt lands:\n");
    for (auto &p : T.poll_pc) std::fprintf(o, "  %04X : %llu\n", p.first, (unsigned long long)p.second);
    if (!T.glue_pc.empty()) {
        std::fprintf(o, "task take-off glue, address where the interrupt lands:\n");
        for (auto &p : T.glue_pc) std::fprintf(o, "  %04X : %llu\n", p.first, (unsigned long long)p.second);
    }
    std::fprintf(o, "runs of consecutive frames with the interrupt inside a task: 1 frame %llu, 2 %llu, 3 %llu, 4 %llu, 5-8 %llu, 9-16 %llu, 17-64 %llu, more %llu; longest %u frames\n",
                 (unsigned long long)T.run_len_hist[0], (unsigned long long)T.run_len_hist[1], (unsigned long long)T.run_len_hist[2], (unsigned long long)T.run_len_hist[3],
                 (unsigned long long)T.run_len_hist[4], (unsigned long long)T.run_len_hist[5], (unsigned long long)T.run_len_hist[6], (unsigned long long)T.run_len_hist[7],
                 T.run_len_max);

    std::fprintf(o, "\n== 2. Length of a frame in cycles (the frame ends at the first instruction boundary at or after 50,688 cycles)\n");
    std::fprintf(o, "shortest %u, longest %u, mean %.3f\n", T.frame_min, T.frame_max, (double)T.frame_sum / (double)T.frames);
    std::fprintf(o, "cycles beyond 50,688 (the instruction that crosses the end of the frame; the excess is not carried into the next frame):\n");
    for (auto &p : T.overshoot) std::fprintf(o, "  +%u : %llu frames (%.2f percent)\n", p.first, (unsigned long long)p.second, pct(p.second, T.frames));

    std::fprintf(o, "\n== 3. Cost of the interrupt handler (008D to its RET, in every frame in which it ran)\n");
    std::fprintf(o, "frames measured %llu; cheapest %u, dearest %u, mean %.1f cycles (%.2f percent of the frame); handler not finished at the end of a frame: %llu (stopped at:%s)\n",
                 (unsigned long long)T.h_n, T.h_min, T.h_max, T.h_n ? (double)T.h_sum / (double)T.h_n : 0.0, T.h_n ? 100.0 * ((double)T.h_sum / (double)T.h_n) / 50688.0 : 0.0,
                 (unsigned long long)T.h_cut, [&] { std::string r; for (auto &x : T.cut_pcs) r += " " + x; return r; }().c_str());

    std::fprintf(o, "\n== 4. Task queue depth (tasks waiting, of 32 places) and busy timed-event slots (of 16)\n");
    std::fprintf(o, "at the interrupt, queue depth: ");
    for (auto &p : T.qdepth_land) std::fprintf(o, "%u:%llu ", p.first, (unsigned long long)p.second);
    std::fprintf(o, "\n  deepest at an interrupt %u; seen at the top of the polling loop: ", T.qdepth_max);
    for (auto &p : T.qdepth_poll) std::fprintf(o, "%u:%llu ", p.first, (unsigned long long)p.second);
    std::fprintf(o, "\nat the interrupt, busy timed-event slots: ");
    for (auto &p : T.slots_land) std::fprintf(o, "%u:%llu ", p.first, (unsigned long long)p.second);
    std::fprintf(o, "\n  most at an interrupt %u\n", T.slots_max);

    std::fprintf(o, "\n== 5. What the interrupt saves on the stack (it shows in the RAM hash: the stack lies inside the hashed work RAM)\n");
    std::fprintf(o, "stack pointer at the interrupt: lowest %04llX, highest %04llX\n", (unsigned long long)T.sp_min, (unsigned long long)T.sp_max);
    std::fprintf(o, "distinct register sets (AF BC DE HL IX IY) at the interrupt: %zu in all, %zu among the frames with the interrupt inside a task\n", T.regs_seen.size(),
                 T.regs_seen_mid.size());

    std::fprintf(o, "\n== 6. Every place where the interrupt arrived inside a running task\n");
    std::fprintf(o, "address | routine entry | routine kind | frames (players A/B/C/D) | task numbers in hexadecimal (frames) | cycles since this address last ran in the same frame, at the interrupt (least-most; '-' = it had not run before in the frame) | shortest repeat of this address anywhere in the frame in the whole run | verdict\n");
    std::map<unsigned, uint64_t> by_routine;
    unsigned loops = 0;
    for (auto &p : T.mid_places) {
        const Landing &L = p.second;
        const unsigned re = routine_of(routines, p.first);
        by_routine[re] += L.frames;
        std::string ts;
        for (auto &t : L.tasks) {
            char b[40];
            if (t.first < 0) std::snprintf(b, sizeof b, "none(%llu) ", (unsigned long long)t.second);
            else std::snprintf(b, sizeof b, "%02X(%llu) ", t.first, (unsigned long long)t.second);
            ts += b;
        }
        const PcInfo &pi = info[p.first];
        char gap[48], rep[48];
        if (L.same_frame_prev) std::snprintf(gap, sizeof gap, "%u-%u", L.turn_min, L.turn_max);
        else std::snprintf(gap, sizeof gap, "-");
        if (pi.turn_n) std::snprintf(rep, sizeof rep, "%u", pi.turn_min);
        else std::snprintf(rep, sizeof rep, "-");
        const char *verdict = !L.same_frame_prev ? "first pass in the frame" : (L.turn_min == pi.turn_min && pi.turn_min <= 3000) ? "LOOP: one turn costs the shortest repeat" : "repeat after other work";
        if (L.same_frame_prev && L.turn_min == pi.turn_min && pi.turn_min <= 3000) ++loops;
        char pl[80];
        std::snprintf(pl, sizeof pl, "%llu (%llu/%llu/%llu/%llu)", (unsigned long long)L.frames, (unsigned long long)L.by_player[0], (unsigned long long)L.by_player[1],
                      (unsigned long long)L.by_player[2], (unsigned long long)L.by_player[3]);
        std::fprintf(o, "%04X | %04X | %s | %s | %s| %s | %s | %s\n", p.first, re, kind_of(routines, re), pl, ts.c_str(), gap, rep, verdict);
    }
    std::fprintf(o, "places: %zu; routines they lie in: %zu; places that are inside a loop with a measured turn cost: %u\n", T.mid_places.size(), by_routine.size(), loops);
    std::fprintf(o, "by routine (entry : frames):");
    for (auto &p : by_routine) std::fprintf(o, " %04X:%llu", p.first, (unsigned long long)p.second);
    std::fprintf(o, "\n");

    // cycles per routine over the whole run
    std::map<unsigned, std::pair<uint64_t, uint64_t>> cyc; // routine -> (handler cycles, task cycles)
    uint64_t th = 0, tt = 0;
    for (unsigned a = 0; a < 0x4000; ++a) {
        if (!info[a].cyc_handler && !info[a].cyc_task) continue;
        auto &c = cyc[routine_of(routines, a)];
        c.first += info[a].cyc_handler;
        c.second += info[a].cyc_task;
        th += info[a].cyc_handler;
        tt += info[a].cyc_task;
    }
    std::fprintf(o, "\n== 7. Where the cycles go (exclusive: a routine's own instructions, not the routines it calls), whole run\n");
    std::fprintf(o, "inside the interrupt handler and what it calls: %llu cycles; outside it (tasks, polling loop, start-up): %llu cycles\n", (unsigned long long)th, (unsigned long long)tt);
    std::vector<std::pair<unsigned, std::pair<uint64_t, uint64_t>>> v(cyc.begin(), cyc.end());
    std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.second.second > b.second.second; });
    std::fprintf(o, "routines outside the handler, most cycles first (entry : cycles : share of all frames' cycles):\n");
    for (size_t i = 0; i < v.size() && i < 16; ++i)
        std::fprintf(o, "  %04X : %llu : %.3f percent\n", v[i].first, (unsigned long long)v[i].second.second, pct(v[i].second.second, T.frames * 50688ull));
    std::sort(v.begin(), v.end(), [](auto &a, auto &b) { return a.second.first > b.second.first; });
    std::fprintf(o, "routines inside the handler, most cycles first (entry : cycles : mean per frame):\n");
    for (size_t i = 0; i < v.size() && i < 12; ++i)
        if (v[i].second.first) std::fprintf(o, "  %04X : %llu : %.1f\n", v[i].first, (unsigned long long)v[i].second.first, (double)v[i].second.first / (double)T.h_n);
    std::fclose(o);

    std::printf("timing study: %llu frames, %llu with the interrupt inside a task (%.2f percent), %zu places; written to %s\n", (unsigned long long)T.frames, (unsigned long long)T.mid,
                pct(T.mid, T.frames), T.mid_places.size(), argv[3]);
    return 0;
}
