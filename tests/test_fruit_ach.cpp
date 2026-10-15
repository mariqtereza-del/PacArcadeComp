// X26: the fruit achievements. Part 1: made-up events (each kind, five different, the same fruit five times, helped game, player 2). Part 2: the REAL ROM (tests/scenario.h): in each round where a fruit
// kind appears (1, 2, 3, 5, 7, 9, 11, 13) the game's own code shows the fruit, Pac-Man eats it, and exactly the matching achievement (plus FRUIT SALAD) unlocks, once;
// then the five fruit events those real runs produced (kinds 0-4, rounds 1, 2, 3, 5, 7) are played into one engine as one game: FRUIT BASKET unlocks exactly at the fifth. Usage: test_fruit_ach S27-replay-file
#include <cstdio>
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
static const char *kName[8] = {"cherry", "strawberry", "orange", "apple", "melon", "galaxian", "bell", "key"};
static int fruit_id(int kind) { return kCherry + kind; }

struct Rig { // the real machine with the collector (for its event log) and the achievement engine
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
    // The round counter is set so that the NEXT round is level L, one dot is left, Pac-Man eats it; the round after the flashing maze is level L.
    bool go_to_level(int L) {
        w.wr(scenario::video_addr(scenario::kDotRight), 0x10);
        w.wr(0x4E13, L - 2);
        w.wr(0x4E0A, 0x68 + L - 2);
        w.wr(0x4E0E, 243);
        w.place_pac(scenario::kDotRight);
        for (int f = 0; f < 2000 && w.rd(0x4E13) != L - 1; ++f) {
            if (w.rd(0x4E0E) != 244) w.place_pac(scenario::kDotRight);
            w.step();
        }
        w.steps(300);
        return w.rd(0x4E13) == L - 1;
    }
    // The free-running frames after the round change may have eaten the dot used to bring the fruit; a dot is put back on that tile (test-only poke of video RAM), as the game itself had it.
    bool eat_fruit() {
        w.wr(scenario::video_addr(scenario::kDotRight), 0x10);
        w.wr(0x4E0E, 69);
        int f = 0;
        while (f < 1500 && w.rd(0x4DD2) == 0) {
            w.place_pac(scenario::kDotRight);
            w.step();
            ++f;
        } // the next dot brings the fruit
        if (w.rd(0x4DD2) == 0) return false;
        const unsigned eaten_before = sc.count(events::kFruitEaten);
        for (
            int attempt = 0; attempt < 6 && w.rd(0x4DD2) != 0;
            ++attempt) { // three tiles to one side of the fruit; the joystick is held towards it and the game's own eating code runs (put back and tried again if Pac-Man was still turning)
            w.place_pac({(w.rd(0x4DD2) >> 3) + 32, (w.rd(0x4DD3) >> 3) + 30 + 3});
            for (int k = 0; k < 60 && w.rd(0x4DD2) != 0; ++k) w.step_held(0x04);
        }
        return sc.count(events::kFruitEaten) > eaten_before; // eaten, not timed out
    }
    int last_fruit_kind() {
        int k = -1;
        for (const Event &e : sc.log())
            if (e.type == events::kFruitEaten) k = e.value;
        return k;
    }
};
int main(int argc, char **argv) {
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int k = 0; k < 8; ++k) {
            Engine h;
            h.on_event(ev(events::kGameStart, 1, 3), true);
            h.on_event(ev(events::kFruitEaten, 1, k), true);
            int n = 0;
            for (int i = kCherry; i <= kKey; ++i) n += h.unlocked(i) ? 1 : 0;
            chk(std::string("a ") + kName[k] + " unlocks its own achievement", h.unlocked(fruit_id(k)), 1);
            chk(std::string("  and no other fruit achievement"), n, 1);
            chk(std::string("  FRUIT SALAD too, FRUIT BASKET not"), h.unlocked(kFirstFruit) * 10 + h.unlocked(kFiveFruits), 10);
        }
        for (int k = 0; k < 4; ++k) { g.on_event(ev(events::kFruitEaten, 1, k), true); }
        chk("four different fruits: FRUIT BASKET not yet", g.unlocked(kFiveFruits), 0);
        chk("the fifth different fruit unlocks FRUIT BASKET", g.on_event(ev(events::kFruitEaten, 1, 4), true) >= 0 && g.unlocked(kFiveFruits), 1);
        chk("it unlocks exactly once: a sixth kind adds only its own",
            (int)(g.on_event(ev(events::kFruitEaten, 1, 5), true) == fruit_id(5)) + (int)g.unlocked_now().size(), 2);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int i = 0; i < 6; ++i) g.on_event(ev(events::kFruitEaten, 1, 0), true);
        chk("the same fruit six times: no FRUIT BASKET", g.unlocked(kFiveFruits), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int k = 0; k < 3; ++k) g.on_event(ev(events::kFruitEaten, 1, k), true);
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int k = 3; k < 5; ++k) g.on_event(ev(events::kFruitEaten, 1, k), true);
        chk("three kinds in one game and two in the next: no FRUIT BASKET (one game only)", g.unlocked(kFiveFruits), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int k = 0; k < 5; ++k) g.on_event(ev(events::kFruitEaten, 1, k), k == 2 ? false : true);
        chk("a helped game: nothing from the fruit eaten after it was helped; FRUIT BASKET not unlocked", g.unlocked(kFiveFruits), 0);
        chk("the fruits before the help counted (cherry, strawberry)", g.unlocked(kCherry) + g.unlocked(kStrawberry), 2);
        chk("the ones after did not (orange, apple, melon)", g.unlocked(kOrange) + g.unlocked(kApple) + g.unlocked(kMelon), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int k = 0; k < 5; ++k) g.on_event(ev(events::kFruitEaten, 2, k), true);
        chk("player 2's fruit: nothing", g.count(), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 5), true);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        chk("5 lives: nothing", g.count(), 0);
    }
    for (int i = kCherry; i <= kFiveFruits; ++i) {
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
    const int levels[8] = {1, 2, 3, 5, 7, 9, 11, 13};
    for (int k = 0; k < 8; ++k) {
        Rig r(prog, argv[1]);
        const int L = levels[k];
        bool ok = true;
        if (L > 1) ok = r.go_to_level(L);
        chk(std::string("round ") + std::to_string(L) + " reached", ok, 1);
        chk(std::string("round ") + std::to_string(L) + ": the game shows a fruit and Pac-Man eats it", r.eat_fruit(), 1);
        chk(std::string("round ") + std::to_string(L) + ": the fruit is the ", r.last_fruit_kind() == k, 1);
        int others = 0;
        for (int i = kCherry; i <= kKey; ++i)
            if (i != fruit_id(k)) others += r.unlocks[i];
        chk(std::string("round ") + std::to_string(L) + ": " + def(fruit_id(k)).name + " unlocked exactly once", r.unlocks[fruit_id(k)], 1);
        chk("  no other single-fruit achievement", others, 0);
        chk("  FRUIT SALAD once", r.unlocks[kFirstFruit], 1);
        chk("  FRUIT BASKET not", r.unlocks[kFiveFruits], 0);
        r.w.steps(200);
        chk("  still once after 200 more frames", r.unlocks[fruit_id(k)], 1);
    }
    // Five different fruits in ONE game: the game's own code cannot be driven through five rounds in one machine run by the test's RAM pokes (after a jump of several rounds the fruit timer is not reliable),
    // so the five FRUIT EATEN events are taken from five real-ROM runs (rounds 1, 2, 3, 5, 7, kinds as the game itself produced them) and played into ONE engine as one game.
    {
        std::vector<Event> game;
        Event gs = ev(events::kGameStart, 1, 3);
        game.push_back(gs);
        const int seq[5] = {0, 1, 2, 3, 4};
        bool ok = true;
        for (int i = 0; i < 5; ++i) {
            Rig r(prog, argv[1]);
            bool a1 = levels[seq[i]] == 1 ? true : r.go_to_level(levels[seq[i]]);
            bool a2 = r.eat_fruit();
            ok = ok && a1 && a2;
            Event e = ev(events::kFruitEaten, 1, r.last_fruit_kind());
            chk(std::string("real run ") + std::to_string(i + 1) + ": fruit eaten is kind " + kName[seq[i]], e.value, seq[i]);
            game.push_back(e);
        }
        chk("five real fruit events obtained", ok, 1);
        Engine g;
        int got = 0;
        for (size_t i = 0; i < game.size(); ++i) {
            g.on_event(game[i], true);
            if (i >= 1 && i <= 4) got += g.unlocked(kFiveFruits) ? 1 : 0;
        }
        chk("after four different real fruits: FRUIT BASKET not yet", got, 0);
        chk("after the fifth different real fruit: FRUIT BASKET unlocked", g.unlocked(kFiveFruits), 1);
        chk("each of the five single-fruit achievements once",
            g.unlocked(kCherry) + g.unlocked(kStrawberry) + g.unlocked(kOrange) + g.unlocked(kApple) + g.unlocked(kMelon), 5);
        chk("total unlocked: FRUIT SALAD + 5 fruits + FRUIT BASKET", g.count(), 7);
    }
    {
        Rig r(prog, argv[1]);
        r.counts = false;
        bool ok = r.eat_fruit();
        chk("helped game, real ROM: the fruit is eaten", ok, 1);
        chk("nothing unlocks in a helped game", r.g.count(), 0);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
