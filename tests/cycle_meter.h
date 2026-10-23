// N07: the cycle meter. FOR TESTS ONLY. Measures, for any ROM routine, the exact Z80 cycles and the number of instructions from its first instruction to the
// RET that returns from it (callees included), in every call of a run, and looks for a formula (a constant, or a + b * a register, or a + b * x + c * y).
// Use: put meter::Meter into the frame loop of a test (tests/logic_probe.h has the same loop without the meter):
//     meter::Meter mt;  mt.watch(0x2A23, "random number");  mt.watch(0x0008, "fill");
//     for each frame:   mt.run_frame(machine, [&](uint16_t pc) { /* the player's own per-instruction callback, may be empty */ });
//     mt.report(stdout);
// A call that is cut by the end of a frame (the interrupt handler runs in between) is counted as "interrupted" and left out of the numbers. A routine
// that does not end with RET (RST 20h, 28h, 30h end with a jump) is reported as "never returned"; give such a routine an end address with watch_to().
// The report format is stable: later step evidence files (N08 onwards) quote these lines. Addresses, counts and cycle numbers only; no ROM bytes.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "logic_probe.h"

namespace meter {

struct Feature {
    std::string name;
    std::function<int(Machine &)> get;
};

// one call, as seen at the first instruction and at the return
struct Sample {
    std::vector<int> feats; // the feature values at entry
    int cycles = 0, instr = 0;
};

// a formula  y = a + sum(coef[i] * feature[idx[i]])  that is exactly right for every call
struct Formula {
    bool found = false;
    int a = 0;
    std::vector<int> idx, coef;
};

// Fit y = a + b*x (+ c*z) over distinct (features, y) rows with their counts. Exact integer arithmetic on the check; least squares only proposes the numbers.
inline Formula fit(const std::vector<std::vector<int>> &feats, const std::vector<int> &y, const std::vector<std::string> &) {
    Formula f;
    const size_t n = y.size();
    if (!n) return f;
    bool constant = true;
    for (size_t i = 1; i < n; ++i) constant = constant && y[i] == y[0];
    if (constant) {
        f.found = true;
        f.a = y[0];
        return f;
    }
    const size_t nf = feats[0].size();
    auto check = [&](int a, const std::vector<int> &idx, const std::vector<int> &coef) {
        for (size_t i = 0; i < n; ++i) {
            long v = a;
            for (size_t k = 0; k < idx.size(); ++k) v += (long)coef[k] * feats[i][idx[k]];
            if (v != y[i]) return false;
        }
        return true;
    };
    // one feature
    for (size_t j = 0; j < nf; ++j) {
        // two rows with different feature values give b
        size_t r0 = 0, r1 = n;
        for (size_t i = 1; i < n; ++i)
            if (feats[i][j] != feats[0][j]) {
                r1 = i;
                break;
            }
        if (r1 == n) continue;
        const int dy = y[r1] - y[r0], dx = feats[r1][j] - feats[r0][j];
        if (dy % dx) continue;
        const int b = dy / dx, a = y[r0] - b * feats[r0][j];
        if (check(a, {(int)j}, {b})) {
            f.found = true;
            f.a = a;
            f.idx = {(int)j};
            f.coef = {b};
            return f;
        }
    }
    // two features: least squares on the normal equations, rounded, then the exact check
    for (size_t j = 0; j < nf; ++j)
        for (size_t k = j + 1; k < nf; ++k) {
            double s[3][4] = {{0}};
            for (size_t i = 0; i < n; ++i) {
                const double v[3] = {1.0, (double)feats[i][j], (double)feats[i][k]};
                for (int p = 0; p < 3; ++p) {
                    for (int q = 0; q < 3; ++q) s[p][q] += v[p] * v[q];
                    s[p][3] += v[p] * y[i];
                }
            }
            bool ok = true;
            for (int c = 0; c < 3 && ok; ++c) { // Gauss elimination with pivoting
                int piv = c;
                for (int r = c + 1; r < 3; ++r)
                    if (std::fabs(s[r][c]) > std::fabs(s[piv][c])) piv = r;
                if (std::fabs(s[piv][c]) < 1e-9) {
                    ok = false;
                    break;
                }
                for (int q = 0; q < 4; ++q) std::swap(s[c][q], s[piv][q]);
                for (int r = 0; r < 3; ++r)
                    if (r != c) {
                        const double m = s[r][c] / s[c][c];
                        for (int q = c; q < 4; ++q) s[r][q] -= m * s[c][q];
                    }
            }
            if (!ok) continue;
            const int a = (int)std::lround(s[0][3] / s[0][0]), b = (int)std::lround(s[1][3] / s[1][1]), c = (int)std::lround(s[2][3] / s[2][2]);
            if (b == 0 || c == 0) continue;
            if (check(a, {(int)j, (int)k}, {b, c})) {
                f.found = true;
                f.a = a;
                f.idx = {(int)j, (int)k};
                f.coef = {b, c};
                return f;
            }
        }
    return f;
}

class Meter {
public:
    Meter() {
        auto reg8 = [&](const char *nm, std::function<int(Machine &)> g) { base_.push_back({nm, g}); };
        reg8("A", [](Machine &m) { return (int)(m.on_get_af() >> 8); });
        reg8("B", [](Machine &m) { return (int)(m.on_get_bc() >> 8); });
        reg8("C", [](Machine &m) { return (int)(m.on_get_bc() & 0xFF); });
        reg8("D", [](Machine &m) { return (int)(m.on_get_de() >> 8); });
        reg8("E", [](Machine &m) { return (int)(m.on_get_de() & 0xFF); });
        reg8("H", [](Machine &m) { return (int)(m.on_get_hl() >> 8); });
        reg8("L", [](Machine &m) { return (int)(m.on_get_hl() & 0xFF); });
        reg8("B(0=256)", [](Machine &m) { int b = (int)(m.on_get_bc() >> 8); return b ? b : 256; }); // DJNZ counts 0 as 256 turns
        reg8("C(0=256)", [](Machine &m) { int c = (int)(m.on_get_bc() & 0xFF); return c ? c : 256; });
        reg8("BC", [](Machine &m) { return (int)m.on_get_bc(); });
        reg8("DE", [](Machine &m) { return (int)m.on_get_de(); });
        reg8("HL", [](Machine &m) { return (int)m.on_get_hl(); });
    }

