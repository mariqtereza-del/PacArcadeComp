// X22: per-run statistics. A "run" is one player's game from GAME START to GAME OVER. The collector reads the game events (events.h, which only READ memory)
// and counts: score, level reached, dots, energizers, ghosts eaten, longest ghost chain (1-4), fruit eaten, deaths, extra lives and time played.
// It lives outside the machine (it never writes to it), so the emulation and the replay hashes cannot change. Purist mode does not run it.
// The assist rule (assist.h): a run counts only while the game counts. When the game is helped (state loaded, rewind ...) the run is frozen and marked
// "not counted"; the summary page then says so instead of showing numbers.
#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include "events.h"

namespace stats {

struct Run {
    bool active = false; // GAME START seen and GAME OVER not yet
    bool counted = true; // false once the game was helped; the numbers are then frozen
    int score = 0, level = 1, dots = 0, energizers = 0, ghosts = 0, chain = 0, fruit = 0, deaths = 0, extra_lives = 0;
    unsigned frames = 0;      // machine frames played (60.606 per second)
    unsigned level_start = 0; // X24: the value of frames when the current level began (for the time of a level)
    int seconds() const { return (int)((double)frames * 50688.0 / 3072000.0 + 0.5); }
};

// X24: a level was cleared (ROUND CLEARED): the player's score at that moment and the machine frames the level took. Only counted games make one.
struct Clear {
    int player = 1, level = 1, score = 0;
    unsigned frames = 0;
};

// What the summary page shows.
struct Summary {
    Run run;
    int player = 1;
    bool two_players = false;
};

inline std::string time_text(int seconds) { // "2M 35S" (the game's font has no colon)
    return std::to_string(seconds / 60) + "M " + (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60) + "S";
}

class Collector {
public:
    // The machine was loaded, rewound, restarted or paused: do not compare with an old frame. power_on = restarted (self-test skipped again; runs dropped).
    void invalidate(bool power_on = false) {
        rd_.invalidate(power_on);
        if (power_on) {
            run_[0] = run_[1] = Run();
            cur_ = 0;
            fresh_.clear();
            clears_.clear();
        }
    }
    // Feed one finished machine frame. counts = the assist rule says the game still counts. score1/score2 = the two players' scores in points.
    // Returns true when a GAME OVER happened on this frame (then take() holds the summary).
    bool feed(const events::Raw &raw, unsigned frame, bool counts, int score1, int score2) {
        if (!counts)
            for (Run &r : run_)
                if (r.active) r.counted = false; // before the events of this frame are applied
        const size_t had = rd_.log().size();
        rd_.feed(raw, frame);
        bool over = false;
        for (size_t i = had; i < rd_.log().size(); ++i) over |= apply(rd_.log()[i]);
        Run &r = run_[cur_];
        if (r.active && r.counted) {
            r.score = std::max(r.score, cur_ == 0 ? score1 : score2);
            if (raw.main_state == 3 && raw.level_state != 7) ++r.frames;
        }
        for (Clear &c : fresh_) {
            const Run &q = run_[c.player - 1];
            c.score = q.score;
            c.frames = q.frames - q.level_start;
            clears_.push_back(c);
        } // after this frame's score and time were added
        fresh_.clear();
        return over;
    }
    // X24: the levels cleared since the last call (oldest first); the list is emptied.
    std::vector<Clear> take_clears() {
        std::vector<Clear> v;
        v.swap(clears_);
        return v;
    }
    const Summary &take() const { return summary_; }
    const Run &run(int player0) const { return run_[player0 & 1]; }
    const std::vector<events::Event> &log() const { return rd_.log(); } // X25: the achievements read the same events
    size_t event_count() const { return rd_.log().size(); }
    unsigned count(int type) const { return rd_.count(type); }

private:
    bool apply(const events::Event &e) {
        const int p = e.player == 2 ? 1 : 0;
        if (e.type == events::kGameStart) { // a new game: both runs start afresh
            run_[0] = run_[1] = Run();
            run_[0].active = true;
            cur_ = 0;
            two_ = false;
            return false;
        }
        if (e.type == events::kPlayer) {
            cur_ = p;
            if (p == 1) {
                two_ = true;
                if (!run_[1].active && !run_[1].frames) run_[1].active = true;
            }
            return false;
        }
        cur_ = p;
        Run &r = run_[p];
        if (!r.active) return false;
        if (e.type == events::kGameOver) {
            summary_.run = r;
            summary_.player = p + 1;
            summary_.two_players = two_;
            r.active = false;
            return true;
        }
        if (!r.counted) return false;
        switch (e.type) {
        case events::kDot: ++r.dots; break;
        case events::kEnergizer: ++r.energizers; break;
        case events::kGhostEaten:
            ++r.ghosts;
            r.chain = std::max(r.chain, e.value);
            break;
        case events::kFruitEaten: ++r.fruit; break;
        case events::kDeath: ++r.deaths; break;
        case events::kExtraLife: ++r.extra_lives; break;
        case events::kLevel:
            r.level = std::max(r.level, e.value);
            r.level_start = r.frames;
            break;
        case events::kRoundCleared: {
            Clear c;
            c.player = p + 1;
            c.level = r.level;
            fresh_.push_back(c);
            break;
        }
        default: break;
        }
        return false;
    }
    events::Reader rd_;
    std::vector<Clear> fresh_, clears_;
    Run run_[2];
    int cur_ = 0;
    bool two_ = false;
    Summary summary_;
};

} // namespace stats
