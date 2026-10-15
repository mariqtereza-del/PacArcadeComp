// S34R: the vibration engine (src/app/haptics.h) on made-up frame snapshots. No ROM, no window, no SDL.
#include <cstdio>
#include "../src/app/haptics.h"
using namespace haptics;
static int fails = 0, checks = 0;
static void chk(const char *what, bool ok) {
    ++checks;
    if (!ok) {
        ++fails;
        std::printf("  FAIL: %s\n", what);
    }
}
static Snap playing() {
    Snap s;
    s.main_state = 3;
    s.level_state = 3;
    s.lives = 3;
    s.pac_y = 100;
    s.pac_x = 100;
    for (int i = 0; i < 4; ++i) {
        s.ghost_y[i] = 10;
        s.ghost_x[i] = 10;
    }
    return s;
}
// a fresh engine that has passed the power-on self-test and holds the given first snapshot
static Engine warm(int level, const Snap &first) {
    Engine e;
    e.set_level(level);
    for (int i = 0; i < 330; ++i) e.feed(first);
    return e;
}
int main() {
    const Profile flat = {1, 1, 1};
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.score = 10;
        unsigned ev = e.feed(b);
        chk("dot: score +10", ev == (1u << kDot));
        Out o = e.current(flat, false);
        chk("dot: small motor only", o.high > 0 && o.low == 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.score = 50;
        chk("energizer: score +50", e.feed(b) == (1u << kEnergizer));
        Out o = e.current(flat, true);
        chk("energizer: both motors and triggers", o.low > 0 && o.high > 0 && o.trig_l > 0 && o.trig_r > 0);
    }
    {
        int want[4] = {200, 400, 800, 1600};
        float last = 0;
        for (int i = 0; i < 4; ++i) {
            Snap a = playing(), b = a;
            Engine e = warm(2, a);
            b.score = want[i];
            chk("ghost chain: event", e.feed(b) == (1u << kGhost));
            Out o = e.current(flat, false);
            chk("ghost chain: each ghost is stronger", o.low > last);
            last = o.low;
        }
    }
    {
        int want[8] = {100, 300, 500, 700, 1000, 2000, 3000, 5000};
        for (int i = 0; i < 8; ++i) {
            Snap a = playing(), b = a;
            Engine e = warm(2, a);
            b.score = want[i];
            chk("fruit values", e.feed(b) == (1u << kFruit));
        }
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.score = 10;
        b.lives = 4;
        unsigned ev = e.feed(b);
        chk("extra life: lives +1 with a score change", (ev & (1u << kExtraLife)) != 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.lives = 2;
        chk("lives going down alone (a death, a player change) is not an extra life", e.feed(b) == 0);
        Engine e2 = warm(2, a);
        Snap c = a;
        c.lives = 4;
        chk("lives +1 with no score change (the other player's turn) is not an extra life", e2.feed(c) == 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.death_anim = 1;
        chk("death starts when the animation starts", e.feed(b) == (1u << kDeath));
        b.death_anim = 2;
        chk("death does not restart", e.feed(b) == 0);
        float peak = 0;
        int frames = 0;
        for (; frames < 200; ++frames) {
            Out o = e.current(flat, false);
            peak = std::max(peak, o.low);
            e.advance();
            if (!e.active()) break;
        }
        chk("death: strong", peak > 0.9f);
        chk("death: lasts about 110 frames", frames >= 105 && frames <= 112);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.level = 1;
        chk("level done", e.feed(b) == (1u << kLevelDone));
    }
    {
        Snap a = playing(), b = a;
        a.powered = 1;
        Engine e = warm(2, a);
        b.powered = 0;
        chk("fright end", e.feed(b) == (1u << kFrightEnd));
    }
    {
        Snap a = playing(), b;
        a.main_state = 2;
        b = a;
        b.credits = 1;
        Engine e2 = warm(2, a);
        chk("coin: credits +1 (attract and ready screens)", e2.feed(b) == (1u << kCoin));
    }
    {
        Snap a = playing();
        Engine e;
        e.set_level(2);
        unsigned ev = 0;
        Snap b = a;
        for (int i = 0; i < 300; ++i) {
            b.credits = i % 9;
            ev |= e.feed(b);
        }
        chk("self-test: memory sweeps in the first 300 frames are ignored", ev == 0);
    }
    {
        Snap a = playing(), far = a, close = a;
        far.ghost_y[0] = 100;
        far.ghost_x[0] = 125;
        close.ghost_y[0] = 100;
        close.ghost_x[0] = 108;
        Engine ef = warm(2, a);
        unsigned evf = ef.feed(far);
        chk("heartbeat: starts with a ghost 25 pixels away", (evf & (1u << kNear)) != 0);
        Out of = ef.current(flat, false);
        Engine ec = warm(2, a);
        unsigned evc = ec.feed(close);
        chk("heartbeat: starts with a ghost 8 pixels away", (evc & (1u << kNear)) != 0);
        Out oc = ec.current(flat, false);
        chk("heartbeat: closer is stronger", oc.low > of.low);
        Snap blue = close;
        for (int i = 0; i < 4; ++i) blue.edible[i] = 1;
        Engine eb = warm(2, a);
        chk("heartbeat: blue ghosts do not scare", (eb.feed(blue) & (1u << kNear)) == 0);
        Snap dying = close;
        dying.death_anim = 3;
        Engine ed = warm(2, a);
        chk("heartbeat: not while dying", (ed.feed(dying) & (1u << kNear)) == 0);
        Engine e3 = warm(2, a);
        int beats = 0;
        for (int i = 0; i < 96; ++i) {
            if (e3.feed(close) & (1u << kNear)) ++beats;
            e3.advance();
        }
        chk("heartbeat: one beat every 24 frames (4 in 96)", beats == 4);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(1, a);
        b.score = 10;
        chk("light: no dot purr", e.feed(b) == 0);
        Snap c = b;
        c.score = 60;
        chk("light: energizer still shakes", e.feed(c) == (1u << kEnergizer));
        Out l = e.current(flat, false);
        Engine e2 = warm(2, a);
        Snap d = a;
        d.score = 50;
        e2.feed(d);
        Out f = e2.current(flat, false);
        chk("light is softer than full", l.low < f.low);
        Snap close = a;
        close.ghost_y[0] = 100;
        close.ghost_x[0] = 105;
        Engine e3 = warm(1, a);
        chk("light: no heartbeat", (e3.feed(close) & (1u << kNear)) == 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(0, a);
        b.score = 50;
        b.death_anim = 1;
        chk("off: nothing at all", e.feed(b) == 0 && e.active() == 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        e.invalidate();
        b.score = 50;
        chk("after a load or pause the first frame is only remembered", e.feed(b) == 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.score = 50;
        e.feed(b);
        e.invalidate(true);
        chk("restart: effects are cleared", e.active() == 0);
        b.score = 100;
        chk("restart: the self-test gate is back", e.feed(b) == 0);
    }
    {
        Snap a = playing(), b = a;
        Engine e = warm(2, a);
        b.score = 1600;
        e.feed(b);
        Out x = e.current(profile_of(Kind::Xbox), true), p4 = e.current(profile_of(Kind::PS4), true), p5 = e.current(profile_of(Kind::PS5), true),
            ot = e.current(profile_of(Kind::Other), true);
        chk("Xbox: trigger motors used", x.trig_l > 0 && x.trig_r > 0);
        chk("PS4: no trigger motors", p4.trig_l == 0);
        chk("PS5: no trigger motors", p5.trig_l == 0);
        chk("other: no trigger motors", ot.trig_l == 0);
        chk("PS4 small motor boosted", p4.high >= x.high);
        chk("PS5 softer than Xbox", p5.low < x.low && p5.high <= x.high);
        chk("all strengths within 0..1", p4.high <= 1 && x.low <= 1 && p5.low >= 0);
    }
    chk("SDL types: 360/One are Xbox, 4 is PS4, 7 is PS5, others other", kind_of_sdl_type(1) == Kind::Xbox && kind_of_sdl_type(2) == Kind::Xbox &&
                                                                             kind_of_sdl_type(4) == Kind::PS4 && kind_of_sdl_type(7) == Kind::PS5 &&
                                                                             kind_of_sdl_type(0) == Kind::Other);
    chk("to_u16", to_u16(0) == 0 && to_u16(1) == 65535 && to_u16(0.5f) >= 32767 && to_u16(2) == 65535 && to_u16(-1) == 0);
    std::printf("%s: %d checks, %d failed\n", fails ? "FAIL" : "PASS", checks, fails);
    return fails ? 1 : 0;
}