    // Watch the routine whose first instruction is at `entry`. `extras` are more numbers to look at when the routine starts (a RAM byte, say).
    void watch(uint16_t entry, const std::string &name, std::vector<Feature> extras = {}) { add(entry, name, 0, false, std::move(extras)); }
    // Watch a routine that does not end with RET: a call ends when the program counter reaches `end_pc` after an instruction.
    void watch_to(uint16_t entry, uint16_t end_pc, const std::string &name, std::vector<Feature> extras = {}) { add(entry, name, end_pc, true, std::move(extras)); }

    // The frame loop of tests/logic_probe.h with the meter added. `before(pc)` is the caller's own callback (called before every instruction).
    template <class Before> void run_frame(Machine &m, Before &&before) {
        m.bus.io_log.clear();
        const uint32_t t0 = m.ticks_in_frame();
        ++frame_no_;
        for (;;) {
            const uint16_t pc = (uint16_t)m.on_get_pc();
            before(pc);
            const uint32_t tb = m.ticks_in_frame() - t0;
            if (!watch_.empty()) {
                auto it = watch_.find(pc);
                if (it != watch_.end()) {
                    // the first instruction of a loop that jumps back to the entry is not a new call: same routine, same stack pointer
                    bool again = false;
                    for (const Active &a : active_) again = again || (a.entry == pc && a.sp == (uint16_t)m.on_get_sp());
                    if (!again) begin(m, it->second, tb);
                }
            }
            const bool ret_like = !active_.empty() && is_ret(m, pc);
            const uint8_t op0 = m.bus.read(pc);
            const bool prefix_only = op0 == 0xDD || op0 == 0xFD; // the core runs the index prefix as a step of its own; the instruction is counted once, at its opcode
            m.on_step();
            if (!active_.empty()) {
                if (!prefix_only)
                    for (Active &a : active_) ++a.instr;
                finish_calls(m, ret_like, (uint16_t)m.on_get_pc(), m.ticks_in_frame() - t0);
            }
            if (m.ticks_in_frame() - t0 >= Machine::cycles_per_frame) break;
        }
        ++m.frame;
        m.bus.set_frame(m.frame);
        m.deliver_vblank_interrupt();
    }

