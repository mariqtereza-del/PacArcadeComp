// X01: the game event reader. Part 1: made-up frames for every event type (no ROM). Part 2 (only if the ROM-made files exist): the real S27 replay,
// where the event counts must equal counters kept independently from RAM, plus two short scripted scenarios (fruit, round clear) made by poking
// RAM in this test program only (never in the game).  Usage: test_events [replay-file]
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include "../src/app/events.h"
#include "../src/board/machine.h"
#include "../src/app/replay.h"
using namespace events;
static int fails = 0, checks = 0;
static void chk(const char *what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what, got, want);
    }
}
struct Rig {
    Reader r;
    Raw s;
    unsigned f = 0;
    Rig() {
        s.main_state = 1;
        for (int i = 0; i < 330; ++i) step();
    } // the self-test period passes first
    void step() { r.feed(s, ++f); }
    int last_type() const { return r.log().empty() ? -1 : r.log().back().type; }
    const Event &last() const { return r.log().back(); }
};
static void unit() {
    {
        Rig g;
        g.s.main_state = 3;
        g.s.lives = 3;
        g.s.level_state = 3;
        g.step();
        chk("game start event", g.r.count(kGameStart), 1);
        chk("game start lives", g.last().value, 3);
        chk("event frame number", g.last().frame, g.f);
    }
    {
        Rig g;
        g.s.main_state = 3;
        g.s.level_state = 3;
        g.s.lives = 3;
        g.step();
        size_t n = g.r.log().size();
        g.s.dots = 1;
        g.s.score = 10;
        g.step();
        chk("dot", g.r.count(kDot), 1);
        g.s.dots = 2;
        g.s.score = 60;
        g.step();
        chk("energizer", g.r.count(kEnergizer), 1);
        chk("energizer not a dot", g.r.count(kDot), 1);
        chk("two events so far", g.r.log().size() - n, 2);
        chk("player 1", g.last().player, 1);
        g.s.score += 200;
        g.step();
        chk("ghost place 1", g.last().value, 1);
        g.s.score += 400;
        g.step();
        chk("ghost place 2", g.last().value, 2);
        g.s.score += 800;
        g.step();
        chk("ghost place 3", g.last().value, 3);
        g.s.score += 1600;
        g.step();
        chk("ghost place 4", g.last().value, 4);
        chk("ghost events", g.r.count(kGhostEaten), 4);
        g.s.death_anim = 1;
        g.step();
        chk("death", g.r.count(kDeath), 1);
        g.s.death_anim = 2;
        g.step();
        chk("death only once per episode", g.r.count(kDeath), 1);
        g.s.death_anim = 0;
        g.step();
        g.s.death_anim = 1;
        g.step();
        chk("second death", g.r.count(kDeath), 2);
        g.s.lives = 2;
        g.step();
        chk("lives event", g.last().type, kLives);
        chk("lives value", g.last().value, 2);
        g.s.level_state = 7;
        g.step();
        chk("game over", g.r.count(kGameOver), 1);
        g.step();
        chk("game over only once", g.r.count(kGameOver), 1);
    }
    {
        Rig g;
        g.s.main_state = 3;
        g.s.level_state = 3;
        g.s.lives = 3;
        g.step();
        for (int k = 0; k < 8; ++k) {
            static const int pts[] = {100, 300, 500, 700, 1000, 2000, 3000, 5000};
            g.s.fruit_pos = 0x8D80;
            g.s.fruit_idx = 6 + k;
            g.step();
            chk("fruit appeared", g.last().type, kFruitAppeared);
            chk("fruit kind on appearing", g.last().value, k);
            g.s.fruit_pos = 0;
            g.s.score += pts[k];
            g.step();
            chk("fruit eaten", g.last().type, kFruitEaten);
            chk("fruit kind eaten", g.last().value, k);
        }
        chk("8 appeared", g.r.count(kFruitAppeared), 8);
        chk("8 eaten", g.r.count(kFruitEaten), 8);
        g.s.fruit_pos = 0x8D80;
        g.s.fruit_idx = 6;
        g.step();
        g.s.fruit_pos = 0;
        g.step();
        chk("a fruit that times out is not eaten", g.r.count(kFruitEaten), 8);
        g.s.dots = 243;
        g.step();
        chk("not cleared at 243", g.r.count(kRoundCleared), 0);
        g.s.dots = 244;
        g.step();
        chk("round cleared", g.r.count(kRoundCleared), 1);
        g.s.level = 1;
        g.step();
        chk("level number event", g.last().type, kLevel);
        chk("level number (1-based)", g.last().value, 2);
        g.s.lives = 3;
        g.s.score += 10;
        g.step();
        g.s.lives = 4;
        g.s.score += 10;
        g.step();
        chk("extra life", g.r.count(kExtraLife), 1);
        chk("extra life value", g.last().value, 4);
    }
    {
        Rig g;
        g.s.main_state = 3;
        g.s.level_state = 3;
        g.s.lives = 3;
        g.step();
        g.s.player = 1;
        g.s.dots = 50;
        g.s.score = 1000;
        g.step();
        chk("player change", g.last().type, kPlayer);
        chk("player 2 value", g.last().value, 2);
        chk("player switch is not a dot", g.r.count(kDot), 0);
        g.s.dots = 51;
        g.s.score += 10;
        g.step();
        chk("P2 dot belongs to player 2", g.last().player, 2);
    }
    {
        Rig g;
        g.s.main_state = 3;
        g.s.level_state = 3;
        g.s.lives = 3;
        g.step();
        g.r.invalidate();
        g.s.dots = 100;
        g.s.score = 5000;
        g.step();
        chk("nothing judged on the first frame after invalidate", g.r.log().size(), 1);
        g.s.dots = 101;
        g.s.score = 5010;
        g.step();
        chk("judging resumes", g.r.count(kDot), 1);
    }
    {
        Reader r;
        Raw s;
        s.main_state = 3;
        s.lives = 3;
        s.level_state = 3;
        for (int i = 1; i < 200; ++i) {
            s.dots = i;
            s.score = 10 * i;
            r.feed(s, i);
        }
        chk("nothing during the power-on self-test", r.log().size(), 0);
    }
    {
        Rig g;
        g.s.main_state = 3;
        g.s.level_state = 3;
        g.s.lives = 3;
        g.step();
        g.s.dots = 1;
        g.s.score = 10;
        g.step();
        Event e = g.last();
        chk("log line", line(e) == "332 P1 DOT", 1);
        Event h{5, 2, kGhostEaten, 3};
        chk("log line ghost", line(h) == "5 P2 GHOST EATEN place 3", 1);
        Event fr{6, 1, kFruitEaten, 4};
        chk("log line fruit", line(fr) == "6 P1 FRUIT EATEN MELON", 1);
    }
}
static std::vector<uint8_t> rd(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
struct Ind {
    long dots = 0, energ = 0, deaths = 0, coins = 0, clears = 0, starts = 0, overs = 0;
}; // independent counters straight from RAM
static void replay_check(const std::vector<uint8_t> &prog, const char *file, int scenario, const char *label, Reader *out = nullptr) {
    Replay rp;
    std::string e;
    if (!rp.load(file, e)) {
        std::printf("  cannot load %s\n", file);
        ++fails;
        return;
    }
    static Machine m(prog);
    m = Machine(prog);
    Reader rd_;
    Ind in;
    auto r = [&](unsigned a) { return (int)m.bus.read(a); };
    int pd = 0, pdots = 0, pcoin = 0, pmain = 0, plst = 0, fruit_seen = 0, moved = 0, pplayer = 0;
    unsigned fr_seen = 0;
    for (uint32_t fr = 0; fr < rp.frames; ++fr) {
        uint8_t a, b;
        rp.inputs_at(fr, a, b);
        m.bus.in0 = a;
        m.bus.in1 = b;
        m.bus.dsw1 = rp.dip;
        if (scenario == 1 && fr == 700 && r(0x4E00) == 3) m.bus.write(0x4E0E, 69);  // 1 more dot and the fruit appears
        if (scenario == 2 && fr == 700 && r(0x4E00) == 3) m.bus.write(0x4E0E, 243); // 1 more dot and the round is cleared
        if (scenario == 1 && fruit_seen && fr > fr_seen + 20 && fr < fr_seen + 60) {
            m.bus.write(0x4D08, r(0x4DD2));
            m.bus.write(0x4D09, r(0x4DD3));
            m.bus.write(0x4D39, (r(0x4DD2) >> 3) + 32);
            m.bus.write(0x4D3A, (r(0x4DD3) >> 3) + 30);
            ++moved;
        }
        if (scenario == 3 && fr == 700 && r(0x4E00) == 3) {
            m.bus.write(0x4E80, 0x80);
            m.bus.write(0x4E81, 0x99);
            m.bus.write(0x4E82, 0x00);
        } // score 9,980: 2 more dots reach 10,000
        m.run_frame();
        if (scenario == 1 && !fruit_seen && fr >= 700 && r(0x4DD2)) {
            fruit_seen = 1;
            fr_seen = fr;
        }
        rd_.feed(read_raw(r), fr + 1);
        // independent counters (the self-test period, first 320 frames, is skipped as in the reader)
        if (fr + 1 > 320) {
            int d = r(0x4DA5), dots = r(0x4E0E), coin = r(0x4E6E), ms = r(0x4E00), ls = r(0x4E04), pl = r(0x4E09);
            if (d && !pd) ++in.deaths;
            if (ms == 3 && pmain == 3 && pl == pplayer && r(0x4E04) == 3 && dots == pdots + 1) ++in.dots;
            if (coin > pcoin && fr > 320) ++in.coins;
            if (ms == 3 && pmain != 3) ++in.starts;
            if (ms == 3 && ls == 7 && plst != 7) ++in.overs;
            if (dots == 244 && pdots != 244) ++in.clears;
            pd = d;
            pdots = dots;
            pcoin = coin;
            pmain = ms;
            plst = ls;
            pplayer = pl;
        } else {
            pd = r(0x4DA5);
            pdots = r(0x4E0E);
            pcoin = r(0x4E6E);
            pmain = r(0x4E00);
            plst = r(0x4E04);
            pplayer = r(0x4E09);
        }
    }
    std::printf(
        "%s: %u frames, events: dots %u energizers %u ghosts %u deaths %u coins(starts) %u fruit appeared %u eaten %u rounds cleared %u game over %u extra life %u\n",
        label, rp.frames, rd_.count(kDot) + rd_.count(kEnergizer), rd_.count(kEnergizer), rd_.count(kGhostEaten), rd_.count(kDeath), rd_.count(kGameStart),
        rd_.count(kFruitAppeared), rd_.count(kFruitEaten), rd_.count(kRoundCleared), rd_.count(kGameOver), rd_.count(kExtraLife));
    if (scenario == 0) {
        chk("S27 dots+energizers = 4E0E steps", rd_.count(kDot) + rd_.count(kEnergizer), in.dots);
        chk("S27 deaths = 4DA5 episodes", rd_.count(kDeath), in.deaths);
        chk("S27 game starts = 4E00 -> running", rd_.count(kGameStart), in.starts);
        chk("S27 game overs", rd_.count(kGameOver), in.overs);
        chk("S27 energizers (known from the S34V run)", rd_.count(kEnergizer), 2);
        chk("S27 dots (known from the S34V run: 121 steps = 119 dots + 2 energizers)", rd_.count(kDot), 119);
        chk("S27 deaths (known)", rd_.count(kDeath), 5);
    } else if (scenario == 1) {
        chk("scenario: fruit appeared once", rd_.count(kFruitAppeared), 1);
        chk("scenario: fruit eaten once", rd_.count(kFruitEaten), 1);
        for (const Event &ev : rd_.log())
            if (ev.type == kFruitAppeared || ev.type == kFruitEaten) {
                chk("scenario: level 1 fruit is the cherry", ev.value, 0);
                std::printf("  %s\n", line(ev).c_str());
            }
    } else if (scenario == 2) {
        chk("scenario: round cleared once", rd_.count(kRoundCleared), 1);
        chk("round clear = 4E0E reaching 244", rd_.count(kRoundCleared), in.clears);
        chk("scenario: next level number event", rd_.count(kLevel) >= 1, 1);
        for (const Event &ev : rd_.log())
            if (ev.type == kRoundCleared || ev.type == kLevel) std::printf("  %s\n", line(ev).c_str());
    } else if (scenario == 3) {
        chk("scenario: extra life once (score reaches 10,000)", rd_.count(kExtraLife), 1);
        for (const Event &ev : rd_.log())
            if (ev.type == kExtraLife) {
                chk("extra life: lives become 4", ev.value, 4);
                std::printf("  %s\n", line(ev).c_str());
            }
    }
    if (out) *out = rd_;
}
int main(int argc, char **argv) {
    std::printf("X01 events: unit test on made-up frames\n");
    unit();
    std::printf("  %d checks so far, %d failed\n", checks, fails);
    std::vector<uint8_t> prog = rd("build/rom/prog.bin");
    if (prog.empty() || argc < 2) {
        std::printf("  (no ROM files or replay given: real-replay part skipped)\n");
        return fails ? 1 : 0;
    }
    replay_check(prog, argv[1], 0, "S27 replay");
    replay_check(prog, argv[1], 1, "S27 replay + fruit poke");
    replay_check(prog, argv[1], 2, "S27 replay + round-clear poke");
    replay_check(prog, argv[1], 3, "S27 replay + score poke (extra life)");
    std::printf("%d checks, %d failed\n", checks, fails);
    std::printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails ? 1 : 0;
}
