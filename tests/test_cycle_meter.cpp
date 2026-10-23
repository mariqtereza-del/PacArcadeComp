// N07: test of the cycle meter (tests/cycle_meter.h). Plays coverage players A, B and C with the meter in the frame loop and checks:
//  1. the random number routine 2A23 takes exactly 111 cycles and 13 instructions in every call (the values the readable rule uses);
//  2. the fill routine 0008 follows an exact formula: cycles = 5 + 26 * B(0=256), instructions = 1 + 3 * B(0=256);
//  3. the meter does not change the game (the frame hashes of the run with the meter equal those of the run without it);
//  4. the formula finder works on made-up numbers (constant, one number, two numbers, no formula) and a wrong claim is caught;
//  5. a routine that ends with a jump (RST 28h) is reported as never returned, and one given an end address (watch_to) is measured up to that address.
// Usage: test_cycle_meter PROG.BIN FRAMES_A FRAMES_B REPORT.txt
#define run_frame run_frame_plain
#include "logic_probe.h"
#undef run_frame
#include "cycle_meter.h"

static meter::Meter *g_meter = nullptr;
namespace probe {
template <class Before> inline void run_frame(Machine &m, Before &&before) {
    if (g_meter) return g_meter->run_frame(m, before);
    return run_frame_plain(m, before);
}
} // namespace probe
#include "coverage_player.h"

static int g_fail = 0;
#define CHECK(c, msg) do { if (!(c)) { std::printf("FAIL: %s\n", msg); ++g_fail; } else std::printf("ok: %s\n", msg); } while (0)

int main(int argc, char **argv) {
    if (argc < 5) { std::fprintf(stderr, "usage: test_cycle_meter PROG.BIN FRAMES_A FRAMES_B REPORT.txt\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    std::fclose(f);
    const unsigned long fa = std::strtoul(argv[2], nullptr, 10), fb = std::strtoul(argv[3], nullptr, 10);

    // ---- 4. the formula finder on made-up numbers
    {
        std::vector<std::vector<int>> F;
        std::vector<int> y1, y2, y3, y4;
        for (int b = 1; b <= 20; ++b)
            for (int c = 0; c < 4; ++c) {
                F.push_back({b, c, (b * 7) % 5});
                y1.push_back(111);
                y2.push_back(5 + 26 * b);
                y3.push_back(9 + 26 * b + 3 * c);
                y4.push_back((b * b) ^ c);
            }
        const std::vector<std::string> nm = {"B", "C", "X"};
        auto a = meter::fit(F, y1, nm), b = meter::fit(F, y2, nm), c = meter::fit(F, y3, nm), d = meter::fit(F, y4, nm);
        CHECK(a.found && a.idx.empty() && a.a == 111, "formula finder: a constant");
        CHECK(b.found && b.idx.size() == 1 && b.idx[0] == 0 && b.a == 5 && b.coef[0] == 26, "formula finder: 5 + 26 * B");
        CHECK(c.found && c.idx.size() == 2 && c.a == 9 && c.coef[0] == 26 && c.coef[1] == 3, "formula finder: 9 + 26 * B + 3 * C");
        CHECK(!d.found, "formula finder: no formula for made-up numbers that have none");
        y2[17] += 1; // one call off by a cycle: no exact formula any more
        CHECK(!meter::fit(F, y2, nm).found, "formula finder: one wrong call is caught");
    }

    // ---- 1 to 3: the real machine
    meter::Meter mt;
    mt.watch(0x2A23, "random number (2A23)");
    mt.watch(0x0008, "fill bytes (RST 08h)");
    mt.watch(0x0010, "read table byte (RST 10h)");
    mt.watch_to(0x0042, 0x0050, "task queue push, up to its RET at 0050 (the RET itself is not counted)");
    mt.watch(0x0028, "queue task inline (RST 28h; ends with a jump, so it never returns)");
    // run A and B and C with the meter, and A without it
    g_meter = nullptr;
    static Machine ma0(prog);
    const covplayer::Stats plain = covplayer::run(ma0, fa, nullptr);
    g_meter = &mt;
    static Machine ma1(prog);
    const covplayer::Stats withm = covplayer::run(ma1, fa, nullptr);
    CHECK(plain.checksum == withm.checksum, "the meter does not change the game (player A frame hashes equal with and without the meter)");
    covplayer::run_b(prog, fb, nullptr);
    covplayer::run_c(prog, fb, nullptr);
    g_meter = nullptr;

    const meter::Meter::Result *r = mt.result(0x2A23);
    CHECK(r && mt.measured_calls(0x2A23) > 50, "the random number routine was called often enough to measure");
    CHECK(mt.is_constant(0x2A23, 111, 13), "2A23: every call takes exactly 111 cycles and 13 instructions");

    const meter::Meter::Result *rf = mt.result(0x0008);
    bool fill_ok = rf && !rf->rows.empty();
    int turns_min = 1 << 30, turns_max = 0;
    if (fill_ok) {
        const size_t nf = rf->feat_names.size();
        size_t ib = 0;
        for (size_t i = 0; i < nf; ++i) if (rf->feat_names[i] == "B(0=256)") ib = i;
        for (auto &row : rf->rows) {
            const int b = row.first[ib], cyc = row.first[nf], ins = row.first[nf + 1];
            turns_min = std::min(turns_min, b);
            turns_max = std::max(turns_max, b);
            if (cyc != 5 + 26 * b || ins != 1 + 3 * b) fill_ok = false;
        }
    }
    CHECK(fill_ok && mt.measured_calls(0x0008) > 100 && turns_max > turns_min, "0008: every call takes 5 + 26 * B cycles and 1 + 3 * B instructions (B = 0 counts as 256)");
    std::printf("  fill: %llu calls, B from %d to %d\n", (unsigned long long)mt.measured_calls(0x0008), turns_min, turns_max);
    const meter::Meter::Result *rq = mt.result(0x0028);
    CHECK(rq && mt.measured_calls(0x0028) == 0 && rq->left_without_ret > 0, "0028 ends with a jump: reported as never returned, no numbers invented");
    CHECK(mt.measured_calls(0x0042) > 100 && mt.measured_calls(0x0010) > 100, "the queue push and the table reader were measured");
    {
        const meter::Meter::Result *rp = mt.result(0x0042);
        bool ok = rp && !rp->rows.empty();
        if (ok)
            for (auto &row : rp->rows) {
                const size_t nf = rp->feat_names.size();
                const int c = row.first[nf], n = row.first[nf + 1];
                ok = ok && ((c == 66 && n == 7) || (c == 68 && n == 8)); // 76/8 and 78/9 with the RET (10 cycles, 1 instruction) taken off
            }
        CHECK(ok, "watch_to: 0042 measured up to 0050 gives 66/7 or 68/8, which is 76/8 or 78/9 less the RET");
    }

    FILE *o = std::fopen(argv[4], "wb");
    if (!o) return 2;
    mt.report(o);
    std::fclose(o);
    mt.report(stdout);
    std::printf("%s\n", g_fail ? "FAILED" : "all checks passed");
    return g_fail ? 1 : 0;
}