    // results ---------------------------------------------------------------------------------------------------------------------------------------
    struct Result {
        std::string name;
        uint16_t entry = 0;
        uint64_t calls = 0, interrupted = 0, left_without_ret = 0;
        std::vector<std::string> feat_names;
        std::map<std::vector<int>, uint64_t> rows; // (features..., cycles, instr) -> number of calls
    };
    const Result *result(uint16_t entry) const {
        auto it = res_.find(entry);
        return it == res_.end() ? nullptr : &it->second;
    }

    // Every call of the routine took exactly `cycles` cycles and `instr` instructions (and there was at least one call).
    bool is_constant(uint16_t entry, int cycles, int instr) const {
        const Result *r = result(entry);
        if (!r || r->rows.empty()) return false;
        for (auto &row : r->rows) {
            const size_t nf = row.first.size() - 2;
            if (row.first[nf] != cycles || row.first[nf + 1] != instr) return false;
        }
        return true;
    }
    uint64_t measured_calls(uint16_t entry) const {
        const Result *r = result(entry);
        uint64_t n = 0;
        if (r)
            for (auto &row : r->rows) n += row.second;
        return n;
    }

    // The report, one block per routine. Format (stable):
    //   METER <entry> <name>
    //     calls measured N; interrupted by the end of a frame M; left without RET K
    //     cycles: least L, most H, mean X; instructions: least l, most h
    //     distinct (cycles, instructions) pairs: P: c/i x n, ...      (the 12 most frequent)
    //     FORMULA cycles = ...  [exact for all N calls]   |   FORMULA cycles: none with at most two numbers
    //     FORMULA instructions = ...
    void report(FILE *o) const {
        for (auto &kv : res_) report_one(o, kv.second);
    }

private:
    struct Watch {
        uint16_t entry, end_pc;
        bool until;
        std::string name;
        std::vector<Feature> extras;
    };
    struct Active {
        uint16_t entry;
        uint16_t sp;
        uint32_t t_start;
        uint32_t frame;
        int instr;
        std::vector<int> feats;
    };

    std::vector<Feature> base_;
    std::map<uint16_t, Watch> watch_;
    std::map<uint16_t, Result> res_;
    std::vector<Active> active_;
    uint32_t frame_no_ = 0;

    void add(uint16_t entry, const std::string &name, uint16_t end_pc, bool until, std::vector<Feature> extras) {
        Watch w{entry, end_pc, until, name, std::move(extras)};
        Result r;
        r.name = name;
        r.entry = entry;
        for (auto &f : base_) r.feat_names.push_back(f.name);
        for (auto &f : w.extras) r.feat_names.push_back(f.name);
        res_[entry] = r;
        watch_[entry] = std::move(w);
    }

    void begin(Machine &m, const Watch &w, uint32_t tb) {
        const uint16_t sp = (uint16_t)m.on_get_sp();
        // calls that never came back (a jump out, a restart) lie deeper than this one on the stack: they are over
        while (!active_.empty() && active_.back().sp <= sp) {
            ++res_[active_.back().entry].left_without_ret;
            active_.pop_back();
        }
        Active a;
        a.entry = w.entry;
        a.sp = sp;
        a.t_start = tb;
        a.frame = frame_no_;
        a.instr = 0;
        for (auto &f : base_) a.feats.push_back(f.get(m));
        for (auto &f : w.extras) a.feats.push_back(f.get(m));
        active_.push_back(std::move(a));
    }

    static bool is_ret(Machine &m, uint16_t pc) {
        const uint8_t op = m.bus.read(pc);
        if (op == 0xC9 || (op & 0xC7) == 0xC0) return true;                         // RET, RET cc
        if (op == 0xED) {
            const uint8_t op2 = m.bus.read((uint16_t)(pc + 1));
            return op2 == 0x4D || op2 == 0x45;                                      // RETI, RETN
        }
        return false;
    }

