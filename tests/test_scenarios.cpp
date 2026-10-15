// X02: every scenario of tests/scenario.h on the real ROM, judged by the events of src/app/events.h (X01) and by the game's own RAM.
#include <cstdio>
#include <functional>
#include "scenario.h"
using namespace events;
static int fails = 0, checks = 0;
static void chk(const char *what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what, got, want);
    } else std::printf("  ok: %s (%ld)\n", what, got);
}
static std::vector<uint8_t> rdf(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static void dump(const scenario::World &w) {
    for (const Event &e : w.reader().log())
        if (e.type != kDot && e.type != kLives) std::printf("    %s\n", line(e).c_str());
}
int main(int argc, char **argv) {
    std::vector<uint8_t> prog = rdf("build/rom/prog.bin");
    if (prog.empty() || argc < 2) {
        std::printf("needs build/rom/prog.bin and the S27 replay file as argument\n");
        return 2;
    }
    std::printf("Scenario FRUIT (dot counter 69, then Pac-Man on the fruit)\n");
    {
        scenario::World w(prog, argv[1]);
        chk("maze running", w.ok(), 1);
        bool ok = w.fruit_on_screen_then_eaten();
        chk("fruit came and was eaten", ok, 1);
        dump(w);
        chk("FRUIT APPEARED once", w.reader().count(kFruitAppeared), 1);
        chk("FRUIT EATEN once", w.reader().count(kFruitEaten), 1);
        for (const Event &e : w.reader().log())
            if (e.type == kFruitEaten) chk("level 1 fruit is the cherry", e.value, 0);
        chk("score rose by 100 for the cherry (RAM)", w.score() >= 100, 1);
    }
    std::printf("Scenario GHOSTS (energizer, then four ghosts on Pac-Man)\n");
    {
        scenario::World w(prog, argv[1]);
        bool ok = w.four_ghosts_eaten();
        chk("four ghosts eaten", ok, 1);
        dump(w);
        chk("ENERGIZER seen", w.reader().count(kEnergizer), 1);
        chk("GHOST EATEN x4", w.reader().count(kGhostEaten), 4);
        chk("score = 50 (energizer) + 200 + 400 + 800 + 1600 + dots (RAM)", w.score() - 3050 >= 0 && w.score() - 3050 < 100, 1);
        int place = 0;
        for (const Event &e : w.reader().log())
            if (e.type == kGhostEaten) chk("ghost chain place in order", e.value, ++place);
    }
    std::printf("Scenario ROUND (one dot left)\n");
    {
        scenario::World w(prog, argv[1]);
        bool ok = w.last_dot_then_next_level();
        chk("round cleared and next level started", ok, 1);
        dump(w);
        chk("ROUND CLEARED once", w.reader().count(kRoundCleared), 1);
        for (const Event &e : w.reader().log())
            if (e.type == kLevel) chk("LEVEL event says 2", e.value, 2);
        chk("RAM level counter 4E13 = 1", w.rd(0x4E13), 1);
        chk("dot counter restarted (small number)", w.rd(0x4E0E) < 20, 1);
    }
    std::printf("Scenario LEVEL 17\n");
    {
        scenario::World w(prog, argv[1]);
        bool ok = w.level_17();
        chk("level 17 reached", ok, 1);
        dump(w);
        int last = 0;
        for (const Event &e : w.reader().log())
            if (e.type == kLevel) last = e.value;
        chk("the last LEVEL event says 17 (an earlier one, 16, is the test's own poke of 4E13)", last, 17);
        chk("RAM level counter 4E13 = 16", w.rd(0x4E13), 16);
        std::printf("    level-order pointer 4E0A = %02X (level 1 is 68, level 2 69, one more per round)\n", w.rd(0x4E0A));
        chk("level-order pointer 4E0A moved on by one round (0x68 + 16)", w.rd(0x4E0A), 0x68 + 16);
        chk("the entry it points to (ROM 0078) is 16 = the 17th level record", prog[w.rd(0x4E0A)], 16);
        w.wr(0x4E0E, 69);
        w.place_pac(scenario::kDotRight);
        int f = 0;
        while (f < 1500 && w.rd(0x4DD2) == 0) {
            w.place_pac(scenario::kDotRight);
            w.step();
            ++f;
        }
        chk("fruit shown on level 17 is the key (points index 13)", w.rd(0x4DD4), 13);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
