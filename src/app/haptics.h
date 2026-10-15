// S34R: pad vibration (rumble). Nothing here can touch the machine: the engine only looks at a few RAM values after each frame
// (the same values the debug overlay shows, docs/notes/ram_map.md) and decides how hard the pad's motors should turn.
//
// What shakes the pad (Full; Light leaves out the dots and the heartbeat and is a bit softer):
//   COIN        a short light tick                       DOT          a tiny tick for every dot (a purr while eating)
//   ENERGIZER   a firm thump (both motors and triggers)  GHOST        a hard thump, stronger for each ghost in the chain (200, 400, 800, 1600)
//   FRUIT       two quick taps                           EXTRA LIFE   three rising taps
//   DEATH       a long rumble that fades with a crumbling stutter (the death animation)
//   LEVEL DONE  two celebration pulses                   FRIGHT END   a soft tick when the ghosts stop being blue
//   NEAR        a heartbeat (lub-dub) that gets stronger when a ghost that can hurt you is close
// Purist mode has no vibration (the arcade had none).
//
// Per pad type (SDL tells which one is connected): Xbox (also a DualShock seen through DS4Windows, Windows then reports an Xbox pad), PS4, PS5 or other.
// The type only changes the strength of the two motors and whether the trigger motors (Xbox One pads) are used; the pattern is the same.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace haptics {

enum Fx { kCoin, kDot, kEnergizer, kGhost, kFruit, kExtraLife, kDeath, kLevelDone, kFrightEnd, kNear, kFxCount };
inline const char *name(int fx) {
    static const char *n[] = {"COIN", "DOT", "ENERGIZER", "GHOST EATEN", "FRUIT", "EXTRA LIFE", "DEATH", "LEVEL DONE", "FRIGHT END", "GHOST NEAR"};
    return fx >= 0 && fx < kFxCount ? n[fx] : "?";
}

// The RAM values the detector needs (read after every frame).
struct Snap {
    int main_state = 0, level_state = 0, level = 0, lives = 0, credits = 0, death_anim = 0, powered = 0;
    int score = 0; // player 1 + player 2 score in points (the ROM keeps it as BCD digits: a dot is 10, an energizer 50)
    int pac_y = 0, pac_x = 0;
    int ghost_y[4] = {0, 0, 0, 0}, ghost_x[4] = {0, 0, 0, 0}, edible[4] = {0, 0, 0, 0};
};

enum class Kind { Other, Xbox, PS4, PS5 };
struct Profile {
    float low, high, trigger;
}; // motor strength factors (starting values; the real pads are tuned by the user's test)
inline Profile profile_of(Kind k) {
    switch (k) {
    case Kind::Xbox: return {1.00f, 1.00f, 0.85f};
    case Kind::PS4: return {0.90f, 1.20f, 0.0f}; // the DualShock's small motor is weak, so it gets a boost
    case Kind::PS5: return {0.80f, 0.90f, 0.0f}; // the DualSense motors are strong
    default: return {1.00f, 1.00f, 0.0f};
    }
}
// SDL_GameControllerType values: 1 XBOX360, 2 XBOXONE, 3 PS3, 4 PS4, 5 NINTENDO_SWITCH_PRO, 6 VIRTUAL, 7 PS5 (SDL 2.0.14+).
inline Kind kind_of_sdl_type(int t) { return t == 1 || t == 2 ? Kind::Xbox : t == 3 || t == 4 ? Kind::PS4 : t == 7 ? Kind::PS5 : Kind::Other; }
inline const char *kind_name(Kind k) { return k == Kind::Xbox ? "Xbox" : k == Kind::PS4 ? "PlayStation 4" : k == Kind::PS5 ? "PlayStation 5" : "other"; }

struct Out {
    float low = 0, high = 0, trig_l = 0, trig_r = 0;
}; // 0..1; low = the big (left) motor, high = the small (right) motor
inline bool operator==(const Out &a, const Out &b) { return a.low == b.low && a.high == b.high && a.trig_l == b.trig_l && a.trig_r == b.trig_r; }
inline bool operator!=(const Out &a, const Out &b) { return !(a == b); }

class Engine {
public:
    // 0 = off, 1 = light, 2 = full
    void set_level(int v) {
        level_ = v < 0 ? 0 : v > 2 ? 2 : v;
        if (!level_) fx_.clear();
    }
    int level() const { return level_; }
    void invalidate(bool power_on = false) {
        if (power_on) frames_ = 0;
        have_prev_ = false;
        fx_.clear();
        near_phase_ = -1;
    } // after a load, rewind, restart or pause: do not compare with an old frame
    void silence() { fx_.clear(); }
    void preview() { start(kEnergizer, 1.0f); } // a taste of the new strength when it is changed in the menu

    static int dist(const Snap &s, int i) { return std::max(std::abs(s.ghost_y[i] - s.pac_y), std::abs(s.ghost_x[i] - s.pac_x)); }

