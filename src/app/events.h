// X01: the game event reader. It looks at a few RAM values after every machine frame (docs/notes/ram_map.md) and turns what changed into typed
// events: game start, game over, death, dot, energizer, ghost eaten (place 1-4 in the chain), fruit appeared, fruit eaten (which fruit),
// round cleared, extra life, level number, player 1/2 and lives. Achievements, statistics, challenges and the lightbar will all use it.
// It only READS memory (it never writes to the machine), so the emulation and the replay hashes cannot change. The vibration engine (haptics.h) is separate.
//
// How each event is recognised (all values are RAM; "prev" = the frame before):
//   GAME START    4E00 (main state) becomes 3 (running) from something else        GAME OVER     4E04 (level state) becomes 7
//   DEATH         4DA5 (death animation) goes from 0 to non-zero                   LIVES         4E14 changes while running (value = lives)
//   DOT / ENERGIZER  the dot counter 4E0E goes up by 1; the score step tells them apart (10 = dot, 50 = energizer)
//   GHOST EATEN   score step 200/400/800/1600 = place 1/2/3/4                       FRUIT APPEARED  fruit position word 4DD2/4DD3 goes from 0 to non-zero (kind = 4DD4 - 6)
//   FRUIT EATEN   the fruit position goes back to 0 AND the score step is that fruit's value (100 cherry ... 5000 key); a fruit that times out has no score step
//   ROUND CLEARED 4E0E becomes 244 (all 240 dots + 4 energizers)                    LEVEL         4E13 changes while running (value = level number, 1-based)
//   EXTRA LIFE    lives +1 together with a score step                               PLAYER        4E09 changes while running (value = 1 or 2)
// Nothing is judged during the board's power-on self-test (first 320 frames, it fills memory with test patterns) or on the first frame after invalidate().
#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace events {

enum Type { kGameStart, kGameOver, kDeath, kDot, kEnergizer, kGhostEaten, kFruitAppeared, kFruitEaten, kRoundCleared, kExtraLife, kLevel, kPlayer, kLives, kTypeCount };
inline const char *name(int t) {
    static const char *n[] = {"GAME START",  "GAME OVER",     "DEATH",      "DOT",   "ENERGIZER", "GHOST EATEN", "FRUIT APPEARED",
                              "FRUIT EATEN", "ROUND CLEARED", "EXTRA LIFE", "LEVEL", "PLAYER",    "LIVES"};
    return t >= 0 && t < kTypeCount ? n[t] : "?";
}
inline const char *fruit_name(int k) {
    static const char *n[] = {"CHERRY", "STRAWBERRY", "ORANGE", "APPLE", "MELON", "GALAXIAN", "BELL", "KEY"};
    return k >= 0 && k < 8 ? n[k] : "?";
}

// Everything the reader needs from RAM, for one frame.
struct Raw {
    int main_state = 0, level_state = 0, player = 0, dots = 0, level = 0, lives = 0, death_anim = 0;
    int fruit_pos = 0, fruit_idx = 0; // 4DD2 + 256 * 4DD3; 4DD4 (index into the points table, 6 = cherry ... 13 = key)
    int score = 0;                    // the score of both players added, in points (BCD in RAM)
};

struct Event {
    unsigned frame = 0; // machine frame number (1 = the first frame run)
    int player = 1;     // 1 or 2
    int type = kDot;
    int value = 0; // GHOST EATEN: place 1-4; FRUIT: kind 0-7; LEVEL: number; LIVES / EXTRA LIFE: lives; PLAYER: 1 or 2; GAME START: lives
};
inline std::string line(const Event &e) {
    std::string s = std::to_string(e.frame) + " P" + std::to_string(e.player) + " " + name(e.type);
    if (e.type == kGhostEaten) s += " place " + std::to_string(e.value);
    else if (e.type == kFruitAppeared || e.type == kFruitEaten) s += std::string(" ") + fruit_name(e.value);
    else if (e.type == kLevel || e.type == kLives || e.type == kExtraLife || e.type == kPlayer || e.type == kGameStart) s += " " + std::to_string(e.value);
    return s;
}

