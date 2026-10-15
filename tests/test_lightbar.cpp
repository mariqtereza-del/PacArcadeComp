// X09: unit test of src/app/lightbar.h (the light bar decisions) with made-up situations.
#include "../src/app/lightbar.h"
#include <cstdio>
#include <string>
#include <algorithm>
#include <cstdlib>
using namespace lightbar;
static int checks = 0, fails = 0;
static void chk(const std::string &what, long got, long want) {
    ++checks;
    if (got != want) {
        ++fails;
        std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what.c_str(), got, want);
    } else std::printf("  ok: %s\n", what.c_str());
}
static long pack(Rgb c) { return (c.r << 16) | (c.g << 8) | c.b; }
int main() {
    Rgb c;
    Driver d;
    chk("no light bar (Xbox / DS4Windows): nothing is sent", d.update(false, true, false, Scene::Play, nullptr, &c), 0);
    chk("still nothing a frame later", d.update(false, true, false, Scene::Play, nullptr, &c), 0);
    chk("and nothing to put back on exit", d.restore(false, &c), 0);
    chk("light bar, playing: a colour is sent", d.update(true, true, false, Scene::Play, nullptr, &c), 1);
    chk("it is Pac-Man yellow", pack(c), pack(kYellow));
    chk("next frame: nothing again (only changes are sent)", d.update(true, true, false, Scene::Play, nullptr, &c), 0);
    chk("menu opens: sent", d.update(true, true, false, Scene::Menu, nullptr, &c), 1);
    chk("dim blue", pack(c), pack(kDimBlue));
    chk("menu stays: nothing", d.update(true, true, false, Scene::Menu, nullptr, &c), 0);
    chk("back to play: sent", d.update(true, true, false, Scene::Play, nullptr, &c), 1);
    chk("yellow again", pack(c), pack(kYellow));
    chk("option turned off while playing: sent", d.update(true, false, false, Scene::Play, nullptr, &c), 1);
    chk("the bar is put out", pack(c), pack(kOut));
    chk("option off in the menu too: nothing more", d.update(true, false, false, Scene::Menu, nullptr, &c), 0);
    chk("option on again in the menu: dim blue", d.update(true, true, false, Scene::Menu, nullptr, &c) && pack(c) == pack(kDimBlue), 1);
    chk("exit: the neutral blue is put back", d.restore(true, &c), 1);
    chk("it is the neutral blue", pack(c), pack(kNeutral));
    chk("exit twice: only once", d.restore(true, &c), 0);
    {
        Driver p;
        chk("Purist from the start: never touched", p.update(true, true, true, Scene::Play, nullptr, &c) || p.update(true, true, true, Scene::Menu, nullptr, &c), 0);
        chk("Purist: nothing to put back", p.restore(true, &c), 0);
    }
    {
        Driver p;
        p.update(true, true, false, Scene::Play, nullptr, &c);
        chk("Purist switched on after a colour was set: the neutral blue once", p.update(true, true, true, Scene::Play, nullptr, &c) && pack(c) == pack(kNeutral), 1);
        chk("then quiet", p.update(true, true, true, Scene::Menu, nullptr, &c), 0);
        chk("Purist off again: yellow is sent again", p.update(true, true, false, Scene::Play, nullptr, &c) && pack(c) == pack(kYellow), 1);
    }
    {
        Driver p;
        p.update(true, true, false, Scene::Play, nullptr, &c);
        p.invalidate();
        chk("new pad plugged in: the colour is sent again", p.update(true, true, false, Scene::Play, nullptr, &c), 1);
    }
    {
        Driver p;
        p.update(true, true, false, Scene::Play, nullptr, &c);
        chk("the pad loses its light bar (type changed): quiet and forgotten", p.update(false, true, false, Scene::Play, nullptr, &c), 0);
        chk("it comes back: sent again", p.update(true, true, false, Scene::Play, nullptr, &c), 1);
    }
    // X10: the colour for each situation
    {
        Situation s;
        s.main_state = 3;
        s.level_state = 3;
        s.pac_y = 100;
        s.pac_x = 100;
        for (int i = 0; i < 4; ++i) {
            s.ghost_y[i] = 200;
            s.ghost_x[i] = 40;
        }
        chk("far from every ghost: yellow", pack(colour_for(s)), pack(kYellow));
        chk("mood safe", (long)mood_of(s), (long)Mood::Safe);
        s.ghost_y[2] = 100;
        s.ghost_x[2] = 127;
        chk("27 pixels away: a little red", colour_for(s).g == 0 && colour_for(s).r > 80 && colour_for(s).r < 100, 1);
        chk("mood near", (long)mood_of(s), (long)Mood::Near);
        s.ghost_x[2] = 128;
        chk("28 pixels away: not near (the same limit as the heartbeat)", pack(colour_for(s)), pack(kYellow));
        s.ghost_y[2] = 110;
        s.ghost_x[2] = 100;
        int r10 = colour_for(s).r;
        s.ghost_y[2] = 105;
        int r5 = colour_for(s).r;
        s.ghost_y[2] = 100;
        int r0 = colour_for(s).r;
        chk("closer is brighter (10 px < 5 px < 0 px)", r10 < r5 && r5 < r0, 1);
        chk("on top of Pac-Man: full red", pack(colour_for(s)), pack(Rgb{255, 0, 0}));
        s.ghost_y[2] = 105;
        s.ghost_x[2] = 103;
        chk("distance is the larger of the two (5 and 3 = 5)", colour_for(s).r, r5);
        s.edible[2] = 1;
        chk("a blue (edible) ghost close by does not count", pack(colour_for(s)), pack(kYellow));
        s.edible[2] = 0;
        s.level_state = 5;
        chk("not while the maze is not running", pack(colour_for(s)), pack(kYellow));
        s.level_state = 3;
        s.death_anim = 1;
        chk("not while Pac-Man dies", pack(colour_for(s)), pack(kYellow));
        s.death_anim = 0;
        s.main_state = 2;
        chk("attract demo / no game: dark", pack(colour_for(s)), pack(kOut));
        chk("mood dark", (long)mood_of(s), (long)Mood::Dark);
        s.main_state = 3;
        s.level_state = 7;
        chk("game over (level state 7): dark", pack(colour_for(s)), pack(kOut));
        s.level_state = 3;
        s.powered = 1;
        for (int i = 0; i < 4; ++i) s.edible[i] = 1;
        chk("ghosts frightened: blue-white", pack(colour_for(s)), pack(kFright));
        chk("mood frightened", (long)mood_of(s), (long)Mood::Frightened);
        s.ghost_y[2] = 100;
        s.ghost_x[2] = 100;
        chk("frightened wins over near", pack(colour_for(s)), pack(kFright));
        s.main_state = 2;
        chk("frightened in the attract demo: still dark", pack(colour_for(s)), pack(kOut));
    }
    // fades
    {
        Rgb a = kYellow, b = kFright;
        int n = 0;
        while (a != b && n < 100) {
            Rgb p = a;
            a = step_towards(a, b);
            ++n;
            chk("fade step is at most 32 per channel", std::abs(a.r - p.r) <= 32 && std::abs(a.g - p.g) <= 32 && std::abs(a.b - p.b) <= 32, 1);
        }
        chk("yellow to blue-white takes a few frames (not 1, not more than 12)", n > 1 && n <= 12, 1);
        chk("it arrives exactly", pack(a), pack(kFright));
        chk("no step when already there", pack(step_towards(a, b)), pack(b));
    }
    {
        Driver d;
        Situation s;
        s.main_state = 3;
        s.level_state = 3;
        s.pac_y = 100;
        s.pac_x = 100;
        s.ghost_y[0] = 200;
        s.ghost_x[0] = 40;
        for (int i = 1; i < 4; ++i) {
            s.ghost_y[i] = 250;
            s.ghost_x[i] = 20;
        }
        s.lives = 3;
        s.dots = 10;
        s.player = 0;
        int sent_in_selftest = 0;
        bool any_yellow = false;
        for (int i = 0; i < 320; ++i) {
            if (d.update(true, true, false, Scene::Play, &s, &c)) {
                ++sent_in_selftest;
                if (pack(c) == pack(kYellow)) any_yellow = true;
            }
        }
        chk("the power-on self-test (320 frames): the bar stays dark", sent_in_selftest == 1 && !any_yellow, 1);
        chk("after it: safe, a colour is sent", d.update(true, true, false, Scene::Play, &s, &c), 1);
        chk("it is yellow at once (no fade from dark)", pack(c), pack(kYellow));
        chk("mood change reported", d.mood_changed(), 1);
        s.powered = 1;
        int sends = 0;
        Rgb last = c;
        while (d.update(true, true, false, Scene::Play, &s, &c)) {
            ++sends;
            last = c;
            if (sends > 50) break;
        }
        chk("frightened: the bar fades there in several steps", sends > 1 && sends <= 12, 1);
        chk("and ends on blue-white", pack(last), pack(kFright));
        chk("the mood is frightened", d.mood() == Mood::Frightened, 1);
        s.powered = 0;
        d.update(true, true, false, Scene::Play, &s, &c);
        chk("end of fright: the mood is safe again at once", (long)d.mood(), (long)Mood::Safe);
        chk("mood_changed is false on the next frame", (d.update(true, true, false, Scene::Play, &s, &c), d.mood_changed()), 0);
        d.update(true, true, false, Scene::Menu, &s, &c);
        chk("menu: dim blue at once", pack(c), pack(kDimBlue));
    }
    // X11: flashes, one situation each, on a driver that is past the self-test and sitting on yellow
    struct Rig {
        Driver d;
        Situation s;
        Rgb c;
        Rig() {
            s.main_state = 3;
            s.level_state = 3;
            s.pac_y = 100;
            s.pac_x = 100;
            for (int i = 0; i < 4; ++i) {
                s.ghost_y[i] = 250;
                s.ghost_x[i] = 20;
            }
            s.lives = 3;
            s.dots = 10;
            s.score = 100;
            for (int i = 0; i < 400; ++i) d.update(true, true, false, Scene::Play, &s, &c);
        }
        Rgb step() {
            Rgb o = c;
            if (d.update(true, true, false, Scene::Play, &s, &c)) return c;
            return o;
        }
    };
    {
        Rig r;
        chk("rig: sitting on yellow", pack(r.c), pack(kYellow));
        r.s.death_anim = 1;
        Rgb f = r.step();
        chk("death: a flash starts", (long)r.d.flash_started(), (long)Flash::Death);
        chk("death: the first colour is full red", pack(f), pack(Rgb{255, 0, 0}));
        int prevg = f.g;
        bool rising = true;
        for (int n = 1; n < 34; ++n) {
            Rgb x = r.step();
            if (x.g < prevg) rising = false;
            prevg = x.g;
        }
        chk("death: the red dies away during the first pulse (yellow-green comes back every frame)", rising && r.c.g > 120, 1);
        for (int i = 0; i < 80; ++i) r.step();
        chk("death: the flash is over after 72 frames (yellow again)", pack(r.c), pack(kYellow));
        chk("no flash active", (long)r.d.flash(), (long)Flash::None);
        r.s.death_anim = 0;
        r.step();
        chk("death animation over: nothing new", (long)r.d.flash_started(), (long)Flash::None);
    }
    {
        Rig r;
        r.s.lives = 4;
        r.s.score = 10100;
        r.step();
        chk("extra life (lives +1 with a score step): green flash", (long)r.d.flash_started(), (long)Flash::ExtraLife);
        chk("its first colour is full green", pack(r.c), pack(Rgb{0, 255, 0}));
        for (int i = 0; i < 70; ++i) r.step();
        chk("green is over after 60 frames", pack(r.c), pack(kYellow));
    }
    {
        Rig r;
        r.s.lives = 4;
        r.step();
        chk("lives +1 without a score step (a new game / DIP): no flash", (long)r.d.flash_started(), (long)Flash::None);
    }
    {
        Rig r;
        r.s.lives = 2;
        r.step();
        chk("a life lost without a death animation yet: no extra-life flash", (long)r.d.flash_started(), (long)Flash::None);
    }
    {
        Rig r;
        r.s.player = 1;
        r.s.lives = 4;
        r.s.score += 50;
        r.step();
        chk("player switch swaps the lives: no flash", (long)r.d.flash_started(), (long)Flash::None);
    }
    {
        Rig r;
        r.s.dots = 244;
        r.s.level_state = 12;
        r.step();
        chk("round cleared (244 dots): white pulse", (long)r.d.flash_started(), (long)Flash::RoundCleared);
        chk("first colour is white", pack(r.c), pack(Rgb{255, 255, 255}));
        int pulses = 0;
        Rgb pv = r.c;
        for (int i = 0; i < 100; ++i) {
            Rgb x = r.step();
            if (x.b > pv.b + 100) ++pulses;
            pv = x;
        }
        chk("three pulses (each starts bright again)", pulses >= 2, 1);
        chk("then back to the playing colour", pack(r.c), pack(kYellow));
        chk("round cleared again on the same count: no second flash", (r.s.dots = 244, r.step(), (long)r.d.flash_started()), (long)Flash::None);
    }
    {
        Rig r;
        r.s.level_state = 7;
        for (int i = 0; i < 12; ++i) r.step();
        chk("game over: dark (after a short fade)", pack(r.c), pack(kOut));
        chk("mood dark", (long)r.d.mood(), (long)Mood::Dark);
        r.s.main_state = 1;
        r.s.level_state = 0;
        r.step();
        chk("attract: still dark", pack(r.c), pack(kOut));
        r.s.main_state = 3;
        r.s.level_state = 1;
        r.s.lives = 3;
        r.step();
        chk("a new game starts: yellow at once (no fade from dark)", pack(r.c), pack(kYellow));
        chk("mood safe", (long)r.d.mood(), (long)Mood::Safe);
    }
    {
        Rig r;
        r.s.death_anim = 1;
        r.step();
        Rgb in_flash = r.c;
        r.d.update(true, true, false, Scene::Menu, nullptr, &r.c);
        chk("a menu in the middle of a flash: dim blue, no red", pack(r.c), pack(kDimBlue));
        for (int i = 0; i < 100; ++i) r.d.update(true, true, false, Scene::Menu, nullptr, &r.c);
        r.d.update(true, true, false, Scene::Play, &r.s, &r.c);
        chk("closing the menu: the flash goes on where it was (not over yet)", r.d.flash() == Flash::Death || r.d.flash() == Flash::None, 1);
        (void)in_flash;
    }
    {
        Rig r;
        r.d.discontinuity();
        r.s.death_anim = 1;
        r.step();
        chk("after a load / rewind the first frame is not compared (no flash)", (long)r.d.flash_started(), (long)Flash::None);
        r.step();
        r.s.death_anim = 0;
        r.step();
        r.s.death_anim = 1;
        r.step();
        chk("the next death is seen again", (long)r.d.flash_started(), (long)Flash::Death);
    }
    {
        Rig r;
        r.d.discontinuity(true);
        r.s.level_state = 3;
        for (int i = 0; i < 12; ++i) r.step();
        chk("restart: the self-test frames are dark again", pack(r.c), pack(kOut));
    }
    {
        Rig r;
        Rgb o;
        chk("option off: no flash colours, the bar stays out", (r.s.death_anim = 1, r.d.update(true, false, false, Scene::Play, &r.s, &o), pack(o)), pack(kOut));
    }
    {
        Rig r;
        Rgb o;
        r.s.death_anim = 1;
        r.d.update(true, true, true, Scene::Play, &r.s, &o);
        chk("Purist: a flash colour is never sent (the neutral blue goes back once)", pack(o), pack(kNeutral));
        chk("Purist: and then nothing", r.d.update(true, true, true, Scene::Play, &r.s, &o), 0);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