    // Feed one finished frame; returns a bit mask of the effects that started in it (bit = 1 << Fx).
    unsigned feed(const Snap &s) {
        unsigned ev = 0;
        ++frames_;
        if (have_prev_ &&
            frames_ > 320) { // the board's power-on self-test (about 300 frames, boot_flow.md) fills memory with test patterns: nothing is judged before it ends
            const bool playing = s.main_state == 3 && prev_.main_state == 3;
            if (s.credits > prev_.credits) ev |= start(kCoin, 1.0f);
            if (playing) {
                const int d = s.score - prev_.score;
                if (d == 10) ev |= start(kDot, 1.0f);
                else if (d == 50) ev |= start(kEnergizer, 1.0f);
                else if (d == 200 || d == 400 || d == 800 || d == 1600) ev |= start(kGhost, d == 200 ? 0.6f : d == 400 ? 0.75f : d == 800 ? 0.9f : 1.0f);
                else if (d == 100 || d == 300 || d == 500 || d == 700 || d == 1000 || d == 2000 || d == 3000 || d == 5000) ev |= start(kFruit, 1.0f);
                if (s.lives == prev_.lives + 1 && d > 0) ev |= start(kExtraLife, 1.0f);
                if (s.death_anim != 0 && prev_.death_anim == 0) ev |= start(kDeath, 1.0f);
                if (s.level == prev_.level + 1) ev |= start(kLevelDone, 1.0f);
                if (prev_.powered && !s.powered && !s.death_anim) ev |= start(kFrightEnd, 1.0f);
                // the heartbeat: a ghost that is not blue within 28 picture pixels of Pac-Man while the maze runs
                float near = 0;
                if (s.level_state == 3 && !s.death_anim)
                    for (int i = 0; i < 4; ++i)
                        if (!s.edible[i]) {
                            int dd = dist(s, i);
                            if (dd < 28) near = std::max(near, (28 - dd) / 28.0f);
                        }
                if (near > 0) {
                    if (near_phase_ < 0) near_phase_ = 0;
                    if (near_phase_ % 24 == 0) ev |= start(kNear, 0.35f + 0.65f * near);
                    else if (near_phase_ % 24 == 10) start(kNear + 100, 0.25f + 0.5f * near); // the "dub" (not reported as a separate event)
                    ++near_phase_;
                } else near_phase_ = -1;
            } else near_phase_ = -1;
        }
        prev_ = s;
        have_prev_ = true;
        return ev;
    }

    // The motors for the frame just fed. Call advance() once per frame after feed().
    Out current(const Profile &p, bool trigger_motors) const {
        Out o;
        for (const E &e : fx_) {
            float l = 0, h = 0, t = 0;
            shape(e, &l, &h, &t);
            o.low = std::max(o.low, l);
            o.high = std::max(o.high, h);
            o.trig_l = std::max(o.trig_l, t);
        }
        const float g = level_ == 1 ? 0.6f : 1.0f;
        o.low = clamp(o.low * p.low * g);
        o.high = clamp(o.high * p.high * g);
        o.trig_l = trigger_motors ? clamp(o.trig_l * p.trigger * g) : 0;
        o.trig_r = o.trig_l;
        return o;
    }
    void advance() {
        for (E &e : fx_) ++e.age;
        fx_.erase(std::remove_if(fx_.begin(), fx_.end(), [](const E &e) { return e.age >= length(e.fx); }), fx_.end());
    }
    size_t active() const { return fx_.size(); }

private:
    struct E {
        int fx;
        int age;
        float power;
    };
    static float clamp(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
    static int length(int fx) {
        switch (fx) {
        case kCoin: return 4;
        case kDot: return 3;
        case kEnergizer: return 22;
        case kGhost: return 16;
        case kFruit: return 12;
        case kExtraLife: return 30;
        case kDeath: return 110;
        case kLevelDone: return 40;
        case kFrightEnd: return 5;
        case kNear: return 8;
        case kNear + 100: return 6;
        default: return 1;
        }
    }
    unsigned start(int fx, float power) {
        if (level_ == 0) return 0;
        if (level_ == 1 && (fx == kDot || fx == kNear || fx == kNear + 100)) return 0; // Light: no dot purr, no heartbeat
        fx_.push_back({fx, 0, power});
        return fx < kFxCount ? (1u << fx) : 0;
    }
    static void shape(const E &e, float *l, float *h, float *t) {
        const float a = (float)e.age, n = (float)length(e.fx), k = 1.0f - a / n, p = e.power;
        switch (e.fx) {
        case kCoin: *h = 0.45f; break;
        case kDot: *h = 0.30f; break;
        case kEnergizer:
            *l = 0.65f * k;
            *h = 0.55f * k;
            *t = 0.7f * k;
            break;
        case kGhost:
            *l = p * k;
            *h = p * (1.0f - a / 11.0f > 0 ? 1.0f - a / 11.0f : 0);
            *t = p * k;
            break;
        case kFruit: {
            int ph = e.age % 6;
            if (ph < 3) {
                *h = 0.7f;
                *l = 0.3f;
            }
            break;
        }
        case kExtraLife: {
            int ph = e.age % 10, tap = e.age / 10;
            if (ph < 4) {
                float v = 0.4f + 0.2f * tap;
                *l = v;
                *h = v;
                *t = v;
            }
            break;
        }
        case kDeath: {
            float kk = std::pow(k, 1.3f);
            *l = kk;
            *h = ((e.age / 5) % 2 == 0) ? 0.6f * kk : 0.0f;
            *t = 0.5f * kk;
            break;
        }
        case kLevelDone: {
            int ph = e.age % 20;
            if (ph < 8) {
                *l = 0.5f;
                *h = 0.6f;
                *t = 0.5f;
            }
            break;
        }
        case kFrightEnd: *h = 0.35f; break;
        case kNear: *l = 0.55f * p * k; break;
        default: *l = 0.35f * p * k; break;
        }
    }
    int level_ = 2;
    int frames_ = 0;
    std::vector<E> fx_;
    Snap prev_;
    bool have_prev_ = false;
    int near_phase_ = -1;
};

inline uint16_t to_u16(float v) { return (uint16_t)(v <= 0 ? 0 : v >= 1 ? 65535 : v * 65535.0f + 0.5f); }

} // namespace haptics
