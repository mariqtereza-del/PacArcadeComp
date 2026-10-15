// X11: the light bar's flashes on the REAL machine. The S27 replay (and pokes, as in test_events.cpp: score 9,980 -> extra life, dot counter 243 -> round cleared) is run frame by frame;
// after each frame the RAM values go to lightbar::Driver (as main.cpp does) and the frames at which the driver starts a flash or changes its mood are compared with the frames
// of the event reader's events (src/app/events.h), which were verified against the game's own counters in X01.
#include "../src/app/events.h"
#include "../src/app/lightbar.h"
#include "../src/app/replay.h"
#include "../src/board/machine.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
using namespace lightbar;
static int checks = 0, fails = 0;
static void chk(const std::string &what, bool ok, const std::string &extra = "") {
    ++checks;
    if (!ok) {
        ++fails;
        std::printf("  FAIL: %s %s\n", what.c_str(), extra.c_str());
    } else std::printf("  ok: %s %s\n", what.c_str(), extra.c_str());
}
static std::vector<uint8_t> rd(const char *p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}
static std::string list(const std::vector<unsigned> &v) {
    std::string s;
    for (unsigned x : v) s += std::to_string(x) + " ";
    return s.empty() ? "-" : s;
}
struct Result {
    std::vector<unsigned> death, extra, clear, dark_start, new_game;
    std::vector<unsigned> ev_death, ev_extra, ev_clear, ev_over, ev_start;
    int sends = 0;
};
static Result run(const std::vector<uint8_t> &prog, const char *file, int scenario) {
    Result R;
    Replay rp;
    std::string e;
    if (!rp.load(file, e)) {
        std::printf("  cannot load\n");
        ++fails;
        return R;
    }
    static Machine m(prog);
    m = Machine(prog);
    events::Reader er;
    Driver d;
    Rgb c;
    Mood pm = Mood::None;
    auto r = [&](unsigned a) { return (int)m.bus.read(a); };
    for (uint32_t fr = 0; fr < rp.frames; ++fr) {
        uint8_t a, b;
        rp.inputs_at(fr, a, b);
        m.bus.in0 = a;
        m.bus.in1 = b;
        m.bus.dsw1 = rp.dip;
        if (scenario == 2 && fr == 700 && r(0x4E00) == 3) m.bus.write(0x4E0E, 243);
        if (scenario == 3 && fr == 700 && r(0x4E00) == 3) {
            m.bus.write(0x4E80, 0x80);
            m.bus.write(0x4E81, 0x99);
            m.bus.write(0x4E82, 0x00);
        }
        m.run_frame();
        er.feed(events::read_raw(r), fr + 1);
        Situation s;
        s.main_state = r(0x4E00);
        s.level_state = r(0x4E04);
        s.death_anim = r(0x4DA5);
        s.powered = r(0x4DA6);
        s.pac_y = r(0x4D08);
        s.pac_x = r(0x4D09);
        s.lives = r(0x4E14);
        s.dots = r(0x4E0E);
        s.player = r(0x4E09);
        for (int i = 0; i < 4; ++i) {
            s.ghost_y[i] = r(0x4D00 + 2 * i);
            s.ghost_x[i] = r(0x4D01 + 2 * i);
            s.edible[i] = r(0x4DA7 + i);
        }
        for (int p = 0; p < 2; ++p)
            for (int k = 2; k >= 0; --k) {
                int bb = r(0x4E80 + 4 * p + k);
                s.score += ((bb >> 4) * 10 + (bb & 15)) * (k == 2 ? 10000 : k == 1 ? 100 : 1);
            }
        if (d.update(true, true, false, Scene::Play, &s, &c)) ++R.sends;
        unsigned f = fr + 1;
        switch (d.flash_started()) {
        case Flash::Death: R.death.push_back(f); break;
        case Flash::ExtraLife: R.extra.push_back(f); break;
        case Flash::RoundCleared: R.clear.push_back(f); break;
        default: break;
        }
        if (d.mood_changed()) {
            if (d.mood() == Mood::Dark && pm != Mood::Dark) R.dark_start.push_back(f);
            if (d.mood() != Mood::Dark && pm == Mood::Dark) R.new_game.push_back(f);
        }
        if (d.mood() != Mood::None) pm = d.mood();
    }
    for (const events::Event &ev : er.log()) {
        if (ev.type == events::kDeath) R.ev_death.push_back(ev.frame);
        if (ev.type == events::kExtraLife) R.ev_extra.push_back(ev.frame);
        if (ev.type == events::kRoundCleared) R.ev_clear.push_back(ev.frame);
        if (ev.type == events::kGameOver) R.ev_over.push_back(ev.frame);
        if (ev.type == events::kGameStart) R.ev_start.push_back(ev.frame);
    }
    return R;
}
int main(int argc, char **argv) {
    std::vector<uint8_t> prog = rd("build/rom/prog.bin");
    if (prog.empty() || argc < 2) {
        std::printf("  (no ROM files or replay given)\n");
        return 1;
    }
    {
        Result R = run(prog, argv[1], 0);
        std::printf("S27 replay: red flashes at %s| events DEATH at %s\n", list(R.death).c_str(), list(R.ev_death).c_str());
        chk("S27: a red flash at every DEATH event frame", R.death == R.ev_death && !R.death.empty(), "(" + std::to_string(R.death.size()) + ")");
        chk("S27: no green or white flash in the plain replay", R.extra.empty() && R.clear.empty());
        std::printf("  game over events at %s| dark begins at %s| game starts at %s| yellow comes back at %s\n", list(R.ev_over).c_str(), list(R.dark_start).c_str(),
                    list(R.ev_start).c_str(), list(R.new_game).c_str());
        chk(
            "S27: the bar goes dark at the first frame of every GAME OVER", !R.ev_over.empty() && [&] {
                for (unsigned g : R.ev_over) {
                    bool f = false;
                    for (unsigned x : R.dark_start)
                        if (x == g) f = true;
                    if (!f) return false;
                }
                return true;
            }());
        chk(
            "S27: yellow comes back at the frame of every GAME START (after the first one)", !R.ev_start.empty() && [&] {
                for (unsigned g : R.ev_start) {
                    bool f = false;
                    for (unsigned x : R.new_game)
                        if (x == g) f = true;
                    if (!f) return false;
                }
                return true;
            }());
    }
    {
        Result R = run(prog, argv[1], 3);
        std::printf("score poke: green flash at %s| EXTRA LIFE event at %s\n", list(R.extra).c_str(), list(R.ev_extra).c_str());
        chk("extra life: one green flash, at the EXTRA LIFE event frame", R.extra == R.ev_extra && R.extra.size() == 1);
    }
    {
        Result R = run(prog, argv[1], 2);
        std::printf("round poke: white pulse at %s| ROUND CLEARED event at %s\n", list(R.clear).c_str(), list(R.ev_clear).c_str());
        chk("round cleared: one white pulse, at the ROUND CLEARED event frame", R.clear == R.ev_clear && R.clear.size() == 1);
    }
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
