// The INVINCIBLE test cheat on the REAL machine (tests/scenario.h), in both rule sets: the original ROM routines (the 23 ROM bytes at 1763 are replaced) and the readable rules with the native
// copy of that code (game::att::g_invincible). Without the cheat a ghost on Pac-Man kills him; with it a ghost that is not blue is ignored, the game keeps running, a blue ghost is still eaten
// (the whole chain of four), switching it off brings the death back, and with the cheat off nothing differs from a game that never heard of it.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include "../src/app/cheat.h"
#include "../src/rules/rules.h"
#include "scenario.h"
static int checks = 0, fails = 0;
static void chk(const std::string &what, long got, long want) { ++checks; if (got != want) { ++fails; std::printf("  FAIL: %s (got %ld, wanted %ld)\n", what.c_str(), got, want); } else std::printf("  ok: %s\n", what.c_str()); }
static std::vector<uint8_t> rdf(const char *p) { std::ifstream f(p, std::ios::binary); return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {}); }
static bool g_on = false;
static void apply(scenario::World &w) { cheat::set_invincible(w.machine().bus, g_on); game::att::g_invincible = g_on; }
static void stp(scenario::World &w) { apply(w); w.step(); }
static void stp_held(scenario::World &w, int d) { apply(w); w.step_held(d); }
static bool eat_pellet(scenario::World &w) { w.place_pac(scenario::kBesideEnergizer); for (int k = 0; k < 300 && !w.rd(0x4DA6); ++k) stp_held(w, 0x08); return w.rd(0x4DA6) == 1; }
static void run_mode(const std::vector<uint8_t> &prog, const char *replay, bool readable) {
    std::printf("== %s rules\n", readable ? "readable" : "original");
    auto mk = [&](scenario::World &w) { if (readable) w.machine().rules = rules::table(); };
    g_on = false;
    { scenario::World w(prog, replay); mk(w); int f = 0; for (; f < 600 && (int)w.reader().count(events::kDeath) == 0; ++f) { w.place_ghost_on_pac(0); stp(w); }
      chk("without the cheat a ghost on Pac-Man kills him", (int)w.reader().count(events::kDeath) >= 1, 1); }
    { scenario::World w(prog, replay); mk(w); Machine &mc = w.machine(); uint8_t orig[23]; std::memcpy(orig, mc.bus.rom + 0x1763, 23);
      chk("the 23 ROM bytes are the ones the patch expects", cheat::looks_original(orig), 1);
      g_on = true; apply(w);
      const int lives0 = w.rd(0x4E14); int moved = 0, last_x = w.rd(0x4D09);
      for (int f = 0; f < 1500; ++f) { w.place_ghost_on_pac(f % 4); stp_held(w, 0x02); if (w.rd(0x4D09) != last_x) { ++moved; last_x = w.rd(0x4D09); } }
      chk("with the cheat: 1500 frames with ghosts on Pac-Man, no death", (int)w.reader().count(events::kDeath), 0); chk("  the lives are unchanged", w.rd(0x4E14), lives0);
      chk("  the game is not frozen: Pac-Man moved while the joystick was held", moved > 20, 1); chk("  and the maze is still running", w.rd(0x4E00), 3);
      g_on = false; apply(w);
      chk("switching off puts the original bytes back", std::memcmp(mc.bus.rom + 0x1763, orig, 23) == 0, 1);
      int f = 0; for (; f < 600 && (int)w.reader().count(events::kDeath) == 0; ++f) { w.place_ghost_on_pac(0); stp(w); }
      chk("  and a ghost on Pac-Man kills him again", (int)w.reader().count(events::kDeath) >= 1, 1); }
    { scenario::World w(prog, replay); mk(w); g_on = true; apply(w); bool ok = eat_pellet(w); w.place_pac(scenario::kBesideEnergizer);
      for (int g = 0; g < 4 && ok; ++g) { int n0 = (int)w.reader().count(events::kGhostEaten); for (int k = 0; k < 400 && (int)w.reader().count(events::kGhostEaten) == n0; ++k) { w.place_ghost_on_pac(g); stp(w); }
          ok = (int)w.reader().count(events::kGhostEaten) > n0; for (int k = 0; k < 60; ++k) stp(w); }
      chk("with INVINCIBLE on, after a pellet the blue ghosts are eaten: the whole chain", ok, 1); int last = 0; for (const auto &e : w.reader().log()) if (e.type == events::kGhostEaten) last = e.value; chk("  up to place 4", last, 4);
      chk("  and nobody died", (int)w.reader().count(events::kDeath), 0); }
    { scenario::World a(prog, replay), b(prog, replay); mk(a); mk(b); g_on = false; bool same = true;
      for (int f = 0; f < 800 && same; ++f) { stp(a); stp(b); for (unsigned ad = 0x4C00; ad < 0x4F00 && same; ++ad) same = a.rd(ad) == b.rd(ad); }
      chk("cheat off: two games stay identical (nothing is written)", same, 1); }
    g_on = false; game::att::g_invincible = false;
}
int main(int argc, char **argv) {
    if (argc < 2) { std::printf("usage: test_cheat_invincible S27-replay\n"); return 2; }
    std::vector<uint8_t> prog = rdf("build/rom/prog.bin"); if (prog.empty()) { std::printf("needs build/rom/prog.bin\n"); return 2; }
#ifndef PAC_NO_Z80
    run_mode(prog, argv[1], false);   // the original ROM routines (the patched ROM bytes) need the Z80 machine
#endif
    run_mode(prog, argv[1], true);
    std::printf("%d checks, %d failed\n%s\n", checks, fails, fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails ? 1 : 0;
}
