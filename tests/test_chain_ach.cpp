// X27: the ghost-chain achievements. Part 1: made-up events (chains per round, reset by a new round, fruit order, helped game, player 2, test mode). Part 2: the REAL ROM (tests/scenario.h):
// four ghosts are eaten on one pellet (the game's own scoring runs: 200, 400, 800, 1600), again and again in one round (the pellet's tile is put back in video RAM between chains, a test-only poke
// like the dot in the fruit test), then two fruits in the same round. Usage: test_chain_ach S27-replay-file
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
static void chain(Engine &g, bool c = true) {
    for (int p = 1; p <= 4; ++p) { g.on_event(ev(events::kGhostEaten, 1, p), c); }
}
static int ids5(const Engine &g) {
    return g.unlocked(kChain1) * 1 + g.unlocked(kChain2) * 2 + g.unlocked(kChain3) * 4 + g.unlocked(kChain4) * 8 + g.unlocked(kChainFruit) * 16;
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
    int chains() {
        int n = 0;
        for (const Event &e : sc.log())
            if (e.type == events::kGhostEaten && e.value == 4) ++n;
        return n;
    }
    // One full chain on the top-left pellet: wait until the ghosts are normal again, put the pellet back, eat it, then the four ghosts one after the other.
    // While the test waits, Pac-Man is kept on whichever of two maze tiles is farther from the ghosts (the test does not play, so he must not be caught).
    void safe_spot() {
        const scenario::Tile c[2] = {scenario::kBesideEnergizer, scenario::kDotRight};
        int best = 0, bd = -1;
        for (int k = 0; k < 2; ++k) {
            int md = 100000;
            const int py = (c[k].a - 32) * 8 + 4, px = (c[k].b - 30) * 8 + 4;
            for (int g = 0; g < 4; ++g) {
                int d = std::abs(w.rd(0x4D00 + 2 * g) - py) + std::abs(w.rd(0x4D01 + 2 * g) - px);
                if (d < md) md = d;
            }
            if (md > bd) {
                bd = md;
                best = k;
            }
        }
        w.place_pac(c[best]);
    }
    bool one_chain() {
        // after a chain the eaten ghosts need about 800 frames to come back out of the house before they can be eaten again; Pac-Man is kept away from them meanwhile
        for (int f = 0; f < (chains() ? 900 : 30); ++f) {
            safe_spot();
            w.step();
        }
        for (int f = 0; f < 3000 && w.rd(0x4DA6); ++f) {
            safe_spot();
            w.step();
        }
        const int before = chains();
        w.wr(scenario::video_addr(scenario::kEnergizerTopLeft), 0x14);
        w.four_ghosts_eaten();
        return chains() == before + 1;
    }
    bool eat_fruit(int dots) {
        w.wr(scenario::video_addr(scenario::kDotRight), 0x10);
        w.wr(0x4E0E, dots);
        for (int f = 0; f < 1500 && w.rd(0x4DD2) == 0; ++f) {
            w.place_pac(scenario::kDotRight);
            w.step();
        }
        if (w.rd(0x4DD2) == 0) return false;
        const unsigned before = sc.count(events::kFruitEaten);
        for (int a = 0; a < 6 && w.rd(0x4DD2) != 0; ++a) {
            w.place_pac({(w.rd(0x4DD2) >> 3) + 32, (w.rd(0x4DD3) >> 3) + 30 + 3});
            for (int k = 0; k < 60 && w.rd(0x4DD2) != 0; ++k) w.step_held(0x04);
        }
        return sc.count(events::kFruitEaten) > before;
    }
};
int main(int argc, char **argv) {
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int p = 1; p <= 3; ++p) { g.on_event(ev(events::kGhostEaten, 1, p), true); }
        chk("three ghosts on one pellet: nothing", g.count(), 0);
        g.on_event(ev(events::kGhostEaten, 1, 4), true);
        chk("the fourth ghost: GHOST BUSTER only", ids5(g), 1);
        chk("  GHOST BUSTER is not FIRST PELLET (no events of those)", g.unlocked(kFirstPellet), 0);
        chain(g);
        chk("second chain in the same round: DOUBLE CHAIN", ids5(g), 3);
        chain(g);
        chk("third: TRIPLE CHAIN", ids5(g), 7);
        chain(g);
        chk("fourth: CHAIN REACTION", ids5(g), 15);
        chk("no GHOST FEAST without fruit", g.unlocked(kChainFruit), 0);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        chk("one fruit: not yet", g.unlocked(kChainFruit), 0);
        chk("the second fruit completes GHOST FEAST", g.on_event(ev(events::kFruitEaten, 1, 0), true) >= 0 && g.unlocked(kChainFruit), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        chain(g);
        chain(g);
        chain(g);
        chk("fruit first, 3 chains: no GHOST FEAST", g.unlocked(kChainFruit), 0);
        chain(g);
        chk("the fourth chain completes GHOST FEAST", g.unlocked(kChainFruit), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kLevel, 1, 1), true);
        chain(g);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        g.on_event(ev(events::kLevel, 1, 2), true);
        chain(g);
        chk("one chain in round 1 and one in round 2: no DOUBLE CHAIN", ids5(g), 1);
        chain(g);
        chk("a second chain in round 2: DOUBLE CHAIN", ids5(g), 3);
        g.on_event(ev(events::kLevel, 1, 3), true);
        chain(g);
        chain(g);
        chain(g);
        chk("three chains in round 3: TRIPLE, not REACTION", ids5(g), 7);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chain(g);
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chain(g);
        chk("a new game starts the count again", g.unlocked(kChain2), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        chain(g, true);
        chain(g, false);
        chk("a helped game: only the chain before the help", ids5(g), 1);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        for (int i = 0; i < 4; ++i) g.on_event(ev(events::kGhostEaten, 2, 4), true);
        chk("player 2: nothing", g.count(), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 5), true);
        chain(g);
        chk("5 lives: nothing", g.count(), 0);
    }
    for (int i = kChain1; i <= kChainFruit; ++i) {
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
    // One real chain: the game's own scoring gives the four ghost events with places 1 to 4 (200, 400, 800, 1600). The game cannot be driven through a second chain in the same machine run by the test's pokes
    // (the eaten ghosts come back out of the house after about 800 frames and can catch the unplayed Pac-Man), so each real chain comes from its own real run; the events they produced are then played into
    // ONE engine as one round (documented limitation, same method as FRUIT BASKET in X26).
    std::vector<Event> chain_ev;
    bool chain_ok = true;
    for (int i = 0; i < 4; ++i) {
        Rig r(prog, argv[1]);
        chain_ok = chain_ok && r.one_chain() && r.unlocks[kChain1] == 1 && r.g.count() == 2; // POWER UP (the pellet) and GHOST BUSTER
        int places = 0;
        for (const Event &e : r.sc.log())
            if (e.type == events::kGhostEaten) {
                chain_ev.push_back(e);
                places = places * 10 + e.value;
            }
        chain_ok = chain_ok && places == 1234;
    }
    chk("four real runs: each gives a real chain (places 1 2 3 4) and unlocks only POWER UP and GHOST BUSTER there", chain_ok, 1);
    std::vector<Event> fruit_ev, lvl_ev;
    {
        Rig r(prog, argv[1]);
        bool a = r.eat_fruit(69), b = r.eat_fruit(169);
        chk("one real run: both fruits of a round are eaten", a && b, 1);
        for (const Event &e : r.sc.log()) {
            if (e.type == events::kFruitEaten) fruit_ev.push_back(e);
        }
        chk("  two real fruit events", (long)fruit_ev.size(), 2);
    }
    {
        Rig r(prog, argv[1]);
        bool ok = r.w.last_dot_then_next_level();
        chk("one real run: a round is cleared and the next begins", ok, 1);
        for (const Event &e : r.sc.log())
            if (e.type == events::kRoundCleared || e.type == events::kLevel) lvl_ev.push_back(e);
        int cl = 0, lv = 0;
        for (const Event &e : lvl_ev) {
            cl += e.type == events::kRoundCleared;
            lv += e.type == events::kLevel;
        }
        chk("  real ROUND CLEARED and LEVEL events obtained", cl >= 1 && lv >= 1, 1);
    }
    if (chain_ev.size() == 16 && fruit_ev.size() == 2) {
        auto play = [&](Engine &g, const std::vector<Event> &v, size_t from, size_t to) {
            for (size_t i = from; i < to; ++i) {
                Event e = v[i];
                e.player = 1;
                g.on_event(e, true);
            }
        };
        {
            Engine g;
            g.on_event(ev(events::kGameStart, 1, 3), true);
            play(g, chain_ev, 0, 4);
            chk("real chain 1: GHOST BUSTER only", ids5(g), 1);
            play(g, chain_ev, 4, 8);
            chk("real chain 2: DOUBLE CHAIN", ids5(g), 3);
            play(g, chain_ev, 8, 12);
            chk("real chain 3: TRIPLE CHAIN", ids5(g), 7);
            play(g, chain_ev, 12, 16);
            chk("real chain 4: CHAIN REACTION", ids5(g), 15);
            play(g, fruit_ev, 0, 1);
            chk("real first fruit: GHOST FEAST not yet", g.unlocked(kChainFruit), 0);
            play(g, fruit_ev, 1, 2);
            chk("real second fruit: GHOST FEAST", g.unlocked(kChainFruit), 1);
            chk("everything unlocked exactly once: five chain achievements, FRUIT SALAD and CHERRY PICKER", g.count(), 7);
        }
        {
            Engine g;
            g.on_event(ev(events::kGameStart, 1, 3), true);
            play(g, chain_ev, 0, 4);
            for (const Event &e : lvl_ev) {
                Event q = e;
                q.player = 1;
                g.on_event(q, true);
            }
            play(g, chain_ev, 4, 8);
            chk("real chain, then the game's own round change, then a real chain: no DOUBLE CHAIN", ids5(g), 1);
            play(g, chain_ev, 8, 12);
            chk("  a second chain in the new round: DOUBLE CHAIN", ids5(g), 3);
        }
        {
            Engine g;
            g.on_event(ev(events::kGameStart, 1, 3), true);
            for (size_t i = 0; i < 16; ++i) {
                Event e = chain_ev[i];
                e.player = 1;
                g.on_event(e, i < 4);
            }
            chk("helped game, real events: only the first real chain counted", ids5(g), 1);
        }
    }
    {
        Rig r(prog, argv[1]);
        r.counts = false;
        bool ok = r.one_chain();
        chk("helped game, real ROM: the chain happens", ok, 1);
        chk("  nothing unlocks", r.g.count(), 0);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
