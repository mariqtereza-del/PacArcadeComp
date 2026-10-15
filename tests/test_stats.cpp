// X22: unit test of src/app/stats.h on made-up frames (no ROM): every counter, the assist rule, two players, the time text.
#include <cstdio>
#include <string>
#include "../src/app/stats.h"
using namespace stats;
using events::Raw;
static int checks = 0, fails = 0;
static void chk(const std::string &what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what.c_str(), got, want);
    } else std::printf("  ok: %s\n", what.c_str());
}
struct Rig {
    Collector c;
    Raw s;
    unsigned f = 0;
    int sc1 = 0, sc2 = 0;
    bool counts = true, over = false;
    Rig() {
        s.main_state = 1;
        for (int i = 0; i < 330; ++i) step();
    }
    void step() { over = c.feed(s, ++f, counts, sc1, sc2); }
    void start() {
        s.main_state = 3;
        s.level_state = 3;
        s.lives = 3;
        s.level = 0;
        s.dots = 0;
        step();
    }
    void dot() {
        s.dots++;
        sc1 += 10;
        s.score = sc1 + sc2;
        step();
    }
    void energizer() {
        s.dots++;
        sc1 += 50;
        s.score = sc1 + sc2;
        step();
    }
    void ghost(int pts) {
        sc1 += pts;
        s.score = sc1 + sc2;
        step();
    }
    void game_over() {
        s.level_state = 7;
        step();
    }
};
int main() {
    {
        Rig g;
        g.start();
        for (int i = 0; i < 5; ++i) g.dot();
        g.energizer();
        g.ghost(200);
        g.ghost(400);
        g.ghost(800);
        g.energizer();
        g.ghost(200);
        g.s.fruit_pos = 0x1234;
        g.s.fruit_idx = 6;
        g.step();
        g.s.fruit_pos = 0;
        g.sc1 += 100;
        g.s.score = g.sc1;
        g.step(); // a cherry appears and is eaten
        g.s.death_anim = 1;
        g.step();
        g.s.death_anim = 0;
        g.step();
        g.s.level = 1;
        g.step();
        g.s.lives = 4;
        g.sc1 += 0;
        g.s.score = g.sc1;
        g.sc1 += 10;
        g.s.score = g.sc1;
        g.step(); // level 2; lives +1 together with a score step = extra life
        for (int i = 0; i < 100; ++i) g.step();
        const Run &r = g.c.run(0);
        chk("dots (a dot is +10)", r.dots, 5);
        chk("energizers", r.energizers, 2);
        chk("ghosts eaten", r.ghosts, 4);
        chk("longest chain is 3 (the second chain has only one)", r.chain, 3);
        chk("fruit eaten", r.fruit, 1);
        chk("deaths", r.deaths, 1);
        chk("level reached", r.level, 2);
        chk("extra lives", r.extra_lives, 1);
        chk("score", r.score, g.sc1);
        chk("still counted", r.counted, 1);
        chk("active", r.active, 1);
        chk("time: 100+ machine frames are about 2 seconds", r.seconds(), 2);
        g.game_over();
        chk("game over reported", g.over, 1);
        const Summary &s = g.c.take();
        chk("summary: player 1", s.player, 1);
        chk("summary: one player", s.two_players, 0);
        chk("summary: ghosts", s.run.ghosts, 4);
        chk("summary: score", s.run.score, g.sc1);
        chk("run no longer active", g.c.run(0).active, 0);
        chk("exactly the events' own counts: dots", g.c.count(events::kDot), 5);
    }
    {
        Rig g;
        g.start();
        g.dot();
        g.dot();
        g.counts = false;
        g.dot();
        g.dot();
        g.counts = true;
        g.dot();
        const Run &r = g.c.run(0);
        chk("assisted: counted turns false", r.counted, 0);
        chk("assisted: numbers frozen at the moment the game stopped counting", r.dots, 2);
        chk("assisted: stays frozen even when counts() is true again", g.c.run(0).dots, 2);
        g.game_over();
        chk("assisted: game over still reports a summary", g.over, 1);
        chk("assisted summary says not counted", g.c.take().run.counted, 0);
        g.s.main_state = 1;
        g.step();
        g.start();
        chk("a new game counts again", g.c.run(0).counted, 1);
        g.dot();
        chk("and collects again from zero", g.c.run(0).dots, 1);
    }
    {
        Rig g;
        g.start();
        g.dot();
        g.s.player = 1;
        g.step();
        chk("player 2 plays: two players seen", g.c.run(1).active, 1);
        g.sc2 = 0;
        g.s.dots = 0;
        g.step();
        g.game_over();
        chk("two players: summary is for player 2", g.c.take().player, 2);
        chk("two players flag", g.c.take().two_players, 1);
        chk("player 1's run kept its dot", g.c.run(0).dots, 1);
    }
    {
        Rig g;
        g.start();
        g.dot();
        g.c.invalidate(true);
        chk("power-on restart drops the runs", g.c.run(0).active, 0);
    }
    chk("time text 155 s", time_text(155) == "2M 35S", 1);
    chk("time text 5 s", time_text(5) == "0M 05S", 1);
    {
        Run r;
        r.frames = 3637;
        chk("3637 frames = 60 seconds", r.seconds(), 60);
    }
    std::printf("%d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