    void finish_calls(Machine &m, bool ret_like, uint16_t pc_after, uint32_t t_after) {
        const uint16_t sp = (uint16_t)m.on_get_sp();
        while (!active_.empty()) {
            Active &a = active_.back();
            const Watch &w = watch_.at(a.entry);
            const bool done = w.until ? pc_after == w.end_pc : (ret_like && sp == (uint16_t)(a.sp + 2));
            if (!done) break;
            Result &r = res_[a.entry];
            if (a.frame != frame_no_) {
                ++r.interrupted;
            } else {
                std::vector<int> row = a.feats;
                row.push_back((int)(t_after - a.t_start));
                row.push_back(a.instr);
                ++r.rows[row];
                ++r.calls;
            }
            active_.pop_back();
        }
        // a call still open when the frame ends is cut by the interrupt: it is counted when it finishes (frame number differs)
    }

    static std::string fmt_formula(const Formula &f, const std::vector<std::string> &names, const char *y) {
        char b[200];
        if (!f.found) {
            std::snprintf(b, sizeof b, "%s: none with at most two numbers", y);
            return b;
        }
        std::string s = std::string(y) + " = " + std::to_string(f.a);
        for (size_t i = 0; i < f.idx.size(); ++i) {
            const int c = f.coef[i];
            s += (c < 0 ? " - " : " + ") + std::to_string(std::abs(c)) + " * " + names[f.idx[i]];
        }
        if (f.idx.empty()) s += " (constant)";
        return s;
    }

    void report_one(FILE *o, const Result &r) const {
        std::fprintf(o, "METER %04X %s\n", r.entry, r.name.c_str());
        uint64_t n = 0, sumc = 0;
        int cmin = 1 << 30, cmax = 0, imin = 1 << 30, imax = 0;
        std::map<std::pair<int, int>, uint64_t> pairs;
        std::vector<std::vector<int>> feats;
        std::vector<int> ycyc, yins;
        const size_t nf = r.feat_names.size();
        for (auto &row : r.rows) {
            const int c = row.first[nf], i = row.first[nf + 1];
            n += row.second;
            sumc += (uint64_t)c * row.second;
            cmin = std::min(cmin, c);
            cmax = std::max(cmax, c);
            imin = std::min(imin, i);
            imax = std::max(imax, i);
            pairs[{c, i}] += row.second;
            feats.push_back(std::vector<int>(row.first.begin(), row.first.begin() + nf));
            ycyc.push_back(c);
            yins.push_back(i);
        }
        std::fprintf(o, "  calls measured %llu; interrupted by the end of a frame %llu; left without RET %llu\n", (unsigned long long)n, (unsigned long long)r.interrupted,
                     (unsigned long long)r.left_without_ret);
        if (!n) {
            std::fprintf(o, "  no complete call was seen\n");
            return;
        }
        std::fprintf(o, "  cycles: least %d, most %d, mean %.3f; instructions: least %d, most %d\n", cmin, cmax, (double)sumc / (double)n, imin, imax);
        std::vector<std::pair<uint64_t, std::pair<int, int>>> pv;
        for (auto &p : pairs) pv.push_back({p.second, p.first});
        std::sort(pv.begin(), pv.end(), [](auto &a, auto &b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        std::fprintf(o, "  distinct (cycles/instructions) pairs: %zu:", pairs.size());
        for (size_t i = 0; i < pv.size() && i < 12; ++i) std::fprintf(o, " %d/%d x%llu", pv[i].second.first, pv[i].second.second, (unsigned long long)pv[i].first);
        std::fprintf(o, "\n");
        std::fprintf(o, "  FORMULA %s  [%s for all %llu calls]\n", fmt_formula(fit(feats, ycyc, r.feat_names), r.feat_names, "cycles").c_str(),
                     fit(feats, ycyc, r.feat_names).found ? "exact" : "not exact", (unsigned long long)n);
        std::fprintf(o, "  FORMULA %s  [%s for all %llu calls]\n", fmt_formula(fit(feats, yins, r.feat_names), r.feat_names, "instructions").c_str(),
                     fit(feats, yins, r.feat_names).found ? "exact" : "not exact", (unsigned long long)n);
    }
};

} // namespace meter
