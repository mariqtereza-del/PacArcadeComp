// X24: personal bests. Part 1: the update rules, the file and the page lines on made-up numbers. Part 2: the REAL ROM (tests/scenario.h): a level is cleared by the game's own code
// (one dot left, Pac-Man eats it); the clears reported by the statistics collector (X22) feed the records: a first clear sets a record, a better run beats it, a worse run does not,
// a helped game (assist rule) never counts.  Usage: test_records S27-replay-file
#include <cstdio>
#include <fstream>
#include <string>
#include "../src/app/records.h"
#include "../src/app/stats.h"
#include "scenario.h"
static int fails = 0, checks = 0;
static void chk(const std::string &what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what.c_str(), got, want);
    } else std::printf("  ok: %s\n", what.c_str());
}
static std::vector<uint8_t> rdf(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

struct Played {
    bool ok = false;
    std::vector<stats::Clear> clears;
};
// One real game: the maze runs, `delay` frames pass, the score is set to `score` points (if > 0), one dot is left and Pac-Man eats it. counts = the assist rule.
static Played play(const std::vector<uint8_t> &prog, const char *replay, int delay, int score_bcd_mid, bool counts) {
    stats::Collector sc;
    Played p;
    scenario::World w(prog, replay, [&](const events::Raw &r, unsigned f) {
        const int s = r.score;
        sc.feed(r, f, counts, s, 0);
        for (const stats::Clear &c : sc.take_clears()) p.clears.push_back(c);
    });
    if (!w.ok()) return p;
    {
        const int y = w.rd(0x4D08), x = w.rd(0x4D09), ta = w.rd(0x4D39),
                  tb = w.rd(0x4D3A); // the delay: Pac-Man is held where he is (so he eats nothing more); the clock runs
        for (int i = 0; i < delay; ++i) {
            w.wr(0x4D08, y);
            w.wr(0x4D09, x);
            w.wr(0x4D39, ta);
            w.wr(0x4D3A, tb);
            w.step();
        }
    }
    if (score_bcd_mid) {
        w.wr(0x4E80, 0x00);
        w.wr(0x4E81, score_bcd_mid);
        w.wr(0x4E82, 0x00);
    }
    p.ok = w.last_dot_then_next_level();
    return p;
}
int main(int argc, char **argv) {
    using namespace records;
    std::printf("Part 1: rules\n");
    {
        Table t;
        chk("no records at first", t.any(), 0);
        Result r = t.update(1, 1000, 3000);
        chk("first clear of level 1: first record", r.first, 1);
        chk("score stored", t.get(1).score, 1000);
        chk("frames stored", t.get(1).frames, 3000);
        r = t.update(1, 900, 3500);
        chk("worse score and slower: nothing", r.first + r.new_score + r.new_time, 0);
        chk("record unchanged", t.get(1).score * 10000L + t.get(1).frames, 1000L * 10000 + 3000);
        r = t.update(1, 1200, 3500);
        chk("better score only: NEW BEST score", r.new_score && !r.new_time, 1);
        chk("score now 1200, time still 3000", t.get(1).score * 10000L + t.get(1).frames, 1200L * 10000 + 3000);
        r = t.update(1, 1100, 2900);
        chk("faster only: NEW BEST time", !r.new_score && r.new_time, 1);
        chk("time now 2900, score still 1200", t.get(1).score * 10000L + t.get(1).frames, 1200L * 10000 + 2900);
        r = t.update(1, 5000, 2000);
        chk("both better", r.new_score && r.new_time, 1);
        r = t.update(1, 5000, 2000);
        chk("equal is not a new best", r.new_score + r.new_time, 0);
        chk("levels are separate: level 2 still empty", t.get(2).score, 0);
        r = t.update(2, 10, 10);
        chk("level 2 first record", r.first, 1);
        r = t.update(30, 777, 100);
        chk("level 30 is kept as 21", r.level, 21);
        chk("level 21 holds it", t.get(21).score, 777);
        chk("level 25 reads the same entry", t.get(25).score, 777);
        r = t.update(0, 5, 5);
        chk("level 0 is kept as 1", r.level, 1);
        r = t.update(3, 0, 100);
        chk("zero score is not a record", r.first + r.new_score + r.new_time, 0);
        chk("level 3 stays empty", t.get(3).score, 0);
        const std::string path = "/tmp/x24_records.cfg";
        chk("saved", t.save(path), 1);
        Table u;
        chk("loaded", u.load(path), 1);
        bool same = true;
        for (int l = 1; l <= 21; ++l) same = same && t.get(l).score == u.get(l).score && t.get(l).frames == u.get(l).frames;
        chk("file round trip: every level equal", same, 1);
        chk("the temporary file is gone", std::ifstream("/tmp/x24_records.cfg.tmp").good(), 0);
        {
            std::ofstream f("/tmp/x24_bad.cfg");
            f << "garbage\nlevel 99 score 5 frames 5\nlevel 4 score -3 frames 5\nlevel 5 score 50 frames 60\nlevel x\n";
        }
        Table b;
        b.load("/tmp/x24_bad.cfg");
        chk("bad lines ignored, good line kept", b.get(5).score, 50);
        chk("bad level and negative score ignored", b.get(4).score + b.get(21).score, 0);
        Table n;
        chk("a missing file is not an error for the game (load says no)", n.load("/tmp/x24_nonexistent.cfg"), 0);
    }
    chk("time text: 3637 frames = 1M 00S", time_text(3637) == "1M 00S", 1);
    chk("time text: 2740 frames = 0M 45S", time_text(2740) == "0M 45S", 1);
    {
        Entry e;
        e.score = 12340;
        e.frames = 2740;
        const std::string l = line(1, e);
        chk("page line: " + l, l == "01   12340  0M 45S", 1);
        chk("page line fits 24 characters", l.size() <= 24, 1);
        chk("empty level line", line(3, Entry()) == "03 -", 1);
        chk("level 21 line", line(21, Entry()) == "21+ -", 1);
        chk("level 21 line with record fits", line(21, e).size() <= 24, 1);
    }
    if (argc < 2) {
        std::printf("(no replay file: part 2 skipped)\n");
        return fails ? 1 : 0;
    }
    std::printf("Part 2: real machine\n");
    std::vector<uint8_t> prog = rdf("build/rom/prog.bin");
    if (prog.empty()) {
        std::printf("needs build/rom/prog.bin\n");
        return 2;
    }
    Table t;
    Played a = play(prog, argv[1], 60, 0, true);
    chk("A: a level was cleared by the game's own code", a.ok, 1);
    chk("A: exactly one clear reported", (long)a.clears.size(), 1);
    if (a.clears.size() == 1) {
        const stats::Clear &c = a.clears[0];
        std::printf("    A: level %d score %d frames %u\n", c.level, c.score, c.frames);
        chk("A: it is level 1", c.level, 1);
        chk("A: the time includes the delay (more than 60 frames)", c.frames > 60, 1);
        Result r = t.update(c.level, c.score, c.frames);
        chk("A: first record (score above 0)", r.first, 1);
    }
    const Entry ea = t.get(1);
    Played b = play(prog, argv[1], 0, 0x50, true);
    chk("B (faster, 5,000 points): clear reported", (long)b.clears.size(), 1);
    if (b.clears.size() == 1) {
        const stats::Clear &c = b.clears[0];
        std::printf("    B: level %d score %d frames %u\n", c.level, c.score, c.frames);
        chk("B: score is the poked 5,000 plus the last dot", c.score >= 5000 && c.score < 5100, 1);
        chk("B: faster than A", c.frames < ea.frames, 1);
        Result r = t.update(c.level, c.score, c.frames);
        chk("B: NEW BEST score and time", r.new_score && r.new_time, 1);
        chk("B: the records hold B's numbers", t.get(1).score == c.score && t.get(1).frames == c.frames, 1);
    }
    const Entry eb = t.get(1);
    Played w = play(prog, argv[1], 120, 0, true);
    chk("C (slower, lower score): clear reported", (long)w.clears.size(), 1);
    if (w.clears.size() == 1) {
        const stats::Clear &c = w.clears[0];
        std::printf("    C: level %d score %d frames %u\n", c.level, c.score, c.frames);
        Result r = t.update(c.level, c.score, c.frames);
        chk("C: no new best", r.first + r.new_score + r.new_time, 0);
        chk("C: the record is unchanged", t.get(1).score == eb.score && t.get(1).frames == eb.frames, 1);
    }
    Played h = play(prog, argv[1], 0, 0x90, false);
    chk("D (helped game, score 9,000): the level was cleared but NO clear is reported", h.ok && h.clears.empty(), 1);
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
