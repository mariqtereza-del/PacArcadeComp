// X28: the cleared-round achievements (rounds 2, 5, 9, 13, 17) and PEACEFUL RUN (three rounds in a row with no ghost eaten and no death). Part 1: made-up events. Part 2: the REAL ROM
// (tests/scenario.h): rounds are cleared by the game's own code (one dot left, Pac-Man eats it), round 2, 5, 9, 13 and 17 each unlock exactly their achievement; three clean rounds in a row unlock
// PEACEFUL RUN at the third; a ghost eaten in a round breaks the streak. Usage: test_round_ach S27-replay-file
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include "../src/app/achievements.h"
#include "../src/app/stats.h"
#include "scenario.h"
using namespace achievements;
using events::Event;
static int checks = 0, fails = 0;
static void chk(const std::string &what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what.c_str(), got, want);
    } else std::printf("  ok: %s\n", what.c_str());
}
static Event ev(int type, int player = 1, int value = 0) {
    Event e;
    e.type = type;
    e.player = player;
    e.value = value;
    e.frame = 1;
    return e;
}
static std::vector<uint8_t> rdf(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static const int kLv[5] = {2, 5, 9, 13, 17};
// A made-up round: LEVEL n begins, optional ghost / death, the round is cleared.
static void round_(Engine &g, int level, bool ghost, bool death, bool counted = true) {
    g.on_event(ev(events::kLevel, 1, level), counted);
    if (ghost) g.on_event(ev(events::kGhostEaten, 1, 1), counted);
    if (death) g.on_event(ev(events::kDeath, 1, 0), counted);
    g.on_event(ev(events::kRoundCleared, 1, 0), counted);
}
struct Rig {
    stats::Collector sc;
    Engine g;
    int unlocks[kCount] = {0};
    bool counts = true;
    scenario::World w;
    Rig(const std::vector<uint8_t> &prog, const char *replay)
        : w(prog, replay, [this](const events::Raw &r, unsigned f) {
              sc.feed(r, f, counts, r.score, 0);
              g.feed(sc.log(), counts);
              for (int u : g.newly()) ++unlocks[u];
          }) {}
    bool go_to_level(int L) { // the next round is level L (same poke as the fruit test)
        w.wr(scenario::video_addr(scenario::kDotRight), 0x10);
        w.wr(0x4E13, L - 2);
        w.wr(0x4E0A, 0x68 + L - 2);
        w.wr(0x4E0E, 243);
        w.place_pac(scenario::kDotRight);
        for (int f = 0; f < 2000 && w.rd(0x4E13) != L - 1; ++f) {
            if (w.rd(0x4E0E) != 244) w.place_pac(scenario::kDotRight);
            w.step();
        }
        for (int f = 0; f < 300; ++f) {
            w.place_pac(scenario::kDotRight);
            w.step();
        }
        return w.rd(0x4E13) == L - 1;
    }
    // The round on the screen is cleared by the game's own code: one dot is left (counter 243), Pac-Man eats it; then the next round starts.
    bool clear_round() {
        const unsigned c0 = sc.count(events::kRoundCleared), l0 = sc.count(events::kLevel);
        w.wr(scenario::video_addr(scenario::kDotRight), 0x10);
        w.wr(0x4E0E, 243);
        w.place_pac(scenario::kDotRight);
        for (int f = 0; f < 2000 && sc.count(events::kLevel) == l0;
             ++f) { // Pac-Man is put one tile beside the dot and walks onto it (joystick held), again and again until it is eaten
            if (w.rd(0x4E0E) != 244 && f % 24 == 0) { w.place_pac({scenario::kDotRight.a, scenario::kDotRight.b + 1}); }
            w.step_held(0x04);
        }
        for (int f = 0; f < 300; ++f) {
            w.place_pac({scenario::kDotRight.a, scenario::kDotRight.b + 1});
            w.step();
        }
        return sc.count(events::kRoundCleared) == c0 + 1 && sc.count(events::kLevel) == l0 + 1;
    }
    int cleared_set() {
        int m = 0;
        for (int i = 0; i < 5; ++i) m |= unlocks[kRound2 + i] << i;
        return m;
    }
};
int main(int argc, char **argv) {
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int L = 1; L <= 18; ++L) {
            round_(g, L, false, false);
            int want = 0;
            for (int i = 0; i < 5; ++i) want |= (L >= kLv[i] ? 1 : 0) << i;
            int got = 0;
            for (int i = 0; i < 5; ++i) got |= g.unlocked(kRound2 + i) << i;
            chk("round " + std::to_string(L) + " cleared: the round achievements so far", got, want);
        }
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kLevel, 1, 2), true);
        chk("being in round 2 is not clearing it", g.unlocked(kRound2), 0);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        chk("clearing round 2 unlocks WARMING UP", g.unlocked(kRound2), 1);
        chk("  only it", g.count(), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        round_(g, 1, false, false);
        round_(g, 2, false, false);
        chk("two clean rounds: not yet PEACEFUL RUN", g.unlocked(kPeaceful), 0);
        round_(g, 3, false, false);
        chk("the third clean round: PEACEFUL RUN", g.unlocked(kPeaceful), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        round_(g, 1, false, false);
        round_(g, 2, true, false);
        round_(g, 3, false, false);
        round_(g, 4, false, false);
        chk("a ghost eaten in round 2 breaks the streak: after rounds 3 and 4 not yet", g.unlocked(kPeaceful), 0);
        round_(g, 5, false, false);
        chk("  three clean rounds after it: PEACEFUL RUN", g.unlocked(kPeaceful), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        round_(g, 1, false, false);
        round_(g, 2, false, true);
        round_(g, 3, false, false);
        round_(g, 4, false, false);
        chk("a death in round 2 breaks the streak", g.unlocked(kPeaceful), 0);
        round_(g, 5, false, false);
        chk("  three clean rounds after it: PEACEFUL RUN", g.unlocked(kPeaceful), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kLevel, 1, 1), true);
        g.on_event(ev(events::kDeath, 1, 0), true);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        g.on_event(ev(events::kLevel, 1, 2), true);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        g.on_event(ev(events::kLevel, 1, 3), true);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        chk("a death in round 1 (the first of three) means the next two rounds are only two clean ones", g.unlocked(kPeaceful), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        round_(g, 1, false, false);
        round_(g, 2, false, false);
        g.on_event(ev(events::kGameStart, 1, 3), true);
        round_(g, 1, false, false);
        chk("a new game starts the streak again (2 + 1 clean rounds in two games)", g.unlocked(kPeaceful), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        round_(g, 1, false, false);
        round_(g, 2, false, false, false);
        round_(g, 3, false, false, false);
        chk("a helped game: nothing after the help (no WARMING UP, no PEACEFUL RUN)", g.count(), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int L = 1; L <= 3; ++L) {
            Event a = ev(events::kLevel, 2, L), b = ev(events::kRoundCleared, 2, 0);
            g.on_event(a, true);
            g.on_event(b, true);
        }
        chk("player 2's rounds: nothing", g.count(), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 5), true);
        round_(g, 1, false, false);
        round_(g, 2, false, false);
        round_(g, 3, false, false);
        chk("5 lives: nothing", g.count(), 0);
    }
    for (int i = kRound2; i <= kPeaceful; ++i) {
        const Def &d = def(i);
        bool ok = std::strlen(d.name) <= 14;
        for (const char *c = d.text; *c; ++c) ok = ok && ((*c >= 'A' && *c <= 'Z') || *c == ' ' || *c == '.' || (*c >= '0' && *c <= '9'));
        chk(std::string("name fits and text is in the game font: ") + d.name, ok, 1);
    }
    {
        bool uniq = true;
        for (int i = 0; i < kCount; ++i)
            for (int j = i + 1; j < kCount; ++j) uniq = uniq && std::strcmp(def(i).key, def(j).key) != 0 && std::strcmp(def(i).name, def(j).name) != 0;
        chk("all keys and names are different", uniq, 1);
    }
    if (argc < 2) {
        std::printf("(no replay file: part 2 skipped)\n%d checks, %d failed\n", checks, fails);
        return fails ? 1 : 0;
    }
    std::printf("Part 2: real machine\n");
    std::vector<uint8_t> prog = rdf("build/rom/prog.bin");
    if (prog.empty()) {
        std::printf("needs build/rom/prog.bin\n");
        return 2;
    }
    for (int i = 0; i < 5; ++i) {
        const int L = kLv[i];
        Rig r(prog, argv[1]);
        bool ok = true;
        if (L == 17) {
            r.w.wr(scenario::video_addr(scenario::kDotRight), 0x10);
            ok = r.w.level_17();
            for (int f = 0; f < 300; ++f) {
                r.w.place_pac(scenario::kDotRight);
                r.w.step();
            }
        } else ok = r.go_to_level(L);
        chk("round " + std::to_string(L) + " reached (level counter " + std::to_string(r.w.rd(0x4E13) + 1) + ")", ok && r.w.rd(0x4E13) + 1 == L, 1);
        chk("  jumping into it unlocked no round achievement", r.cleared_set(), 0);
        chk("  the game's own code clears round " + std::to_string(L), r.clear_round(), 1);
        chk("  exactly " + std::string(def(kRound2 + i).name) + " unlocked, once", r.cleared_set(), 1 << i);
        chk("  nobody died", r.sc.count(events::kDeath), 0);
        r.w.steps(100);
        chk("  still once after 100 more frames", r.unlocks[kRound2 + i], 1);
    }
    {
        Rig r(prog, argv[1]);
        bool a = r.clear_round(), b = r.clear_round();
        chk("real: rounds 1 and 2 cleared by the game", a && b, 1);
        chk("  round 2 cleared: WARMING UP", r.unlocks[kRound2], 1);
        chk("  two clean rounds: no PEACEFUL RUN yet", r.unlocks[kPeaceful], 0);
        bool c = r.clear_round();
        chk("real: round 3 cleared", c, 1);
        chk("  the third clean round: PEACEFUL RUN once", r.unlocks[kPeaceful], 1);
        chk("  nobody died, no ghost eaten", r.sc.count(events::kDeath) + r.sc.count(events::kGhostEaten), 0);
    }
    {
        Rig r(prog, argv[1]);
        bool a = r.clear_round();
        bool g = r.w.four_ghosts_eaten();
        for (int f = 0; f < 900; ++f) {
            r.w.place_pac(scenario::kDotRight);
            r.w.step();
        }
        chk("real: round 1 cleared, then in round 2 four ghosts are eaten", a && g && r.sc.count(events::kGhostEaten) == 4, 1);
        bool ok = true;
        for (int k = 0; k < 3; ++k) {
            bool o = r.clear_round();
            ok = o && ok;
        }
        chk("  three more rounds cleared", ok, 1);
        chk("  the round with the ghost broke the streak: no PEACEFUL RUN after the rounds 2, 3, 4", r.unlocks[kPeaceful], 0);
        chk("  nobody died", r.sc.count(events::kDeath), 0);
        bool d = r.clear_round();
        chk("  one more clean round (round 5): three clean ones in a row (3, 4, 5)", d, 1);
        chk("  PEACEFUL RUN once", r.unlocks[kPeaceful], 1);
    }
    {
        Rig r(prog, argv[1]);
        r.counts = false;
        bool a = r.clear_round(), b = r.clear_round(), c = r.clear_round();
        chk("helped game, real ROM: three rounds cleared", a && b && c, 1);
        chk("  nothing unlocks", r.g.count(), 0);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