// Reads the RAM through any function addr -> byte (the machine's bus in the program, a made-up array in the unit test).
template <class Read> Raw read_raw(Read r) {
    Raw s;
    s.main_state = r(0x4E00);
    s.level_state = r(0x4E04);
    s.player = r(0x4E09);
    s.dots = r(0x4E0E);
    s.level = r(0x4E13);
    s.lives = r(0x4E14);
    s.death_anim = r(0x4DA5);
    s.fruit_pos = r(0x4DD2) | (r(0x4DD3) << 8);
    s.fruit_idx = r(0x4DD4);
    for (int p = 0; p < 2; ++p)
        for (int i = 2; i >= 0; --i) {
            int b = r(0x4E80 + 4 * p + i);
            s.score += ((b >> 4) * 10 + (b & 15)) * (i == 2 ? 10000 : i == 1 ? 100 : 1);
        }
    return s;
}

class Reader {
public:
    // After a load, rewind, restart or pause: do not compare with an old frame. power_on = the machine was restarted (the self-test is skipped again).
    void invalidate(bool power_on = false) {
        if (power_on) frames_ = 0;
        have_prev_ = false;
    }
    const std::vector<Event> &log() const { return log_; }
    unsigned count(int type) const { return counts_[type]; }

    // Feed one finished machine frame. `frame` is the machine frame number. Returns how many events were added to the log.
    size_t feed(const Raw &s, unsigned frame) {
        const size_t before = log_.size();
        ++frames_;
        if (have_prev_ && frames_ > 320) {
            const Raw &p = prev_;
            const bool playing = s.main_state == 3 && p.main_state == 3;
            const int pl = s.player + 1, d = s.score - p.score;
            if (s.main_state == 3 && p.main_state != 3) add(frame, pl, kGameStart, s.lives);
            if (playing) {
                if (s.player != p.player) {
                    add(frame, pl, kPlayer, pl);
                } else {
                    if (s.level_state == 7 && p.level_state != 7) add(frame, pl, kGameOver, 0);
                    if (s.death_anim != 0 && p.death_anim == 0) add(frame, pl, kDeath, 0);
                    if (s.dots == p.dots + 1 && s.level_state == 3) add(frame, pl, d == 50 ? kEnergizer : kDot, 0);
                    if (d == 200 || d == 400 || d == 800 || d == 1600) add(frame, pl, kGhostEaten, d == 200 ? 1 : d == 400 ? 2 : d == 800 ? 3 : 4);
                    if (p.fruit_pos == 0 && s.fruit_pos != 0) add(frame, pl, kFruitAppeared, kind(s.fruit_idx));
                    if (p.fruit_pos != 0 && s.fruit_pos == 0 && d > 0 && d == fruit_points(p.fruit_idx)) add(frame, pl, kFruitEaten, kind(p.fruit_idx));
                    if (s.dots == 244 && p.dots != 244) add(frame, pl, kRoundCleared, 0);
                    if (s.level != p.level) add(frame, pl, kLevel, s.level + 1);
                    if (s.lives != p.lives) {
                        if (s.lives == p.lives + 1 && d > 0 && p.lives > 0) add(frame, pl, kExtraLife, s.lives);
                        add(frame, pl, kLives, s.lives);
                    }
                }
            }
        }
        prev_ = s;
        have_prev_ = true;
        return log_.size() - before;
    }
    // The "--events FILE" text: one line per event (see line()).
    bool write(const std::string &path) const {
        FILE *f = std::fopen(path.c_str(), "w");
        if (!f) return false;
        for (const Event &e : log_) std::fprintf(f, "%s\n", line(e).c_str());
        std::fclose(f);
        return true;
    }

private:
    static int kind(int idx) { return idx >= 6 && idx <= 13 ? idx - 6 : 0; }
    static int fruit_points(int idx) {
        static const int v[] = {100, 300, 500, 700, 1000, 2000, 3000, 5000};
        return idx >= 6 && idx <= 13 ? v[idx - 6] : -1;
    }
    void add(unsigned frame, int player, int type, int value) {
        log_.push_back({frame, player, type, value});
        ++counts_[type];
    }
    std::vector<Event> log_;
    unsigned counts_[kTypeCount] = {0};
    Raw prev_;
    bool have_prev_ = false;
    int frames_ = 0;
};

} // namespace events
