// X25: local achievements. A table of achievements (key, name, description) with a condition on the game events (events.h), an unlock file (achievements.cfg next to controls.cfg)
// and a pop-up queue. Nothing is sent anywhere. It only READS game events, so the emulation and the replay hashes cannot change; Purist mode does not run it.
// Rule (shown on the list screen): an achievement can unlock only in a COUNTED game (assist rule, assist.h), for player 1, with 3 lives per game (the lives at GAME START).
// The conditions follow the idea of the RetroAchievements set for Pac-Man (fruit, ghost chains, cleared rounds); names and wording are our own.
// Later steps add achievements by adding a line to the table and a case to Engine::check().
#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "assist.h"
#include "events.h"

namespace achievements {

enum Id {
    kFirstPellet,
    kFirstFruit,
    kCherry,
    kStrawberry,
    kOrange,
    kApple,
    kMelon,
    kGalaxian,
    kBell,
    kKey,
    kFiveFruits, // X26: one per fruit kind (the kind is fixed by the round), and five different kinds in one game
    kChain1,
    kChain2,
    kChain3,
    kChain4,
    kChainFruit, // X27: ghost chains (all four ghosts on one pellet), counted per round
    kRound2,
    kRound5,
    kRound9,
    kRound13,
    kRound17,
    kPeaceful, // X28: cleared rounds, and three clean rounds in a row
    kCount
};
struct Def {
    const char *key;
    const char *name;
    const char *text;
}; // key: stable word in the file; name: up to 14 capital letters; text: one sentence of capital letters
inline const Def &def(int i) {
    static const Def t[kCount] = {
        {"first_pellet", "POWER UP", "EAT A POWER PELLET."},
        {"first_fruit", "FRUIT SALAD", "EAT A BONUS FRUIT."},
        {"fruit_cherry", "CHERRY PICKER", "EAT A CHERRY IN ROUND 1."},
        {"fruit_strawberry", "BERRY GOOD", "EAT A STRAWBERRY IN ROUND 2."},
        {"fruit_orange", "ZESTY", "EAT AN ORANGE IN ROUND 3 OR 4."},
        {"fruit_apple", "APPLE A DAY", "EAT AN APPLE IN ROUND 5 OR 6."},
        {"fruit_melon", "SWEET MELON", "EAT A MELON IN ROUND 7 OR 8."},
        {"fruit_galaxian", "SPACE SNACK", "EAT A GALAXIAN IN ROUND 9 OR 10."},
        {"fruit_bell", "BELL RINGER", "EAT A BELL IN ROUND 11 OR 12."},
        {"fruit_key", "KEY MASTER", "EAT A KEY IN ROUND 13 OR LATER."},
        {"fruit_five", "FRUIT BASKET", "EAT FIVE DIFFERENT FRUITS IN ONE GAME."},
        {"chain_1", "GHOST BUSTER", "EAT ALL FOUR GHOSTS ON ONE POWER PELLET."},
        {"chain_2", "DOUBLE CHAIN", "DO IT TWICE IN ONE ROUND."},
        {"chain_3", "TRIPLE CHAIN", "DO IT THREE TIMES IN ONE ROUND."},
        {"chain_4", "CHAIN REACTION", "DO IT ON ALL FOUR PELLETS OF A ROUND."},
        {"chain_fruit", "GHOST FEAST", "FOUR CHAINS AND TWO FRUITS IN ONE ROUND."},
        {"round_2", "WARMING UP", "CLEAR ROUND 2."},
        {"round_5", "HIGH FIVE", "CLEAR ROUND 5."},
        {"round_9", "NINE LIVES", "CLEAR ROUND 9."},
        {"round_13", "LUCKY THIRTEEN", "CLEAR ROUND 13."},
        {"round_17", "SEVENTEEN", "CLEAR ROUND 17."},
        {"peaceful", "PEACEFUL RUN", "CLEAR THREE ROUNDS IN A ROW WITHOUT EATING A GHOST OR DYING."},
    };
    return t[i >= 0 && i < kCount ? i : 0];
}
// X29: the rule changed at the user's request: a loaded save state does NOT stop achievements (it still stops records and statistics); rewind and the other helps (assist.h) still stop them.
inline const char *kRule1 = "PLAYER 1, 3 LIVES,", *kRule2 = "NO REWIND, STATES OK";
// What the assist rule (assist.h) means for achievements: a game counts if nothing but a loaded state helped.
inline bool counts_for_achievements(uint32_t assist_mask) { return (assist_mask & ~(1u << assist::kLoad)) == 0; }

class Engine {
public:
    // One game event. counted = the assist rule says the game still counts at this moment. Returns the first id unlocked by it (lowest number), or -1; all of them are in unlocked_now().
    int on_event(const events::Event &e, bool counted) {
        if (e.type == events::kGameStart) {
            eligible_ = e.value == 3 && e.player == 1;
            run_pellets_ = run_fruit_ = 0;
            run_kinds_ = 0;
            round_chains_ = round_fruit_ = 0;
            level_ = 1;
            round_ghost_ = round_death_ = false;
            streak_ = 0;
        }
        if (!counted) eligible_ = false; // a helped game never unlocks anything (until the next game starts)
        if (!eligible_ || e.player != 1) return -1;
        if (e.type == events::kLevel) {
            round_chains_ = round_fruit_ = 0;
            level_ = e.value;
            round_ghost_ = round_death_ = false;
        } // X27/X28: the round counters start again with every round
        if (e.type == events::kGhostEaten) round_ghost_ = true;
        if (e.type == events::kDeath) round_death_ = true;
        if (e.type == events::kRoundCleared) streak_ = (!round_ghost_ && !round_death_) ? streak_ + 1 : 0; // X28: a round without a ghost eaten and without a death
        if (e.type == events::kGhostEaten && e.value == 4) ++round_chains_;
        if (e.type == events::kFruitEaten) ++round_fruit_;
        if (e.type == events::kEnergizer) ++run_pellets_;
        if (e.type == events::kFruitEaten) {
            ++run_fruit_;
            if (e.value >= 0 && e.value < 8) run_kinds_ |= 1u << e.value;
        }
        last_.clear();
        for (int i = 0; i < kCount; ++i) { // one event can unlock several
            if (unlocked_[i] || !check(i, e)) continue;
            unlocked_[i] = true;
            stamp_[i] = (long long)std::time(nullptr);
            dirty_ = true;
            last_.push_back(i);
        }
        return last_.empty() ? -1 : last_[0];
    }
    // Feed the events a reader has logged since the last call (the reader's log only grows). counted as above. Returns the number unlocked now; they are in newly().
    int feed(const std::vector<events::Event> &log, bool counted) {
        newly_.clear();
        if (cursor_ > log.size()) cursor_ = 0;
        for (; cursor_ < log.size(); ++cursor_) {
            on_event(log[cursor_], counted);
            for (int u : last_) newly_.push_back(u);
        }
        return (int)newly_.size();
    }
    const std::vector<int> &newly() const { return newly_; }
    const std::vector<int> &unlocked_now() const { return last_; } // everything the last on_event() unlocked
    // X29: a save state was loaded in the middle of a game. The game goes on counting (eligible = player 1 with 3 lives, in a running maze); what the engine counted per game and per round before
    // (fruit kinds, chains, the clean-round streak) belongs to a timeline that no longer exists, so it starts again from the loaded round (level = the round's number).
    void resume_after_load(bool eligible, int level) {
        eligible_ = eligible;
        level_ = level < 1 ? 1 : level;
        run_pellets_ = run_fruit_ = 0;
        run_kinds_ = 0;
        round_chains_ = round_fruit_ = 0;
        round_ghost_ = round_death_ = false;
        streak_ = 0;
    }
    // X29: progress of an achievement that has a count, in the game that is running: have of need. need = 0: this achievement has no count.
    void progress(int i, int *have, int *need) const {
        int h = 0, n = 0;
        switch (i) {
        case kFiveFruits: {
            for (int k = 0; k < 8; ++k) h += (run_kinds_ >> k) & 1u;
            n = 5;
            break;
        }
        case kChain2:
            h = round_chains_;
            n = 2;
            break;
        case kChain3:
            h = round_chains_;
            n = 3;
            break;
        case kChain4:
            h = round_chains_;
            n = 4;
            break;
        case kChainFruit:
            h = (round_chains_ > 4 ? 4 : round_chains_) + (round_fruit_ > 2 ? 2 : round_fruit_);
            n = 6;
            break;
        case kPeaceful:
            h = streak_;
            n = 3;
            break;
        case kRound2:
        case kRound5:
        case kRound9:
        case kRound13:
        case kRound17: {
            static const int lv[5] = {2, 5, 9, 13, 17};
            n = lv[i - kRound2];
            h = level_ - 1;
            break;
        }
        default: break;
        }
        if (!eligible_) h = 0;
        if (h > n) h = n;
        if (have) { *have = h; }
        if (need) { *need = n; }
    }
    void invalidate_game() { eligible_ = false; } // a state was loaded etc.: nothing more until a new game starts
    bool unlocked(int i) const { return i >= 0 && i < kCount && unlocked_[i]; }
    int count() const {
        int n = 0;
        for (int i = 0; i < kCount; ++i) n += unlocked_[i] ? 1 : 0;
        return n;
    }
    unsigned long long mask() const {
        unsigned long long m = 0;
        for (int i = 0; i < kCount; ++i)
            if (unlocked_[i]) m |= 1ULL << i;
        return m;
    }
    bool eligible() const { return eligible_; }
    long long when(int i) const { return stamp_[i]; }

