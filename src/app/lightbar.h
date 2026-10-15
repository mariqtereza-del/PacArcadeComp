// X09 / X10: the pad's light bar (DualShock 4, DualSense). Pure decision logic, no SDL in here (so it can be unit tested); main.cpp passes the answer to SDL_GameControllerSetLED.
// The light bar lives completely outside the machine: it only READS a few RAM values (through Situation) and never touches the emulation.
//   Playing, safe: Pac-Man yellow.   A ghost that can hurt you close by: red, brighter the closer it is (X10; the same numbers as the vibration heartbeat in haptics.h:
//   a ghost that is not blue, within 28 picture pixels of Pac-Man (the larger of the two distances), while the maze runs and Pac-Man is alive; full strength at distance 0).
//   While the ghosts are frightened (4DA6 = 1): blue-white.   Colours between these fade over a few frames (kFadeStep per frame and channel).
//   X11: a red flash when Pac-Man dies, a green flash for an extra life, a white pulse when a round is cleared; dark during game over and the attract screens (no game running),
//   yellow again as soon as a new game starts.
//   Menu / paused / bindings screen / window-focus pause: dim blue.   Option off: the bar is put out.   Purist mode: the program does not touch the bar
//   (if it had set a colour before, the neutral blue is put back once).   On exit (or when the pad goes away) the neutral blue is put back.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cmath>

