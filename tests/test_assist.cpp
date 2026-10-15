// X03: the shared assisted-game rule (src/app/assist.h): each reason turns counting off, a new game turns it on, the sentence and labels are right.
// The high-score behaviour itself is covered, unchanged, by tests/test_state.sh (S33).
#include <cstdio>
#include <set>
#include <string>
#include "../src/app/assist.h"
static int fails = 0, checks = 0;
static void chk(const char *what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what, got, want);
    }
}
static void chks(const char *what, const std::string &got, const std::string &want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got \"%s\", wanted \"%s\")\n", what, got.c_str(), want.c_str());
    }
}
int main() {
    using namespace assist;
    {
        Rule r;
        chk("a fresh game counts", r.counts(), 1);
        chks("no sentence while it counts", r.sentence(), "");
        chks("no label while it counts", r.label(), "");
    }
    std::set<std::string> labels;
    for (int i = 0; i < kReasonCount; ++i) {
        Rule r;
        r.mark((Reason)i);
        chk("reason turns counting off", r.counts(), 0);
        chk("exactly that reason", r.has((Reason)i), 1);
        chk("one reason", r.reasons(), 1);
        chk("label is 1..12 letters", info(i).label[0] != 0 && std::string(info(i).label).size() <= 12, 1);
        chk("sentence names it", r.sentence().find(info(i).phrase) != std::string::npos, 1);
        chks("label", r.label(), info(i).label);
        labels.insert(info(i).label);
        r.new_game();
        chk("a new game counts again", r.counts(), 1);
        chk("reason gone after the new game", r.has((Reason)i), 0);
    }
    chk("every reason has its own label", (long)labels.size(), kReasonCount);
    {
        Rule r;
        r.mark(kLoad);
        r.mark(kLoad);
        chk("marking twice is one reason", r.reasons(), 1);
    }
    {
        Rule r;
        r.mark(kLoad);
        chks("sentence, one reason", r.sentence(), "ASSISTED: this game loaded a save state, so it does not count for records.");
    }
    {
        Rule r;
        r.mark(kRewind);
        r.mark(kLoad);
        chks("sentence, two reasons (table order)", r.sentence(), "ASSISTED: this game loaded a save state and rewound the game, so it does not count for records.");
        chks("label is the first reason", r.label(), "STATE LOADED");
    }
    {
        Rule r;
        r.mark(kLoad);
        r.mark(kRewind);
        r.mark(kTrainer);
        chks("sentence, three reasons", r.sentence(),
             "ASSISTED: this game loaded a save state, rewound the game and used the trainer, so it does not count for records.");
    }
    {
        Rule r;
        for (int i = 0; i < kReasonCount; ++i) r.mark((Reason)i);
        chk("all reasons at once", r.reasons(), kReasonCount);
        r.new_game();
        chk("one new game clears all", r.counts(), 1);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
