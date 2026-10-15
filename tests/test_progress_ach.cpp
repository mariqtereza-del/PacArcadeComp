// X29: (1) the save-state rule for achievements: a loaded state does not stop them (records and statistics are still stopped), rewind still does;
// (2) the progress counts of the achievements that have one (fruit basket, chains, clean rounds, round numbers), and how a loaded state restarts them. (The progress line on the page is checked in tests/test_menu.cpp.)
#include <cstdio>
#include <cstring>
#include <string>
#include "../src/app/achievements.h"
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
static int have(const Engine &g, int i) {
    int h = -1, n = -1;
    g.progress(i, &h, &n);
    return h;
}
static int need(const Engine &g, int i) {
    int h = -1, n = -1;
    g.progress(i, &h, &n);
    return n;
}
int main() {
    // 1. the rule
    {
        assist::Rule r;
        chk("no help: counts for achievements", counts_for_achievements(r.mask()), 1);
        r.mark(assist::kLoad);
        chk("a loaded state: still counts for achievements", counts_for_achievements(r.mask()), 1);
        chk("  but the game no longer counts for records and statistics", r.counts(), 0);
        r.mark(assist::kRewind);
        chk("a loaded state and a rewind: does not count", counts_for_achievements(r.mask()), 0);
        assist::Rule s;
        s.mark(assist::kStartLevel);
        chk("start-at-level help: not for achievements", counts_for_achievements(s.mask()), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kFruitEaten, 1, 0), true); // a state is loaded: the game goes on
        g.resume_after_load(true, 4);
        chk("after a load the game is still eligible", g.eligible(), 1);
        g.on_event(ev(events::kEnergizer, 1, 0), true);
        chk("an energizer after the load unlocks POWER UP", g.unlocked(kFirstPellet), 1);
        for (int p = 1; p <= 4; ++p) { g.on_event(ev(events::kGhostEaten, 1, p), true); }
        chk("a chain after the load unlocks GHOST BUSTER", g.unlocked(kChain1), 1);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        chk("the round counter was set from the loaded round (4): clearing it unlocks no earlier round", g.unlocked(kRound2) + g.unlocked(kRound5), 0);
        Engine h;
        h.resume_after_load(false, 1);
        h.on_event(ev(events::kEnergizer, 1, 0), true);
        chk("a state loaded outside a running game (or with 5 lives, or as player 2): nothing", h.count(), 0);
        Engine k;
        k.on_event(ev(events::kGameStart, 1, 3), true);
        k.resume_after_load(true, 1);
        k.on_event(ev(events::kGameStart, 1, 3), true);
        k.on_event(ev(events::kEnergizer, 1, 0), true);
        chk("a new game after a load starts normally", k.unlocked(kFirstPellet), 1);
    }
    // 2. progress
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kLevel, 1, 1), true);
        chk("FRUIT BASKET has a count of 5", need(g, kFiveFruits), 5);
        chk("  0 at the start", have(g, kFiveFruits), 0);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        g.on_event(ev(events::kFruitEaten, 1, 3), true);
        chk("  two different kinds (a repeat does not count): 2", have(g, kFiveFruits), 2);
        chk("DOUBLE, TRIPLE and REACTION count the chains of this round (need 2, 3, 4)", need(g, kChain2) * 100 + need(g, kChain3) * 10 + need(g, kChain4), 234);
        for (int p = 1; p <= 4; ++p) { g.on_event(ev(events::kGhostEaten, 1, p), true); }
        chk("one chain: 1 of each", have(g, kChain2) * 100 + have(g, kChain3) * 10 + have(g, kChain4), 111);
        chk("GHOST FEAST counts chains and fruits together (need 6): 1 chain + 2 fruits = 3", have(g, kChainFruit) * 10 + need(g, kChainFruit), 36);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        g.on_event(ev(events::kLevel, 1, 2), true);
        chk("a new round starts the chain count again", have(g, kChain2), 0);
        chk("PEACEFUL RUN (need 3): the round with a ghost was not clean: 0", have(g, kPeaceful) * 10 + need(g, kPeaceful), 3);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        chk("  a clean round 2: 1", have(g, kPeaceful), 1);
        g.on_event(ev(events::kLevel, 1, 3), true);
        g.on_event(ev(events::kRoundCleared, 1, 0), true);
        chk("  a clean round 3: 2", have(g, kPeaceful), 2);
        g.on_event(ev(events::kLevel, 1, 4), true);
        chk("round achievements: need = the round number, have = rounds cleared (level - 1): in round 4", have(g, kRound5) * 100 + need(g, kRound5), 305);
        chk("  WARMING UP is already unlocked (round 2 cleared), its count is capped", have(g, kRound2) * 10 + need(g, kRound2), 22);
        chk("achievements without a count: need 0", need(g, kFirstPellet) + need(g, kCherry) + need(g, kChain1), 0);
    }
    {
        Engine g;
        chk("outside a game every progress is 0", have(g, kPeaceful) + have(g, kRound5) + have(g, kFiveFruits), 0);
    }
    {
        Engine g;
        g.on_event(ev(events::kGameStart, 1, 3), true);
        g.on_event(ev(events::kFruitEaten, 1, 0), true);
        g.on_event(ev(events::kFruitEaten, 1, 1), true);
        chk("two kinds before the load", have(g, kFiveFruits), 2);
        g.resume_after_load(true, 3);
        chk("a load restarts the counts (the old timeline is gone): 0", have(g, kFiveFruits), 0);
        chk("  the round count is the loaded round's: level 3 means 2 rounds cleared", have(g, kRound5), 2);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
