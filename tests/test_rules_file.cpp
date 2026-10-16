// T02i: the rules file (src/rules/rules_file.h): reading, bad lines, the numbers put into the level, the points, and a real changed game on the machine.
// Usage: test_rules_file PROG.BIN FRAMES
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "logic_probe.h"
#include "../src/rules/rules.h"

static int fails = 0, checks = 0;
static void chk(const std::string &what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what.c_str(), got, want);
    }
}
static unsigned popcount(uint32_t v) {
    unsigned n = 0;
    for (; v; v &= v - 1) ++n;
    return n;
}
static uint32_t rng = 99;
static unsigned rnd() { rng = rng * 1664525u + 1013904223u; return rng >> 8; }

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    using rules::Tuning;
    // ---- 1. reading
    {
        Tuning t;
        std::string pr;
        const int n = rules::parse(rules::standard_file_text(), t, &pr);
        chk("standard file: every key is taken", n, (long)rules::key_table().size());
        chk("standard file: no problems", (long)pr.size(), 0);
        chk("standard file: not a mod", t.active, 0);
        std::string again = rules::standard_file_text();
        chk("standard file: has a comment line before every value", (long)std::count(again.begin(), again.end(), '#') >= 3 + (long)rules::key_table().size(), 1);
        // wrong lines are reported with their line number and ignored; the good lines are still used
        const std::string text =
            "# comment\n"
            "ghost_speed_percent 70\n"       // 2 good
            "bogus_key 5\n"                  // 3 unknown key
            "dot_points 15\n"                // 4 not a multiple of 10
            "dot_points abc\n"               // 5 not a number
            "ghost_speed_percent.99 50\n"    // 6 level out of range
            "dot_points.3 20\n"              // 7 this key has no level
            "pacman_speed_percent 5\n"       // 8 out of range
            "justonetoken\n"                 // 9 no value
            "elroy1_dots_left.2 -1\n"        // 10 good (-1 = original)
            "energizer_points 100\r\n"       // 11 good, Windows line end
            "   \n"                          // 12 blank
            "ghost_speed_percent.4 50\n"     // 13 good, wins over the plain line for level 4
            "ghost_speed_percent 90\n";      // 14 good, later plain line: does not overwrite level 4
        pr.clear();
        const int m = rules::parse(text, t, &pr);
        chk("mixed file: 5 values taken", m, 5);
        for (int line : {3, 4, 5, 6, 7, 8, 9}) chk("mixed file: line " + std::to_string(line) + " reported", (long)(pr.find("line " + std::to_string(line) + ":") != std::string::npos), 1);
        for (int line : {2, 10, 11, 13, 14}) chk("mixed file: line " + std::to_string(line) + " not reported", (long)(pr.find("line " + std::to_string(line) + ":") == std::string::npos), 1);
        chk("mixed file: it is a mod", t.active, 1);
        chk("mixed file: plain line sets level 1", t.ghost_speed[1], 90);
        chk("mixed file: level 4 keeps its own line", t.ghost_speed[4], 50);
        chk("mixed file: energizer points", t.points[logic::kEnergizer], 100);
        chk("mixed file: dot points unchanged", t.points[logic::kDot], 10);
        chk("mixed file: elroy -1 stays original", t.elroy1[2], -1);
        Tuning u;
        pr.clear();
        rules::parse("# nothing\nfoo 1\nbar 2\n", u, &pr);
        chk("only bad lines: not a mod", u.active, 0);
        chk("only bad lines: both reported", (long)std::count(pr.begin(), pr.end(), '\n'), 2);
    }
    // ---- 2. speed patterns
    chk("pattern at 100 percent is unchanged", rules::scale_pattern(0x2A552A55, 100), 0x2A552A55);
    for (int pct : {20, 50, 80, 120, 150, 200}) {
        const uint32_t base = 0x55555555, p = rules::scale_pattern(base, pct);
        chk("pattern " + std::to_string(pct) + " percent: ones", popcount(p), std::min(32, (16 * pct + 50) / 100));
    }
    // ---- 3. the level numbers: the standard tuning changes nothing, a mod changes what it says
    {
        Tuning t;
        t.reset();
        for (unsigned row = 0; row < 21; ++row) {
            uint8_t a[logic::kWorkRamSize] = {0}, b[logic::kWorkRamSize] = {0};
            logic::start_level_timers(logic::WorkRam{a}, row);
            std::memcpy(b, a, sizeof a);
            rules::apply_level(logic::WorkRam{b}, row, t);
            chk("standard tuning leaves level row " + std::to_string(row) + " alone", std::memcmp(a, b, sizeof a), 0);
            // a mod
            Tuning m;
            for (int l = 1; l < rules::kLevels; ++l) {
                m.ghost_speed[l] = 50;
                m.pacman_speed[l] = 120;
                m.phase_time[l] = 200;
                m.frightened_time[l] = 50;
                m.elroy1[l] = 7;
                m.elroy2[l] = 3;
                m.pinky_dots[l] = 11;
                m.inky_dots[l] = 22;
                m.clyde_dots[l] = 33;
            }
            uint8_t c[logic::kWorkRamSize];
            std::memcpy(c, a, sizeof a);
            rules::apply_level(logic::WorkRam{c}, row, m);
            logic::WorkRam wa{a}, wc{c};
            chk("mod: ghost normal speed has fewer steps (row " + std::to_string(row) + ")", popcount(rules::get_pattern(wc, logic::kSpeedPatternAddress[4])) <=
                                                                                                     (popcount(rules::get_pattern(wa, logic::kSpeedPatternAddress[4])) + 1) / 2 + 1, 1);
            chk("mod: Pinky has the same new normal speed as Blinky", rules::get_pattern(wc, logic::kSpeedNormalAddress[1]), rules::get_pattern(wc, logic::kSpeedPatternAddress[4]));
            chk("mod: Pac-Man's speed pattern changed or was already full", rules::get_pattern(wc, logic::kSpeedPatternAddress[0]) != rules::get_pattern(wa, logic::kSpeedPatternAddress[0]) ||
                                                                                 popcount(rules::get_pattern(wa, logic::kSpeedPatternAddress[0])) >= 27, 1);
            chk("mod: elroy 1", c[logic::kElroyDotsLeftAddress - 0x4C00], 7);
            chk("mod: elroy 2", c[logic::kElroyDotsLeftAddress + 1 - 0x4C00], 3);
            chk("mod: Pinky/Inky/Clyde dots", c[logic::kHouseLimitsAddress - 0x4C00] * 10000 + c[logic::kHouseLimitsAddress + 1 - 0x4C00] * 100 + c[logic::kHouseLimitsAddress + 2 - 0x4C00], 11 * 10000 + 22 * 100 + 33);
            chk("mod: frightened time halves", logic::word_at(wc, logic::kFrightenedTimeAddress), logic::word_at(wa, logic::kFrightenedTimeAddress) / 2);
            const uint16_t p0 = logic::word_at(wa, logic::kPhaseLimitsAddress);
            if (p0 && p0 < 65534) chk("mod: first phase twice as long", logic::word_at(wc, logic::kPhaseLimitsAddress), 2 * p0);
            for (unsigned i = 0; i < 7; ++i) {
                const uint16_t v = logic::word_at(wa, logic::kPhaseLimitsAddress + 2 * i);
                if (v >= 65534) chk("mod: 'never' stays never", logic::word_at(wc, logic::kPhaseLimitsAddress + 2 * i), v);
            }
        }
    }
    // ---- 4. points and the extra life
    {
        Tuning t;
        t.reset();
        for (int i = 0; i < 20000; ++i) { // the standard tuning scores exactly like the original routine
            uint8_t a[logic::kWorkRamSize], b[logic::kWorkRamSize];
            for (auto &x : a) x = (uint8_t)rnd();
            for (int p = 0; p < 2; ++p)
                for (int k = 0; k < 3; ++k) a[logic::kScoreAddress[p] + k - 0x4C00] = (uint8_t)(((rnd() % 10) << 4) | (rnd() % 10)); // valid BCD
            a[logic::kBonusLifeLimitAddress - 0x4C00] = (uint8_t[]){0x10, 0x15, 0x20, 0xFF}[rnd() % 4];
            std::memcpy(b, a, sizeof a);
            const unsigned idx = rnd() % logic::kPointsKinds;
            const logic::ExtraLife x = logic::add_points(logic::WorkRam{a}, idx), y = rules::add_points_tuned(logic::WorkRam{b}, idx, t);
            if (std::memcmp(a, b, sizeof a) != 0 || x != y) {
                chk("standard points = original points", 0, 1);
                break;
            }
        }
        // a mod: dots are worth 100, the extra life comes at 500
        Tuning m;
        m.points[logic::kDot] = 100;
        m.extra_life_score = 500;
        uint8_t r[logic::kWorkRamSize] = {0};
        logic::WorkRam w{r};
        w[logic::kLivesAddress] = 2;
        w[logic::kLivesShownAddress] = 2;
        int lives_given = 0;
        for (int i = 1; i <= 7; ++i) {
            const logic::ExtraLife e = rules::add_points_tuned(w, logic::kDot, m);
            if (e == logic::kExtraLifeGivenNow) ++lives_given, chk("extra life comes at exactly 500", i, 5);
        }
        chk("score after seven dots of 100", r[0x4E80 - 0x4C00] | (r[0x4E81 - 0x4C00] << 8), 0x0700);
        chk("one extra life only", lives_given, 1);
        chk("lives is 3", r[logic::kLivesAddress - 0x4C00], 3);
        // big values carry into the third byte
        Tuning big;
        big.points[logic::kKey] = 99990;
        uint8_t q[logic::kWorkRamSize] = {0};
        q[0x4E80 - 0x4C00] = 0x20;
        rules::add_points_tuned(logic::WorkRam{q}, logic::kKey, big);
        chk("99990 + 20 = 100010: bytes", q[0x4E80 - 0x4C00] | (q[0x4E81 - 0x4C00] << 8) | (q[0x4E82 - 0x4C00] << 16), 0x100010);
    }
    // ---- 5. a changed game on the machine: slower ghosts really move less; the same hooks with the standard tuning change nothing
    {
        std::vector<uint8_t> prog(0x4000);
        FILE *f = std::fopen(argv[1], "rb");
        if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) return 2;
        std::fclose(f);
        const long frames = std::atol(argv[2]);
        auto play = [&](const Tuning &tune, unsigned long *ghost_steps, unsigned long *pac_steps, bool *same_as_original) {
            static Machine A(prog), B(prog);
            A.~Machine();
            new (&A) Machine(prog);
            B.~Machine();
            new (&B) Machine(prog);
            rules::tuning() = tune;
            B.rules = rules::table();
            A.rules = nullptr;
            uint32_t g = 7;
            int dir = 4, hold = 0;
            unsigned prev[4][2] = {{0}}, prevp[2] = {0};
            bool first = true, same = true;
            *ghost_steps = *pac_steps = 0;
            for (long fr = 0; fr < frames; ++fr) {
                const long p = fr % 15000;
                if (p >= 430 && hold <= 0) {
                    g = g * 1664525u + 1013904223u;
                    static const int d[4] = {1, 2, 4, 8};
                    dir = d[(g >> 24) & 3];
                    hold = 10 + (int)((g >> 16) & 31);
                }
                --hold;
                for (Machine *m : {&A, &B}) {
                    uint8_t in0 = 0xFF, in1 = 0xFF;
                    if (p >= 400 && p < 405) in0 &= (uint8_t)~0x20;
                    if (p >= 420 && p < 424) in1 &= (uint8_t)~0x20;
                    if (p >= 430) in0 &= (uint8_t)~dir;
                    m->bus.in0 = in0;
                    m->bus.in1 = in1;
                    m->bus.dsw1 = 0xC9;
                    m->run_frame();
                }
                if (std::memcmp(A.bus.workram, B.bus.workram, sizeof A.bus.workram) != 0) same = false;
                for (int k = 0; k < 4; ++k) {
                    const unsigned y = B.bus.workram[0x4D00 + 2 * k - 0x4C00], x = B.bus.workram[0x4D01 + 2 * k - 0x4C00];
                    if (!first && (y != prev[k][0] || x != prev[k][1])) ++*ghost_steps;
                    prev[k][0] = y, prev[k][1] = x;
                }
                const unsigned py = B.bus.workram[0x4D08 - 0x4C00], px = B.bus.workram[0x4D09 - 0x4C00];
                if (!first && (py != prevp[0] || px != prevp[1])) ++*pac_steps;
                prevp[0] = py, prevp[1] = px;
                first = false;
            }
            *same_as_original = same;
            rules::tuning().reset();
        };
        Tuning standard, slow;
        for (int l = 1; l < rules::kLevels; ++l) slow.ghost_speed[l] = 60;
        slow.active = true;
        unsigned long gs_std, ps_std, gs_slow, ps_slow;
        bool same_std, same_slow;
        play(standard, &gs_std, &ps_std, &same_std);
        play(slow, &gs_slow, &ps_slow, &same_slow);
        std::printf("part 5: %ld frames of play: ghost steps with the standard rules %lu, with ghosts at 60%% %lu; the machine with the standard tuning equals the original: %s; the slow-ghost machine equals the original: %s\n",
                    frames, gs_std, gs_slow, same_std ? "yes" : "NO", same_slow ? "yes" : "no (as it must)");
        chk("standard tuning: every frame equals the original", same_std, 1);
        chk("slow ghosts: the game is different", same_slow, 0);
        chk("slow ghosts: clearly fewer ghost steps (under 80 percent)", gs_slow * 100 < gs_std * 80, 1);
        chk("slow ghosts: not stuck (over 30 percent)", gs_slow * 100 > gs_std * 30, 1);
        (void)ps_std;
        (void)ps_slow;
    }
    std::printf("%s test_rules_file: %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails ? 1 : 0;
}