namespace lightbar {

struct Rgb {
    uint8_t r = 0, g = 0, b = 0;
    bool operator==(const Rgb &o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb &o) const { return !(*this == o); }
};

const Rgb kYellow = {255, 204, 0}, kDimBlue = {0, 0, 48}, kOut = {0, 0, 0}, kNeutral = {0, 32, 255},
          kFright = {140, 190, 255}; // play, menu, option off, "the pad's normal colour", ghosts frightened
// X11: the flashes. ONE table: colour, length in machine frames, number of pulses (each pulse starts bright and dies away).
enum class Flash { None, Death, ExtraLife, RoundCleared, kCount };
struct FlashDef {
    const char *name;
    Rgb colour;
    int length, pulses;
};
inline const FlashDef &flash_def(Flash f) {
    static const FlashDef t[] = {{"none", {0, 0, 0}, 1, 1}, {"death", {255, 0, 0}, 72, 2}, {"extra life", {0, 255, 0}, 60, 3}, {"round cleared", {255, 255, 255}, 96, 3}};
    return t[(int)f];
}
const int kNearPixels = 28, kFadeStep = 32, kSelfTestFrames = 320; // nothing is judged during the board's power-on self-test (it fills memory with test patterns)

enum class Scene { Play, Menu };
enum class Mood { None, Safe, Near, Frightened, Menu, Off, Neutral, Dark };
inline const char *mood_name(Mood m) {
    static const char *n[] = {"none", "safe", "near a ghost", "ghosts frightened", "menu", "option off", "neutral", "dark (no game)"};
    return n[(int)m];
}

// The RAM values the colour needs (docs/notes/ram_map.md: 4E00 main state, 4E04 level state, 4DA5 death animation, 4DA6 ghosts frightened, 4D08/4D09 Pac-Man y/x, 4D00.. ghosts, 4DA7.. edible flags).
struct Situation {
    int main_state = 0, level_state = 0, death_anim = 0, powered = 0, pac_y = 0, pac_x = 0;
    int ghost_y[4] = {0, 0, 0, 0}, ghost_x[4] = {0, 0, 0, 0}, edible[4] = {0, 0, 0, 0};
    int lives = 0, dots = 0, player = 0, score = 0; // X11: 4E14, 4E0E (244 = round cleared), 4E09, both scores added in points (BCD at 4E80..)
};
inline bool playing(const Situation &s) { return s.main_state == 3; }
inline bool game_over_or_attract(const Situation &s) { return s.main_state != 3 || s.level_state == 7; } // no game is being played
// 0 = no ghost that can hurt you close by, 1 = on top of Pac-Man
inline float nearness(const Situation &s) {
    if (!playing(s) || s.level_state != 3 || s.death_anim) return 0;
    float near = 0;
    for (int i = 0; i < 4; ++i)
        if (!s.edible[i]) {
            int d = std::max(std::abs(s.ghost_y[i] - s.pac_y), std::abs(s.ghost_x[i] - s.pac_x));
            if (d < kNearPixels) near = std::max(near, (kNearPixels - d) / (float)kNearPixels);
        }
    return near;
}
inline Mood mood_of(const Situation &s) {
    if (game_over_or_attract(s)) return Mood::Dark;
    if (!playing(s) || s.death_anim) return Mood::Safe;
    if (s.powered) return Mood::Frightened;
    return nearness(s) > 0 ? Mood::Near : Mood::Safe;
}
// The colour the bar is heading for, while playing.
inline Rgb colour_for(const Situation &s) {
    switch (mood_of(s)) {
    case Mood::Frightened: return kFright;
    case Mood::Near: {
        float b = 0.35f + 0.65f * nearness(s);
        return Rgb{(uint8_t)(255 * b + 0.5f), 0, 0};
    }
    case Mood::Dark: return kOut;
    default: return kYellow;
    }
}
inline Rgb step_towards(Rgb cur, Rgb want, int step = kFadeStep) {
    auto one = [&](uint8_t c, uint8_t w) -> uint8_t {
        int d = (int)w - (int)c;
        return (uint8_t)(d > step ? c + step : d < -step ? c - step : w);
    };
    return Rgb{one(cur.r, want.r), one(cur.g, want.g), one(cur.b, want.b)};
}

class Driver {
public:
    // Call once per machine frame while playing and once per screen frame otherwise. `sit` may be null (then: safe). Returns true when colour *out has to be sent to the pad now.
    bool update(bool has_led, bool enabled, bool purist, Scene scene, const Situation *sit, Rgb *out) {
        mood_changed_ = false;
        flash_started_ = Flash::None;
        if (!has_led) {
            sent_ = false;
            mood_ = Mood::None;
            have_prev_ = false;
            return false;
        } // Xbox pads, DS4Windows pads: nothing happens
        Mood mood;
        Rgb want;
        bool fade = false;
        if (purist) {
            if (!sent_) return false;
            mood = Mood::Neutral;
            want = kNeutral;
        } // put the neutral blue back once, then stay quiet
        else if (!enabled) {
            mood = Mood::Off;
            want = kOut;
        } else if (scene == Scene::Menu) {
            mood = Mood::Menu;
            want = kDimBlue;
            have_prev_ = false;
        } // a menu or a pause: no events are judged across it, the flash timers wait
        else {
            ++frames_;
            if (!sit) {
                have_prev_ = false;
                mood = Mood::Safe;
                want = kYellow;
            } // no RAM values given: simply the playing colour
            else {
                events(*sit);
                mood = frames_ <= kSelfTestFrames ? Mood::Dark : mood_of(*sit);
                want = mood == Mood::Dark ? kOut : colour_for(*sit);
            }
            fade = sent_ && last_play_;
        }
        if (mood != mood_) {
            mood_ = mood;
            mood_changed_ = true;
        }
        target_ = want;
        base_ = fade ? step_towards(base_, want) : want;
        last_play_ = (mood == Mood::Safe || mood == Mood::Near || mood == Mood::Frightened);
        Rgb shown = base_;
        if (flash_ != Flash::None && !purist && enabled && scene == Scene::Play) { // the flash lies over the colour: bright at the start of each pulse, dying away
            const FlashDef &d = flash_def(flash_);
            const float ph = std::fmod((float)flash_age_ * d.pulses / d.length, 1.0f), b = 1.0f - ph;
            auto mix = [&](uint8_t x, uint8_t y) -> uint8_t { return (uint8_t)(x + (y - (int)x) * b + 0.5f); };
            shown = Rgb{mix(base_.r, d.colour.r), mix(base_.g, d.colour.g), mix(base_.b, d.colour.b)};
        }
        if (flash_ != Flash::None && scene == Scene::Play && !purist && enabled && sit && ++flash_age_ >= flash_def(flash_).length)
            flash_ = Flash::None; // the next frame of the flash
        if (purist) {
            sent_ = false;
            last_ = shown;
            *out = shown;
            return true;
        }
        if (sent_ && shown == last_) return false;
        sent_ = true;
        last_ = shown;
        *out = shown;
        return true;
    }
    // On exit or when the pad goes away: true when the neutral colour has to be put back.
    bool restore(bool has_led, Rgb *out) {
        bool need = has_led && sent_;
        sent_ = false;
        if (need) {
            last_ = kNeutral;
            *out = kNeutral;
        }
        return need;
    }
    void invalidate() {
        sent_ = false;
        last_play_ = false;
        mood_ = Mood::None;
        have_prev_ = false;
        flash_ = Flash::None;
    } // a new pad: send again
    // After a load, a rewind, a restart: nothing is compared with the frame before. A restart (power_on) also runs the self-test again.
    void discontinuity(bool power_on = false) {
        have_prev_ = false;
        flash_ = Flash::None;
        if (power_on) frames_ = 0;
    }
    bool sent() const { return sent_; }
    Mood mood() const { return mood_; }
    bool mood_changed() const { return mood_changed_; }    // set by the last update(): a new mood (the log writes one line per mood change)
    Flash flash_started() const { return flash_started_; } // set by the last update(): a flash began in this frame
    Flash flash() const { return flash_; }
    Rgb target() const { return target_; }

private:
    // Looks at one machine frame against the one before: death, extra life, round cleared, new game. Returns the flash that starts now (None if none).
    Flash events(const Situation &s) {
        Flash f = Flash::None;
        if (have_prev_ && frames_ > kSelfTestFrames) {
            const Situation &p = prev_;
            const bool both_playing = s.main_state == 3 && p.main_state == 3;
            if (s.main_state == 3 && p.main_state != 3) flash_ = Flash::None; // a new game: back to yellow, no leftover flash
            if (both_playing) {
                if (s.death_anim != 0 && p.death_anim == 0) f = Flash::Death;
                else if (s.player == p.player && s.lives == p.lives + 1 && s.score > p.score) f = Flash::ExtraLife;
                else if (s.dots == 244 && p.dots != 244) f = Flash::RoundCleared;
            }
        }
        prev_ = s;
        have_prev_ = true;
        if (f != Flash::None) {
            flash_ = f;
            flash_age_ = 0;
            flash_started_ = f;
        }
        return f;
    }
    bool sent_ = false, last_play_ = false, mood_changed_ = false, have_prev_ = false;
    Rgb last_, base_, target_;
    Mood mood_ = Mood::None;
    Flash flash_ = Flash::None, flash_started_ = Flash::None;
    int flash_age_ = 0, frames_ = 0;
    Situation prev_;
};

} // namespace lightbar