    // File: one line per unlocked achievement:  unlocked KEY UNIX-TIME. Unknown keys are kept and written back (a newer version's achievements are not lost). Bad lines are ignored.
    // A file that exists but has no valid line at all is moved to NAME.bad and `corrupt` is set (the game goes on with nothing unlocked).
    bool load(const std::string &path, bool *corrupt = nullptr) {
        if (corrupt) *corrupt = false;
        FILE *f = std::fopen(path.c_str(), "r");
        if (!f) return false;
        char line[300];
        int valid = 0, any = 0;
        while (std::fgets(line, sizeof line, f)) {
            if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
            ++any;
            char key[100] = {0};
            long long t = 0;
            if (std::sscanf(line, "unlocked %99s %lld", key, &t) >= 1 && key[0]) {
                ++valid;
                bool known = false;
                for (int i = 0; i < kCount; ++i)
                    if (!std::strcmp(key, def(i).key)) {
                        unlocked_[i] = true;
                        stamp_[i] = t;
                        known = true;
                    }
                if (!known) unknown_.push_back(std::string(key) + " " + std::to_string(t));
            }
        }
        std::fclose(f);
        if (any > 0 && valid == 0) {
            std::rename(path.c_str(), (path + ".bad").c_str());
            if (corrupt) *corrupt = true;
        }
        return true;
    }
    bool save(const std::string &path) {
        const std::string tmp = path + ".tmp";
        FILE *f = std::fopen(tmp.c_str(), "w");
        if (!f) return false;
        std::fprintf(f, "# Pac-Man port achievements (unlocked KEY UNIX-TIME); delete a line to lock it again\n");
        for (int i = 0; i < kCount; ++i)
            if (unlocked_[i]) std::fprintf(f, "unlocked %s %lld\n", def(i).key, stamp_[i]);
        for (const std::string &u : unknown_) std::fprintf(f, "unlocked %s\n", u.c_str());
        if (std::fclose(f) != 0) return false;
        std::remove(path.c_str());
        if (std::rename(tmp.c_str(), path.c_str()) != 0) return false;
        dirty_ = false;
        return true;
    }
    bool dirty() const { return dirty_; }

private:
    bool check(int i, const events::Event &e) const {
        switch (i) {
        case kFirstPellet: return e.type == events::kEnergizer;
        case kFirstFruit: return e.type == events::kFruitEaten;
        case kCherry:
        case kStrawberry:
        case kOrange:
        case kApple:
        case kMelon:
        case kGalaxian:
        case kBell:
        case kKey: return e.type == events::kFruitEaten && e.value == i - kCherry; // fruit kind 0 cherry ... 7 key (events.h)
        case kFiveFruits: {
            int n = 0;
            for (int k = 0; k < 8; ++k) n += (run_kinds_ >> k) & 1u;
            return e.type == events::kFruitEaten && n >= 5;
        }
        case kChain1: return e.type == events::kGhostEaten && e.value == 4;
        case kChain2: return e.type == events::kGhostEaten && e.value == 4 && round_chains_ >= 2;
        case kChain3: return e.type == events::kGhostEaten && e.value == 4 && round_chains_ >= 3;
        case kChain4: return e.type == events::kGhostEaten && e.value == 4 && round_chains_ >= 4;
        case kChainFruit: return (e.type == events::kGhostEaten || e.type == events::kFruitEaten) && round_chains_ >= 4 && round_fruit_ >= 2;
        case kRound2:
        case kRound5:
        case kRound9:
        case kRound13:
        case kRound17: {
            static const int lv[5] = {2, 5, 9, 13, 17};
            return e.type == events::kRoundCleared && level_ == lv[i - kRound2];
        }
        case kPeaceful: return e.type == events::kRoundCleared && streak_ >= 3;
        default: return false;
        }
    }
    int level_ = 1, streak_ = 0;
    bool round_ghost_ = false, round_death_ = false;
    int round_chains_ = 0, round_fruit_ = 0;
    bool unlocked_[kCount] = {false};
    long long stamp_[kCount] = {0};
    std::vector<std::string> unknown_;
    std::vector<int> newly_, last_;
    bool eligible_ = false, dirty_ = false;
    size_t cursor_ = 0;
    int run_pellets_ = 0, run_fruit_ = 0;
    unsigned run_kinds_ = 0;
};

// The pop-up: a queue of unlocked achievements shown one after the other, 3 seconds each, only while `allowed` (the maze is running: not the READY screen, not paused, no menu).
// The time only runs while it is allowed (now_ms is any millisecond clock).
class Popup {
public:
    void push(int id) { queue_.push_back(id); }
    // Returns the achievement to draw now, or -1.
    int update(bool allowed, unsigned now_ms) {
        const unsigned dt = have_last_ ? now_ms - last_ : 0;
        last_ = now_ms;
        have_last_ = true;
        if (cur_ < 0 && !queue_.empty() && allowed) {
            cur_ = queue_.front();
            queue_.erase(queue_.begin());
            shown_ms_ = 0;
        }
        if (cur_ >= 0 && allowed) {
            shown_ms_ += dt > 200 ? 200 : dt;
            if (shown_ms_ >= kShowMs) { cur_ = -1; }
        }
        return allowed ? cur_ : -1;
    }
    void clear() {
        queue_.clear();
        cur_ = -1;
        shown_ms_ = 0;
    }
    size_t waiting() const { return queue_.size(); }
    static constexpr unsigned kShowMs = 3000;

private:
    std::vector<int> queue_;
    int cur_ = -1;
    unsigned shown_ms_ = 0, last_ = 0;
    bool have_last_ = false;
};

} // namespace achievements
