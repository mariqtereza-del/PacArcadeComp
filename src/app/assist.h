// X03: the one shared "assisted game" rule. A game COUNTS (for the saved high score, and later for achievements, statistics, personal bests, replays and
// challenges) only while nothing in it was helped. Every feature that helps marks the game with a named reason from the table below; everything that records
// something asks counts(). A new game (the machine's state goes to "running" from something else) clears all reasons. Purist mode has none of these features.
// Later steps add their reason by adding a line to the table (and a call to mark()).
#pragma once
#include <cstdint>
#include <string>

namespace assist {

enum Reason { kLoad, kRewind, kStartLevel, kTrainer, kSlowMotion, kFrameAdvance, kCheatHelp, kTestSetup, kModded, kReasonCount };

struct Info {
    const char *label;
    const char *phrase;
}; // label: up to 12 capital letters (the menu); phrase: for the one-sentence explanation
inline const Info &info(int r) {
    static const Info t[kReasonCount] = {{"STATE LOADED", "loaded a save state"},  {"REWIND", "rewound the game"},      {"START LEVEL", "started at a later level"},
                                         {"TRAINER", "used the trainer"},          {"SLOW MOTION", "used slow motion"}, {"FRAME STEP", "stepped frame by frame"},
                                         {"CHEAT HELP", "used a cheat-like help"}, {"TEST SETUP", "used a test set-up"},
                                         {"MODDED", "played with modified rules"}};
    return t[r >= 0 && r < kReasonCount ? r : 0];
}

class Rule {
public:
    void mark(Reason r) { mask_ |= 1u << r; }
    void new_game() { mask_ = 0; }             // a new game counts again
    bool counts() const { return mask_ == 0; } // "counts: yes / no"
    bool has(Reason r) const { return (mask_ >> r) & 1u; }
    uint32_t mask() const { return mask_; }
    int reasons() const {
        int n = 0;
        for (int r = 0; r < kReasonCount; ++r) n += (int)has((Reason)r);
        return n;
    }
    // The label of the first reason (in table order), for the menu and the window title; "" when the game counts.
    std::string label() const {
        for (int r = 0; r < kReasonCount; ++r)
            if (has((Reason)r)) return info(r).label;
        return "";
    }
    // The one sentence shown to the player; "" when the game counts.
    std::string sentence() const {
        if (counts()) return "";
        std::string s = "ASSISTED: this game ";
        int seen = 0, n = reasons();
        for (int r = 0; r < kReasonCount; ++r)
            if (has((Reason)r)) {
                s += (seen == 0 ? "" : seen == n - 1 ? " and " : ", ");
                s += info(r).phrase;
                ++seen;
            }
        return s + ", so it does not count for records.";
    }

private:
    uint32_t mask_ = 0;
};

} // namespace assist
