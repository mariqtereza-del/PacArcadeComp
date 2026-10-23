// S34D: DIP settings put into a running machine (src/app/dipnow.h) give the same game as powering the machine on with those settings.
// Machine A is powered on with the new DIP byte. Machine B is powered on with the old byte and, at the attract screen, gets the new byte through dipnow::apply.
// Both get the same inputs (coin, start, a wandering joystick). From the start of the game on, all RAM (work, video, colour, sprites), the CPU registers and the cycle count
// of every frame must be identical. Also: the apply does not touch the CPU or the cycle count, it waits for a quiet moment, and a wrong version (apply that forgets
// to copy the RAM back) is caught.
// Usage: test_dip_live PROG.BIN
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include "../src/app/dipnow.h"
#include "../src/board/inputs.h"

static int n = 0, fails = 0;
static long g_coin = 600;
#define CHECK(c, m) do { ++n; if (!(c)) { ++fails; std::printf("FAIL: %s\n", m); } } while (0)

struct Run {
    Machine m;
    Inputs in;
    explicit Run(const std::vector<uint8_t> &p, uint8_t dsw) : m(p) { m.bus.dsw1 = dsw; }
};
static void frame(Machine &m, long fr, uint32_t &rng) {
    uint8_t in0 = 0xFF, in1 = 0xFF;
    if (fr >= g_coin && fr < g_coin + 5) in0 &= (uint8_t)~0x20;      // coin
    if (fr >= g_coin + 100 && fr < g_coin + 104) in1 &= (uint8_t)~0x20;      // start
    if (fr >= g_coin + 300) {                                       // wander
        rng = rng * 1664525u + 1013904223u;
        static const uint8_t d[4] = {0x01, 0x02, 0x04, 0x08};
        in0 &= (uint8_t)~d[(rng >> 24) & 3];
    }
    m.bus.in0 = in0;
    m.bus.in1 = in1;
    m.run_frame();
}
static bool same_machine(Machine &a, Machine &b) {
    return !std::memcmp(a.bus.workram, b.bus.workram, sizeof a.bus.workram) && !std::memcmp(a.bus.videoram, b.bus.videoram, sizeof a.bus.videoram) &&
           !std::memcmp(a.bus.colorram, b.bus.colorram, sizeof a.bus.colorram) && !std::memcmp(a.bus.spriteram, b.bus.spriteram, sizeof a.bus.spriteram) &&
           a.on_get_af() == b.on_get_af() && a.on_get_bc() == b.on_get_bc() && a.on_get_de() == b.on_get_de() && a.on_get_hl() == b.on_get_hl() &&
           a.on_get_pc() == b.on_get_pc() && a.on_get_sp() == b.on_get_sp() && a.ticks_in_frame() == b.ticks_in_frame();
}
int main(int argc, char **argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: test_dip_live PROG.BIN\n"); return 2; }
    std::ifstream f(argv[1], std::ios::binary);
    std::vector<uint8_t> prog((std::istreambuf_iterator<char>(f)), {});
    if (argc > 2) g_coin = std::atol(argv[2]);
    const uint8_t old_dip = 0xC9;   // factory: 3 lives, bonus at 10000, normal difficulty, normal names
    const struct { const char *name; DipSettings d; } cases[] = {
        {"5 lives", [] { DipSettings d; d.lives = DipSettings::Five; return d; }()},
        {"1 life, bonus at 20000", [] { DipSettings d; d.lives = DipSettings::One; d.bonus = DipSettings::At20000; return d; }()},
        {"no bonus, hard", [] { DipSettings d; d.bonus = DipSettings::NoBonus; d.normal_difficulty = false; return d; }()},
        {"alternate ghost names, 2 lives, bonus at 15000", [] { DipSettings d; d.normal_ghost_names = false; d.lives = DipSettings::Two; d.bonus = DipSettings::At15000; return d; }()},
    };
    for (const auto &c : cases) {
        const uint8_t nd = c.d.to_byte();
        for (int variant = 0; variant < 2; ++variant) {   // 0: the correct apply, 1: a wrong version that forgets to copy the RAM back (must be caught)
            Run A(prog, nd), B(prog, old_dip);
            uint32_t ra = 7, rb = 7;
            long first_game = -1, bad = 0, applied_at = -1;
            bool quiet_seen = false;
            for (long fr = 0; fr < g_coin + 3400; ++fr) {
                if (applied_at < 0 && fr >= 400 && dipnow::quiet(B.m)) {   // the attract screen: put the new switches in
                    Machine before = B.m;
                    quiet_seen = true;
                    if (variant == 0) CHECK(dipnow::apply(B.m, nd), "apply returned true");
                    else { std::unique_ptr<Machine> t(new Machine(B.m)); t->bus.dsw1 = nd; B.m.bus.dsw1 = nd; (void)t; }   // wrong: only the switch byte changes
                    applied_at = fr;
                    if (variant == 0) {
                        CHECK(before.on_get_pc() == B.m.on_get_pc() && before.on_get_sp() == B.m.on_get_sp() && before.on_get_af() == B.m.on_get_af() && before.on_get_hl() == B.m.on_get_hl() &&
                                  before.ticks_in_frame() == B.m.ticks_in_frame() && !std::memcmp(before.bus.videoram, B.m.bus.videoram, sizeof before.bus.videoram),
                              "apply leaves the CPU, the cycle count and the video memory alone");
                        int changed = 0;
                        for (unsigned i = 0; i < sizeof before.bus.workram; ++i) changed += before.bus.workram[i] != B.m.bus.workram[i];
                        CHECK(changed > 0 && changed <= 8, "apply changes only the few settings bytes of work RAM");
                    }
                }
                B.m.bus.dsw1 = applied_at >= 0 ? nd : old_dip;
                A.m.bus.dsw1 = nd;
                frame(A.m, fr, ra);
                frame(B.m, fr, rb);
                if (first_game < 0 && fr > g_coin + 100 && A.m.bus.read(0x4E00) == 3) first_game = fr;
                if (variant == 0 && std::getenv("NH_PRE") && applied_at >= 0 && first_game < 0 && !same_machine(A.m, B.m)) { static int shown = 0; if (shown++ < 3) { std::printf("   pre-game differs at frame %ld:", fr); for (unsigned i = 0; i < sizeof A.m.bus.workram; ++i) if (A.m.bus.workram[i] != B.m.bus.workram[i]) std::printf(" %04X(%02X/%02X)", 0x4C00 + i, A.m.bus.workram[i], B.m.bus.workram[i]); std::printf("\n"); } }
                if (first_game >= 0 && !same_machine(A.m, B.m)) { ++bad; if (variant == 0 && std::getenv("NH_DEBUG") && bad < 4) { std::printf("   differs at frame %ld:", fr); for (unsigned i = 0; i < sizeof A.m.bus.workram; ++i) if (A.m.bus.workram[i] != B.m.bus.workram[i]) std::printf(" %04X(%02X/%02X)", 0x4C00 + i, A.m.bus.workram[i], B.m.bus.workram[i]); std::printf("\n"); } }
            }
            if (variant == 0) {
                CHECK(quiet_seen, (std::string(c.name) + ": a quiet moment was found").c_str());
                CHECK(first_game > 0 && bad == 0, (std::string(c.name) + ": from the start of the game on, every frame is identical to a machine powered on with those switches").c_str());
            } else CHECK(bad > 0, (std::string(c.name) + ": a wrong apply (switch byte only) is caught").c_str());
            std::printf("  %s [%s]: applied at frame %ld, game started at frame %ld, %ld frames differ\n", c.name, variant ? "wrong version" : "correct", applied_at, first_game, bad);
        }
    }
    {   // quiet(): not during the self test, a coin, or a game
        Run R(prog, old_dip);
        uint32_t r = 3;
        bool ok_idle = false, ok_game = true;
        for (long fr = 0; fr < 1200; ++fr) {
            frame(R.m, fr, r);
            const int st = R.m.bus.read(0x4E00);
            if (st == 1) ok_idle = ok_idle || dipnow::quiet(R.m);
            if (st != 1 && dipnow::quiet(R.m)) ok_game = false;
        }
        CHECK(ok_idle && ok_game, "quiet() is true at the attract screen and false during the self test, with a credit and in a game");
    }
    std::printf("%d checks, %d failures\n%s test_dip_live\n", n, fails, fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
