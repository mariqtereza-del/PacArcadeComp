// N51: the named layout of the actors block (src/game/state.h). (1) anchors from docs/notes/ram_map.md are checked at compile time; (2) in a real game (the S27 replay and the
// scripted player) the struct view and the machine's memory are the same bytes in every frame, and the named fields read what the ROM's addresses hold (pixel positions,
// tile, directions, edible flags); (3) a view shifted by one byte is caught.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "board/machine.h"
#include "game/state.h"
using namespace game;
static_assert(ram::blinky_pixel == 0x4D00 && ram::pinky_pixel == 0x4D02 && ram::inky_pixel == 0x4D04 && ram::clyde_pixel == 0x4D06 && ram::pacman_pixel == 0x4D08, "pixel positions");
static_assert(ram::blinky_direction == 0x4D2C && ram::pinky_direction == 0x4D2D && ram::inky_direction == 0x4D2E && ram::pacman_direction == 0x4D30, "directions");
static_assert(ram::pacman_tile_a == 0x4D39 && ram::pacman_tile_b == 0x4D3A, "pacman tile");
static_assert(ram::pacman_dying == 0x4DA5 && ram::frightened == 0x4DA6 && ram::blinky_edible == 0x4DA7 && ram::clyde_edible == 0x4DAA, "frightened flags");
static_assert(ram::frightened_time == 0x4DBD && ram::fruit_position == 0x4DD2 && ram::blink_timer_1up == 0x4DCE, "other anchors");
static_assert(ram::blinky_vector == 0x4D1E && ram::pacman_step == 0x4D1C && ram::pacman_wanted_step == 0x4D26, "vectors");
static_assert(sizeof(Actors) == 0xD7, "size");
static_assert(ram::main_state == 0x4E00 && ram::level_state == 0x4E04 && ram::current_player == 0x4E09 && ram::dots_eaten == 0x4E0E && ram::level == 0x4E13 && ram::lives == 0x4E14 && ram::lives_displayed == 0x4E15, "flow anchors");
static_assert(ram::pill_array_p1 == 0x4E16 && ram::energizer_array_p1 == 0x4E34 && ram::credits == 0x4E6E && ram::lives_per_game == 0x4E6F && ram::bonus_life_score == 0x4E71 && ram::cocktail_mode == 0x4E72 && ram::ghost_names_mode == 0x4E75, "settings anchors");
static_assert(ram::score_p1 == 0x4E80 && ram::score_p2 == 0x4E84 && ram::high_score == 0x4E88 && ram::voice1_effect_a == 0x4E9C && ram::voice2_effect_a == 0x4EAC && ram::voice3_effect_a == 0x4EBC, "score and sound anchors");
static_assert(ram::task_queue_write_ptr == 0x4C80 && ram::frame_counter_up == 0x4C84 && ram::timer_sixtieths == 0x4C86 && ram::rng_a == 0x4C8B && ram::timed_events == 0x4C90, "system anchors");
static_assert(ram::pacman_colour == 0x4C0B && ram::fruit_sprite_code == 0x4C0C && ram::sprite_codes_copy == 0x4C22 && ram::sprite_coords_copy == 0x4C32, "sprite byte anchors");
static int bad = 0;
static void chk(bool ok, const char *what) { if (!ok) { ++bad; std::printf("  FAIL: %s\n", what); } }
int main(int argc, char **argv) {
    if (argc < 3) { std::printf("usage: test_names PROG.BIN REPLAY\n"); return 2; }
    std::vector<uint8_t> prog(0x4000);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(prog.data(), 1, prog.size(), f) != prog.size()) { std::printf("cannot read ROM\n"); return 2; }
    std::fclose(f);
    // the replay file: lines "frame in0 in1" (as tests/test_native_machine.cpp reads it)
    std::vector<unsigned> fr, a0, a1; unsigned dip = 0xC9; long nfr = 0; char line[256];
    FILE *rf = std::fopen(argv[2], "r");
    if (!rf) { std::printf("cannot read replay\n"); return 2; }
    while (std::fgets(line, sizeof line, rf)) { unsigned a, b, c; if (!std::strncmp(line, "dip ", 4)) dip = (unsigned)std::strtoul(line + 4, nullptr, 16); else if (!std::strncmp(line, "frames ", 7)) nfr = std::atol(line + 7); else if (line[0] != '#' && std::sscanf(line, "%u %x %x", &a, &b, &c) == 3) { fr.push_back(a); a0.push_back(b); a1.push_back(c); } }
    std::fclose(rf);
    static Machine m(prog);
    size_t k = 0; unsigned in0 = 0xFF, in1 = 0xFF; long compared = 0, moved = 0, powered = 0, played = 0, scored = 0; uint8_t last_px = 0;
    for (long i = 0; i < nfr && bad == 0; ++i) {
        while (k < fr.size() && fr[k] <= (unsigned)i) { in0 = a0[k]; in1 = a1[k]; ++k; }
        m.bus.in0 = (uint8_t)in0; m.bus.in1 = (uint8_t)in1; m.bus.dsw1 = (uint8_t)dip;
        m.run_frame();
        Actors &v = actors_view(m.bus.workram);
        const uint8_t *b = (const uint8_t *)&v;
        for (unsigned a = 0; a < sizeof(Actors); ++a) if (b[a] != m.bus.read((uint16_t)(0x4D00 + a))) { chk(false, "view differs from memory"); break; }
        { Flow &fl = flow_view(m.bus.workram); const uint8_t *fb = (const uint8_t *)&fl; for (unsigned a = 0; a < sizeof(Flow); ++a) if (fb[a] != m.bus.read((uint16_t)(0x4E00 + a))) { chk(false, "flow view differs from memory"); break; }
          System &sy = system_view(m.bus.workram); const uint8_t *sb = (const uint8_t *)&sy; for (unsigned a = 0; a < sizeof(System); ++a) if (sb[a] != m.bus.read((uint16_t)(0x4C80 + a))) { chk(false, "system view differs from memory"); break; }
          SpriteBytes &sp = sprite_bytes_view(m.bus.workram); const uint8_t *pb = (const uint8_t *)&sp; for (unsigned a = 0; a < sizeof(SpriteBytes); ++a) if (pb[a] != m.bus.read((uint16_t)(0x4C02 + a))) { chk(false, "sprite bytes view differs from memory"); break; }
          chk(fl.main_state == m.bus.read(0x4E00) && fl.lives == m.bus.read(0x4E14) && fl.level == m.bus.read(0x4E13) && fl.dots_eaten == m.bus.read(0x4E0E), "flow fields");
          chk(fl.score_p1[0] == m.bus.read(0x4E80) && fl.score_p1[2] == m.bus.read(0x4E82) && fl.high_score[1] == m.bus.read(0x4E89) && fl.credits == m.bus.read(0x4E6E), "score fields");
          chk(sy.frame_counter_up == m.bus.read(0x4C84) && sy.rng_a == m.bus.read(0x4C8B), "system fields");
          if (fl.main_state == 3) { ++played; if (fl.score_p1[0] | fl.score_p1[1]) ++scored; } }
        chk(v.pacman_pixel[0] == m.bus.read(0x4D08) && v.pacman_pixel[1] == m.bus.read(0x4D09), "pacman pixel");
        chk(v.blinky_pixel[0] == m.bus.read(0x4D00) && v.clyde_pixel[1] == m.bus.read(0x4D07), "ghost pixels");
        chk(v.pacman_tile_a == m.bus.read(0x4D39) && v.pacman_tile_b == m.bus.read(0x4D3A) && v.pacman_direction == m.bus.read(0x4D30), "pacman tile and direction");
        chk(v.frightened == m.bus.read(0x4DA6) && v.inky_edible == m.bus.read(0x4DA9) && v.clyde_dead == m.bus.read(0x4DAF), "flags");
        // the ram_map relation: tile_a = (pixel byte0 >> 3) + 32 while Pac-Man is in the maze
        if (m.bus.read(0x4E00) == 3 && m.bus.read(0x4E04) == 3 && v.pacman_pixel[0] > 8) chk(v.pacman_tile_a == (uint8_t)((v.pacman_pixel[0] >> 3) + 32), "ram_map relation tile = (pixel >> 3) + 32");
        if (v.pacman_pixel[0] != last_px) ++moved;
        last_px = v.pacman_pixel[0];
        if (v.frightened) ++powered;
        ++compared;
    }
    // a view one byte off must be caught
    { Actors &w = *reinterpret_cast<Actors *>(m.bus.workram + (ram::kActorsBase - 0x4C00) + 1); const uint8_t *b = (const uint8_t *)&w; bool caught = false; for (unsigned a = 0; a < sizeof(Actors) - 1; ++a) if (b[a] != m.bus.read((uint16_t)(0x4D00 + a))) { caught = true; break; } chk(caught, "a shifted view was NOT caught"); }
    std::printf("%ld frames compared (all bytes of the four blocks: %zu + %zu + %zu + %zu); Pac-Man's picture row changed in %ld frames; frightened in %ld frames\n", compared, sizeof(Actors), sizeof(Flow), sizeof(System), sizeof(SpriteBytes), moved, powered);
    chk(played > 500 && scored > 100, "the replay really played a game and scored");
    chk(moved > 50 && powered > 20, "the replay really moved Pac-Man and made ghosts blue");
    std::printf("%s test_names\n", bad ? "FAIL" : "PASS");
    return bad ? 1 : 0;
}
