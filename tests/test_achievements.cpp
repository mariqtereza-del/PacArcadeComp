// X25: unit test of src/app/achievements.h on made-up events (no ROM): unlock rules, the assist rule, player 1 / 3 lives, exactly once, the unlock file (round trip, corrupt file, unknown keys), the pop-up queue.
// Part 2 (needs build/rom/prog.bin and the S27 replay as argument): the REAL ROM with scenarios: the sample achievements unlock exactly once.
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
int main(int argc, char **argv) {
    {
        Engine g;
        chk("nothing unlocked at first", g.count(), 0);
        chk("a pellet before any game started: no unlock (no eligible game)", g.on_event(ev(events::kEnergizer), true), -1);
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chk("game start with 3 lives: eligible", g.eligible(), 1);
        chk("a dot unlocks nothing", g.on_event(ev(events::kDot), true), -1);
        chk("first power pellet unlocks POWER UP", g.on_event(ev(events::kEnergizer), true), kFirstPellet);
        chk("count 1", g.count(), 1);
        chk("mask", (long)g.mask(), 1);
        chk("second pellet: nothing (exactly once)", g.on_event(ev(events::kEnergizer), true), -1);
        chk("still 1", g.count(), 1);
        chk("a cherry unlocks FRUIT SALAD first", g.on_event(ev(events::kFruitEaten, 1, 0), true), kFirstFruit);
        chk("and CHERRY PICKER with it (one event can unlock several)", g.unlocked(kCherry), 1);
        chk("count 3", g.count(), 3);
        chk("another cherry: nothing", g.on_event(ev(events::kFruitEaten, 1, 0), true), -1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 5), true);
        chk("5 lives: not eligible", g.eligible(), 0);
        chk("pellet in a 5-lives game: nothing", g.on_event(ev(events::kEnergizer), true), -1);
        g.on_event(ev(events::kGameStart, 1, 1), true);
        chk("1 life: not eligible", g.eligible(), 0);
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chk("a new 3-lives game: eligible again", g.eligible(), 1);
        chk("pellet now unlocks", g.on_event(ev(events::kEnergizer), true), kFirstPellet);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chk("player 2's pellet: nothing", g.on_event(ev(events::kEnergizer, 2), true), -1);
        chk("player 2's fruit: nothing", g.on_event(ev(events::kFruitEaten, 2), true), -1);
        chk("player 1's pellet unlocks", g.on_event(ev(events::kEnergizer, 1), true), kFirstPellet);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chk("helped game (assist rule): the pellet unlocks nothing", g.on_event(ev(events::kEnergizer), false), -1);
        chk("and the game stays ineligible even when counts() is true again", g.on_event(ev(events::kEnergizer), true), -1);
        chk("nothing unlocked", g.count(), 0);
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chk("a new game counts again", g.on_event(ev(events::kEnergizer), true), kFirstPellet);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.invalidate_game();
        chk("after invalidate_game (state loaded) nothing unlocks", g.on_event(ev(events::kEnergizer), true), -1);
    }
    {
        std::vector<Event> log = {ev(events::kGameStart, 1, 3), ev(events::kDot), ev(events::kEnergizer), ev(events::kFruitEaten)};
        Engine g;
        chk("feed(): three unlocked from a log (pellet, fruit, cherry)", g.feed(log, true), 3);
        chk("newly() holds them in order", g.newly()[0] * 100 + g.newly()[1] * 10 + g.newly()[2], kFirstPellet * 100 + kFirstFruit * 10 + kCherry);
        chk("feed() again with the same log: nothing new (cursor)", g.feed(log, true), 0);
        log.push_back(ev(events::kEnergizer));
        chk("a longer log: only the new event is looked at", g.feed(log, true), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kEnergizer), true);
        const std::string path = "/tmp/x25_ach.cfg";
        std::remove(path.c_str());
        chk("saved", g.save(path), 1);
        chk("no longer dirty", g.dirty(), 0);
        Engine h;
        bool corrupt = true;
        chk("loaded", h.load(path, &corrupt), 1);
        chk("not corrupt", corrupt, 0);
        chk("POWER UP is unlocked after loading", h.unlocked(kFirstPellet), 1);
        chk("FRUIT SALAD is not", h.unlocked(kFirstFruit), 0);
        chk("the unlock time survives", h.when(kFirstPellet) == g.when(kFirstPellet), 1);
        h.on_event(ev(events::kGameStart, 1, 3), true);
        chk("an already unlocked achievement does not unlock again after a restart", h.on_event(ev(events::kEnergizer), true), -1);
        chk("the other one still can", h.on_event(ev(events::kFruitEaten), true), kFirstFruit);
        chk("the temporary file is gone", std::ifstream(path + ".tmp").good(), 0);
    }
    {
        const std::string path = "/tmp/x25_bad.cfg";
        {
            std::ofstream f(path);
            f << "this is not\na valid file\n\x01\x02 binary junk\n";
        }
        std::remove((path + ".bad").c_str());
        Engine g;
        bool corrupt = false;
        chk("a corrupt file: load runs without a crash", g.load(path, &corrupt), 1);
        chk("reported as corrupt", corrupt, 1);
        chk("nothing unlocked", g.count(), 0);
        chk("the bad file was set aside", std::ifstream(path + ".bad").good(), 1);
        chk("the original name is free again", std::ifstream(path).good(), 0);
        chk("the game can save a new file afterwards", g.save(path), 1);
    }
    {
        const std::string path = "/tmp/x25_mixed.cfg";
        {
            std::ofstream f(path);
            f << "# comment\nunlocked first_fruit 12345\ngarbage line\nunlocked future_one 99\n";
        }
        Engine g;
        bool corrupt = true;
        g.load(path, &corrupt);
        chk("mixed file is not corrupt (one good line)", corrupt, 0);
        chk("FRUIT SALAD read", g.unlocked(kFirstFruit), 1);
        chk("time read", (long)g.when(kFirstFruit), 12345);
        g.save(path);
        std::ifstream f(path);
        std::string all((std::istreambuf_iterator<char>(f)), {});
        chk("an unknown achievement of a newer version is kept in the file", all.find("future_one") != std::string::npos, 1);
    }
    {
        Engine g;
        chk("a missing file: load says no", g.load("/tmp/x25_nonexistent.cfg"), 0);
    }
    {
        Popup p;
        chk("nothing to show", p.update(true, 0), -1);
        p.push(kFirstPellet);
        p.push(kFirstFruit);
        chk("not allowed (READY screen): nothing shown", p.update(false, 100), -1);
        chk("still waiting", (long)p.waiting(), 2);
        chk("the time does not run while it is not allowed", p.update(false, 5000), -1);
        chk("allowed: the first is shown", p.update(true, 5100), kFirstPellet);
        chk("2.9 s later it is still shown", p.update(true, 5100 + 100) == kFirstPellet, 1);
        unsigned t = 5200;
        int shown = 0;
        while (p.update(true, t) == kFirstPellet && shown < 100) {
            t += 100;
            ++shown;
        }
        chk("it is shown for about 3 seconds (30 steps of 100 ms in all)", shown >= 28 && shown <= 29, 1);
        chk("then the second one", p.update(true, t + 10), kFirstFruit);
        chk("hidden while not allowed again (menu), the time stops", p.update(false, t + 5000), -1);
        chk("comes back when allowed", p.update(true, t + 5010), kFirstFruit);
    }
    {
        Popup p;
        p.push(kFirstFruit);
        p.update(true, 0);
        chk("a long pause between updates (a frozen window) counts at most 200 ms", p.update(true, 60000), kFirstFruit);
        p.clear();
        chk("clear() removes everything", p.update(true, 60100), -1);
    }
    if (argc < 2) {
        std::printf("(no replay file: part 2 skipped)\n%d checks, %d failed\n", checks, fails);
        return fails ? 1 : 0;
    }
    std::printf("Part 2: real machine (scenarios)\n");
    std::vector<uint8_t> prog = rdf("build/rom/prog.bin");
    if (prog.empty()) {
        std::printf("needs build/rom/prog.bin\n");
        return 2;
    }
    {
        stats::Collector sc;
        Engine g;
        int unlocks[kCount] = {0};
        scenario::World w(prog, argv[1], [&](const events::Raw &r, unsigned f) {
            sc.feed(r, f, true, r.score, 0);
            g.feed(sc.log(), true);
            for (int u : g.newly()) ++unlocks[u];
        });
        chk("maze running", w.ok(), 1);
        chk("nothing unlocked at the start of the maze", g.count(), 0);
        chk("scenario: four ghosts eaten after a power pellet", w.four_ghosts_eaten(), 1);
        chk("POWER UP unlocked exactly once", unlocks[kFirstPellet], 1);
        chk("FRUIT SALAD not yet", unlocks[kFirstFruit], 0);
    }
    {
        stats::Collector sc;
        Engine g;
        int unlocks[kCount] = {0};
        scenario::World w(prog, argv[1], [&](const events::Raw &r, unsigned f) {
            sc.feed(r, f, true, r.score, 0);
            g.feed(sc.log(), true);
            for (int u : g.newly()) ++unlocks[u];
        });
        chk("scenario: fruit appears and is eaten", w.fruit_on_screen_then_eaten(), 1);
        chk("FRUIT SALAD unlocked exactly once", unlocks[kFirstFruit], 1);
        chk("POWER UP not unlocked by it (no pellet eaten)", unlocks[kFirstPellet], 0);
        w.steps(300);
        chk("still once after 300 more frames", unlocks[kFirstFruit], 1);
    }
    {
        stats::Collector sc;
        Engine g;
        int unlocks[kCount] = {0};
        scenario::World w(prog, argv[1], [&](const events::Raw &r, unsigned f) {
            sc.feed(r, f, false, r.score, 0);
            g.feed(sc.log(), false);
            for (int u : g.newly()) ++unlocks[u];
        });
        chk("helped game, scenario: four ghosts", w.four_ghosts_eaten(), 1);
        chk("nothing unlocked in a helped game", unlocks[kFirstPellet] + unlocks[kFirstFruit], 0);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
