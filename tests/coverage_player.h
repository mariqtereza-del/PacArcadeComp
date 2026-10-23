// N02: coverage player A. A scripted driver that plays complete one-player games on the real machine (the ROM's own code): attract mode, coin, start,
// play with a random joystick, rounds cleared quickly (the X02 trick: poking the dot counter so the next dot clears the round), lives lost, game over,
// back to attract mode, the next game. FOR TESTS ONLY (the pokes are never done by the game program).
// Situations of set A: games starting at different levels (so level 1 to 21, the last-level rows and level 256 are all reached), rounds cleared
// (intermissions), lives lost and game over. Everything depends only on the frame number, the player's own random numbers and the machine's RAM, so
// two runs give the same frames. RAM used: 4E00 main state (1 attract, 2 credit, 3 game), 4E04 level state, 4E0E dots eaten, 4E13 level - 1, 4E14 lives.
#pragma once
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>
#include "logic_probe.h"
#include "rom_coverage.h"
#include "../src/app/replay.h"

namespace covplayer {

struct GameA {
    int start_level_minus1; // poked into 4E13 when the game starts (0 = level 1)
    int rounds_to_clear;    // rounds cleared by the dot-counter trick before the player is left to the ghosts
    int attract_frames;     // frames to watch the attract mode before the coin
};

// ten games: a long first look at attract mode, then a spread of start levels
inline const std::vector<GameA> &games_a() {
    static const std::vector<GameA> g = {{0, 0, 2600}, {0, 3, 700}, {1, 2, 650}, {3, 2, 700}, {6, 2, 600}, {10, 2, 640}, {14, 3, 620}, {17, 2, 700}, {19, 2, 660}, {254, 2, 600}};
    return g;
}

struct Stats {
    unsigned games = 0, deaths = 0, game_overs = 0, rounds_cleared = 0, timeouts = 0;
    unsigned ghosts_eaten = 0, fruits_eaten = 0, extra_lives = 0, player_switches = 0; // player B only
    unsigned max_credits_bcd = 0;                                                   // player B only: the highest value of the credit counter 4E6E
    int max_level = 0;
    unsigned long frames = 0;
    std::string checksum; // SHA-1 over the per-frame state hashes of the whole run
};

// Plays the games in order, repeating the list until `frames` frames have run. Marks coverage (may be nullptr) and returns the statistics.
inline Stats run(Machine &m, unsigned long frames, romcov::Coverage *cov) {
    Stats st;
    std::string all;
    uint32_t rng = 777;
    int dir = 0x04, hold = 0;
    size_t gi = 0;
    int phase = 0; // 0 attract, 1 coin pressed, 2 waiting for START, 3 playing
    long wait = 0, play = 0;
    int lives_before = 0, rounds_forced = 0, last_level = -1;
    m.bus.dsw1 = 0xC9;
    for (unsigned long fr = 0; fr < frames; ++fr) {
        const GameA &g = games_a()[gi % games_a().size()];
        // the power-on memory test passes through all values of 4E00 (frames 0 to 250), so the player waits for attract mode before it reads the state
        const unsigned state = fr < 300 ? 0 : m.bus.read(0x4E00);
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (state == 1) {
            if (phase == 3) { // back in attract mode: the game is over
                ++st.game_overs;
                phase = 0, wait = 0;
                ++gi;
            }
            if (phase == 0) {
                ++wait;
                if (wait > g.attract_frames && wait <= g.attract_frames + 5) in0 &= (uint8_t)~0x20; // coin
                if (wait > g.attract_frames + 5) phase = 1, wait = 0;
            }
        } else if (state == 2) {
            if (phase != 2) phase = 2, wait = 0;
            ++wait;
            if (wait > 40 && wait <= 44) in1 &= (uint8_t)~0x20; // start 1
        } else if (state == 3) {
            if (phase != 3) { // the game has just started
                phase = 3, play = 0, rounds_forced = 0, wait = 0;
                ++st.games;
                m.bus.write(0x4E13, (uint8_t)g.start_level_minus1);
                lives_before = m.bus.read(0x4E14);
                last_level = g.start_level_minus1;
            }
            ++play;
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
            if (play > 200) in0 &= (uint8_t)~dir;
            const int lives = m.bus.read(0x4E14);
            if (lives < lives_before) ++st.deaths;
            lives_before = lives;
            const int lvl = m.bus.read(0x4E13);
            if (lvl != last_level) ++st.rounds_cleared, last_level = lvl;
            if (lvl + 1 > st.max_level) st.max_level = lvl + 1;
            if (rounds_forced < g.rounds_to_clear && play % 500 == 250 && m.bus.read(0x4E04) == 3) {
                m.bus.write(0x4E0E, 243); // the next dot eaten clears the round
                ++rounds_forced;
            }
            if (play > 90000) { // safety: a game that does not end is cut short by taking the lives
                m.bus.write(0x4E14, 1);
                ++st.timeouts;
                play = 0;
            }
        }
        m.bus.in0 = in0;
        m.bus.in1 = in1;
        if (cov) probe::run_frame(m, [&](uint16_t pc) { cov->mark(pc); });
        else probe::run_frame(m, [](uint16_t) {});
        all += state_hash(m.bus);
        if ((fr & 4095) == 4095) all = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
        ++st.frames;
    }
    st.checksum = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
    return st;
}


// ================================================================================ coverage player B (N03)
// More situations: all four ghosts eaten in a row, every fruit (by start level), power pills, the extra life (bonus score), two-player games with the
// player switch, and other DIP settings (lives, bonus, difficulty, ghost names, coinage, free play). Same rules as player A: pokes only in tests, and
// everything depends on the frame number, the player's own random numbers and the machine's RAM.
// DIP switch 1 (bit 0-1 coinage: 0 free, 1 one coin one credit, 2 one coin two credits, 3 two coins one credit; bit 2-3 lives 1, 2, 3, 5; bit 4-5 bonus
// 10000, 15000, 20000, none; bit 6 difficulty; bit 7 ghost names), docs/notes/ram_map.md.
struct GameB {
    uint8_t dip;
    int players;            // 1 or 2
    int start_level_minus1; // poked into 4E13 when the game starts
    int rounds_to_clear;    // rounds cleared by the dot-counter trick
    int attract_frames;
    bool cocktail = false;  // N05: the cabinet input says cocktail table (IN1 bit 7 low); player 2 then uses the second joystick (IN1)
    int extra_coins = 0;    // N05: more coins than needed (10 or more credits show a tens digit; 99 is the cap)
    bool coin2 = false;     // N05: every other coin goes into the second coin slot (IN0 bit 6)
};

inline const std::vector<GameB> &games_b() {
    static const std::vector<GameB> g = {
        {0xC9, 2, 0, 2, 450},   // two players, 3 lives, bonus 10000, level 1: cherry, strawberry
        {0xC9, 1, 1, 2, 450},   // level 2
        {0xC8, 1, 2, 1, 450},   // free play, level 3
        {0xCA, 2, 3, 1, 450},   // one coin two credits, two players
        {0xCB, 1, 4, 1, 450},   // two coins one credit
        {0xCD, 1, 6, 2, 450},   // 5 lives
        {0xC1, 1, 8, 1, 450},   // 1 life
        {0xC5, 2, 10, 1, 450},  // 2 lives, two players
        {0xD9, 1, 12, 2, 450},  // bonus 15000
        {0xE9, 1, 0, 2, 450},   // bonus 20000
        {0xF9, 2, 5, 1, 450},   // no bonus life, two players
        {0x89, 1, 7, 2, 450},   // hard
        {0x49, 1, 9, 2, 450},   // alternate ghost names
        {0xC9, 1, 15, 2, 450},  // level 17 and on
        {0xC9, 1, 11, 1, 450},  // level 12: key
        {0xC9, 2, 2, 1, 450, true},              // N05: cocktail table, two players
        {0xCA, 1, 1, 1, 450, false, 60, true},   // N05: 60 coins into both slots at 2 credits each: the credit counter stops at 99
        {0xC9, 1, 3, 1, 450, false, 12, false},  // N05: 14 credits (a tens digit shows)
        {0xCB, 2, 4, 1, 450, true, 3, true},     // N05: cocktail, two coins one credit, second slot
    };
    return g;
}

inline int coins_total(const GameB &g);
inline int coins_needed(const GameB &g) {
    const int coinage = g.dip & 3;
    if (coinage == 0) return 1; // free play: one coin press is harmless
    if (coinage == 3) return g.players == 2 ? 4 : 2;
    if (coinage == 2) return 1;
    return g.players;
}

inline int coins_total(const GameB &g) { return coins_needed(g) + g.extra_coins; }

inline unsigned bcd(unsigned v) { return (v / 10) * 16 + v % 10; }

// The ROM reads the DIP switches once, at power-on, so every game with a different DIP setting starts on a freshly powered-on machine (the game list
// keeps equal settings together where it can; a new machine is made whenever the next game's DIP differs from the running one).
inline Stats run_b(const std::vector<uint8_t> &prog, unsigned long frames, romcov::Coverage *cov) {
    std::unique_ptr<Machine> mp(new Machine(prog));
    Machine *mpp = mp.get();
    unsigned long mf = 0; // frames since this machine was powered on
    bool cur_cocktail = games_b()[0].cocktail;
    unsigned max_credits = 0;
    Stats st;
    std::string all;
    uint32_t rng = 4242;
    int dir = 0x04, hold = 0;
    size_t gi = 0;
    int phase = 0; // 0 attract, 2 credit / waiting for START, 3 playing
    long wait = 0, play = 0, coin_clock = -1;
    int lives_before = 0, rounds_forced = 0, last_level = -1, ghost_level = -1, fruit_level = -1;
    // little tasks inside a game
    int ghost_stage = 0, ghost_idx = 0, ghost_timer = 0; // 0 idle, 1 walking to the energizer, 2 eating ghosts
    int fruit_stage = 0, fruit_timer = 0;                // 0 idle, 1 waiting for the fruit, 2 walking onto it
    bool life_poked = false;
    unsigned extra_lives = 0, ghosts_eaten = 0, fruits_eaten = 0, switches = 0;
    int last_player = 0;
    mpp->bus.dsw1 = games_b()[0].dip;
    auto place_pac = [&](int a, int b) {
        mpp->bus.write(0x4D08, (uint8_t)((a - 32) * 8 + 4));
        mpp->bus.write(0x4D09, (uint8_t)((b - 30) * 8 + 4));
        mpp->bus.write(0x4D39, (uint8_t)a);
        mpp->bus.write(0x4D3A, (uint8_t)b);
    };
    for (unsigned long fr = 0; fr < frames; ++fr) {
        const GameB &g = games_b()[gi % games_b().size()];
        if (phase == 0 && mf > 0 && (mpp->bus.dsw1 != g.dip || cur_cocktail != g.cocktail)) { // the next game needs other switches: power on again
            mp.reset(new Machine(prog));
            mpp = mp.get();
            mf = 0, wait = 0, coin_clock = -1;
        }
        cur_cocktail = g.cocktail;
        Machine &m = *mpp;
        m.bus.dsw1 = g.dip;
        const unsigned state = mf < 300 ? 0 : m.bus.read(0x4E00);
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (state == 1 && phase == 3) { // back in attract mode: the game is over
            ++st.game_overs;
            phase = 0, wait = 0, coin_clock = -1;
            ++gi;
        }
        if ((state == 1 && phase == 0) || state == 2) {
            ++wait; // (free play shows the credit state from the start, so the wait counts in both states)
            if (wait > g.attract_frames && coin_clock < 0) coin_clock = 0;
            if (coin_clock >= 0) {
                ++coin_clock;
                const int spacing = g.extra_coins ? 12 : 30;
                for (int k = 0; k < coins_total(g); ++k)
                    if (coin_clock > k * spacing && coin_clock <= k * spacing + 5) in0 &= (uint8_t)~((g.coin2 && (k & 1)) ? 0x40 : 0x20); // coin (slot 1, or slot 2)
                const long t0 = (long)coins_total(g) * spacing + 60;
                if (state == 2 && coin_clock > t0 && (coin_clock - t0) % 60 < 4) in1 &= (uint8_t)~(g.players == 2 ? 0x40 : 0x20); // start
            }
            if (state == 2) phase = 2;
        } else if (state == 3) {
            if (phase != 3) { // the game has just started
                phase = 3, play = 0, rounds_forced = 0, wait = 0, coin_clock = -1;
                ++st.games;
                m.bus.write(0x4E13, (uint8_t)g.start_level_minus1);
                lives_before = m.bus.read(0x4E14);
                last_level = g.start_level_minus1;
                ghost_level = fruit_level = -1;
                ghost_stage = fruit_stage = 0;
                life_poked = false;
                last_player = m.bus.read(0x4E09);
            }
            ++play;
            const int player = m.bus.read(0x4E09);
            if (player != last_player) ++switches, last_player = player, life_poked = false, ghost_level = fruit_level = -1, ghost_stage = fruit_stage = 0;
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
            bool scripted = false;
            const int lvl = m.bus.read(0x4E13);
            const bool running = m.bus.read(0x4E04) == 3;
            // extra life: the score is put just below the bonus score, the next dots cross it
            if (!life_poked && play > 260 && running) {
                const unsigned t = m.bus.read(0x4E71);
                if (t != 0xFF && t != 0) {
                    const unsigned tm = bcd((t >> 4) * 10 + (t & 15) - 1);
                    const unsigned base = player ? 0x4E84 : 0x4E80;
                    m.bus.write(base, 0x80);
                    m.bus.write(base + 1, (uint8_t)(((tm & 15) << 4) | 9));
                    m.bus.write(base + 2, (uint8_t)(tm >> 4));
                    const int before = m.bus.read(0x4E14);
                    (void)before;
                }
                life_poked = true;
            }
            // ghosts: at the start of each round the top-left energizer is eaten, then the ghosts are put on Pac-Man one after the other
            const bool tasks = play < 9000; // after that the player is left to the ghosts, so that every game ends
            if (ghost_stage == 0 && tasks && running && ghost_level != lvl && m.bus.read(0x4E0E) <= 1 && play > 300) {
                ghost_stage = 1, ghost_timer = 0, ghost_level = lvl;
            }
            if (ghost_stage == 1) {
                if (ghost_timer == 0) place_pac(35, 33); // once: Pac-Man then walks onto the energizer by himself (the ROM only eats when he walks onto it)
                in0 &= (uint8_t)~0x08;
                scripted = true;
                if (m.bus.read(0x4DA6) || ++ghost_timer > 200) ghost_stage = m.bus.read(0x4DA6) ? 2 : 0, ghost_idx = 0, ghost_timer = 0;
            } else if (ghost_stage == 2) {
                scripted = true;
                const int i = ghost_idx;
                m.bus.write(0x4D00 + 2 * i, m.bus.read(0x4D08));
                m.bus.write(0x4D01 + 2 * i, m.bus.read(0x4D09));
                m.bus.write(0x4D0A + 2 * i, m.bus.read(0x4D39));
                m.bus.write(0x4D0B + 2 * i, m.bus.read(0x4D3A));
                if (!m.bus.read(0x4DA7 + i) || ++ghost_timer > 400) {
                    if (!m.bus.read(0x4DA7 + i)) ++ghosts_eaten;
                    ghost_timer = 0;
                    if (++ghost_idx >= 4) ghost_stage = 3;
                }
            } else if (ghost_stage == 3) { // let the game run on a moment
                scripted = false;
                if (++ghost_timer > 90) ghost_stage = 0, ghost_timer = 0;
            }
            // fruit: the dot counter is put at 69, the next dot brings the fruit, Pac-Man is put 3 tiles away and walks onto it
            if (fruit_stage == 0 && ghost_stage == 0 && tasks && running && fruit_level != lvl && m.bus.read(0x4E0E) >= 5 && m.bus.read(0x4E0E) < 60 && play > 400) {
                m.bus.write(0x4E0E, 69);
                fruit_stage = 1, fruit_timer = 0, fruit_level = lvl;
            }
            if (fruit_stage == 1) {
                if (m.bus.read(0x4DD2) || ++fruit_timer > 900) {
                    if (m.bus.read(0x4DD2)) {
                        place_pac((m.bus.read(0x4DD2) >> 3) + 32, (m.bus.read(0x4DD3) >> 3) + 30 - 3);
                        fruit_stage = 2;
                    } else fruit_stage = 0;
                    fruit_timer = 0;
                }
            } else if (fruit_stage == 2) {
                in0 &= (uint8_t)~0x02;
                scripted = true;
                if (!m.bus.read(0x4DD2) || ++fruit_timer > 150) {
                    if (!m.bus.read(0x4DD2)) ++fruits_eaten;
                    fruit_stage = 0;
                }
            }
            if (!scripted && play > 200) {
                if (g.cocktail && player == 1) in1 &= (uint8_t)~dir; // the second joystick of the cocktail table
                else in0 &= (uint8_t)~dir;
            }
            const int lives = m.bus.read(0x4E14);
            if (lives < lives_before) ++st.deaths;
            else if (lives > lives_before && life_poked && play > 262) ++extra_lives;
            lives_before = lives;
            if (lvl != last_level) ++st.rounds_cleared, last_level = lvl;
            if (lvl + 1 > st.max_level) st.max_level = lvl + 1;
            if (rounds_forced < g.rounds_to_clear && play % 700 == 450 && running && ghost_stage == 0 && fruit_stage == 0) {
                m.bus.write(0x4E0E, 243);
                ++rounds_forced;
            }
            if (play > 90000) {
                m.bus.write(0x4E14, 1);
                ++st.timeouts;
                play = 0;
            }
        }
        if (g.cocktail) in1 &= (uint8_t)~0x80;
        m.bus.in0 = in0;
        m.bus.in1 = in1;
        if (cov) probe::run_frame(m, [&](uint16_t pc) { cov->mark(pc); });
        else probe::run_frame(m, [](uint16_t) {});
        { const unsigned cr = m.bus.read(0x4E6E); if (mf >= 300 && cr != 0xFF && cr > max_credits) max_credits = cr; }
        all += state_hash(m.bus);
        if ((fr & 4095) == 4095) all = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
        ++st.frames;
        ++mf;
    }
    st.checksum = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
    st.ghosts_eaten = ghosts_eaten, st.fruits_eaten = fruits_eaten, st.extra_lives = extra_lives, st.player_switches = switches, st.max_credits_bcd = max_credits;
    return st;
}


// ================================================================================ coverage player C (N04)
// All level changes in a row: round after round is cleared (the dot-counter trick) so that the three intermission cartoons play after the levels
// 2, 5, 9, 13 and 17 (the ROM picks the cartoon from the level counter 4E13 when a round is cleared), the level 21 plateau, and level 256 (the
// split screen). A game can start with the level counter poked once the READY screen is over (the ROM sets the counter to 0 while READY shows).
// The player keeps its lives (4E14 is topped up) while it clears rounds; afterwards it is left to the ghosts, so every game ends.
struct GameC {
    int start_level_minus1; // poked into 4E13 (and the level-order pointer 4E0A) once the first maze is running; -1 = start normally at level 1
    int rounds_to_clear;
    int attract_frames;
    bool rack_advance = false; // N05: the rack-advance switch (IN0 bit 4) is pressed twice during play; the game jumps to the next level
};

inline const std::vector<GameC> &games_c() {
    static const std::vector<GameC> g = {{-1, 24, 600}, {254, 3, 500}, {19, 4, 500}, {11, 3, 500}, {3, 4, 500}, {7, 3, 500}, {15, 4, 500}, {255, 2, 500}, {0, 0, 500, true}};
    return g;
}

inline Stats run_c(const std::vector<uint8_t> &prog, unsigned long frames, romcov::Coverage *cov) {
    Stats st;
    std::string all;
    std::unique_ptr<Machine> mp(new Machine(prog));
    Machine &m = *mp;
    uint32_t rng = 9001;
    int dir = 0x04, hold = 0;
    size_t gi = 0;
    int phase = 0;
    long wait = 0, play = 0, level_timer = 0;
    int lives_before = 0, rounds_forced = 0, last_level = -1;
    bool level_poked = false, forced_this_level = false;
    int dot_stage = 0;      // N05: the dot counter is put at 69, 169, 190, 215 and 235 in turn (second fruit; the siren steps), each time after a dot was eaten
    long dot_stage_time = 0;
    int rack_presses = 0, rack_hold = 0;
    m.bus.dsw1 = 0xC9;
    for (unsigned long fr = 0; fr < frames; ++fr) {
        const GameC &g = games_c()[gi % games_c().size()];
        const unsigned state = fr < 300 ? 0 : m.bus.read(0x4E00);
        uint8_t in0 = 0xFF, in1 = 0xFF;
        if (state == 1) {
            if (phase == 3) {
                ++st.game_overs;
                phase = 0, wait = 0;
                ++gi;
            }
            if (phase == 0) {
                ++wait;
                if (wait > g.attract_frames && wait <= g.attract_frames + 5) in0 &= (uint8_t)~0x20;
                if (wait > g.attract_frames + 5) phase = 1, wait = 0;
            }
        } else if (state == 2) {
            if (phase != 2) phase = 2, wait = 0;
            ++wait;
            if (wait > 40 && (wait - 40) % 60 < 4) in1 &= (uint8_t)~0x20;
        } else if (state == 3) {
            if (phase != 3) {
                phase = 3, play = 0, rounds_forced = 0, wait = 0, level_poked = false, forced_this_level = false, level_timer = 0, dot_stage = 0, rack_presses = 0;
                ++st.games;
                lives_before = m.bus.read(0x4E14);
                last_level = 0;
            }
            ++play;
            if (hold <= 0) {
                rng = rng * 1664525u + 1013904223u;
                static const int d[4] = {0x01, 0x02, 0x04, 0x08};
                dir = d[(rng >> 24) & 3];
                hold = 10 + (int)((rng >> 16) & 31);
            }
            --hold;
            if (play > 200) in0 &= (uint8_t)~dir;
            const bool running = m.bus.read(0x4E04) == 3;
            const bool helping = rounds_forced < g.rounds_to_clear;
            if (!level_poked && running) { // the first maze is running: now the level can be chosen
                if (g.start_level_minus1 >= 0) {
                    m.bus.write(0x4E13, (uint8_t)g.start_level_minus1);
                    const int v = g.start_level_minus1 < 20 ? g.start_level_minus1 : 20;
                    m.bus.write(0x4E0A, (uint8_t)(0x68 + v));
                    m.bus.write(0x4E0B, 0);
                    last_level = g.start_level_minus1;
                }
                level_poked = true;
                level_timer = 0;
            }
            const int lvl = m.bus.read(0x4E13);
            if (lvl != last_level) {
                ++st.rounds_cleared, last_level = lvl, level_timer = 0, forced_this_level = false, dot_stage = 0, dot_stage_time = 0;
                if (lvl + 1 > st.max_level) st.max_level = lvl + 1;
            }
            if (running) ++level_timer;
            const int lives = m.bus.read(0x4E14);
            if (lives < lives_before) {
                ++st.deaths;
                if (helping) m.bus.write(0x4E14, 3); // keeps the player alive while rounds are being cleared
            }
            lives_before = m.bus.read(0x4E14);
            if (helping && level_poked && running && !forced_this_level && level_timer > 100 && dot_stage < 5) { // (the staged dot counter values)
                static const int stage_value[5] = {69, 169, 190, 215, 235};
                const int cur = m.bus.read(0x4E0E);
                if (dot_stage_time == 0) {
                    m.bus.write(0x4E0E, (uint8_t)stage_value[dot_stage]);
                    dot_stage_time = level_timer;
                } else if (cur != stage_value[dot_stage] || level_timer - dot_stage_time > 400) { // a dot was eaten (or too long): the next value
                    ++dot_stage;
                    dot_stage_time = 0;
                }
            }
            if (g.rack_advance && running && rack_presses < 2 && rack_hold == 0 && level_timer > 300) rack_hold = 4, ++rack_presses; // the switch is held for 4 frames, twice in all
            if (rack_hold > 0) in0 &= (uint8_t)~0x10, --rack_hold; // rack advance
            if (helping && level_poked && running && !forced_this_level && dot_stage >= 5 && level_timer > 320) {
                m.bus.write(0x4E0E, 243); // the next dot eaten clears the round
                forced_this_level = true;
                ++rounds_forced;
            }
            if (play > 120000) { // safety
                m.bus.write(0x4E14, 1);
                ++st.timeouts;
                play = 0;
            }
        }
        m.bus.in0 = in0;
        m.bus.in1 = in1;
        if (cov) probe::run_frame(m, [&](uint16_t pc) { cov->mark(pc); });
        else probe::run_frame(m, [](uint16_t) {});
        all += state_hash(m.bus);
        if ((fr & 4095) == 4095) all = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
        ++st.frames;
    }
    st.checksum = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
    return st;
}


// ================================================================================ service player D (N05)
// The service (test) switch, IN1 bit 4: held at power-on the ROM shows its test screens (it reads the inputs bit by bit: the player presses the IN0 bits one
// after the other, in one run the IN1 bits too), then the switch is released and a game follows; pressed in the middle of a game it restarts the machine
// into the test. Every 9000 frames the machine is powered on again.
inline Stats run_d(const std::vector<uint8_t> &prog, unsigned long frames, romcov::Coverage *cov) {
    Stats st;
    std::string all;
    std::unique_ptr<Machine> mp(new Machine(prog));
    mp->bus.dsw1 = 0xC9;
    unsigned long mf = 0; // frames since power-on
    int round = 0;
    for (unsigned long fr = 0; fr < frames; ++fr) {
        Machine &m = *mp;
        uint8_t in0 = 0xFF, in1 = 0xFF;
        const unsigned long t = mf;
        if (round % 3 == 0 && t < 4400) { // input test: service held for 300 frames, released for 45 frames with both START buttons held, held again: the ROM tests the input bits one by one
            if (t < 300 || t >= 345) in1 &= (uint8_t)~0x10;
            if (t < 400) in1 &= (uint8_t)~0x60;
            const unsigned long k = (t / 7) % 24;
            if (t >= 345 && t < 3600 && k < 8) in0 &= (uint8_t)~(1u << k); // service stays held until t = 4400
        } else if (round % 3 != 0 && t < 3000) { // service switch held: the test screens
            in1 &= (uint8_t)~0x10;
            const unsigned long k = (t / 7) % 24;
            if (k < 8) in0 &= (uint8_t)~(1u << k);                                        // IN0 bits 0 to 7, 7 frames each
            if (round % 3 == 1 && k >= 8 && k < 12) in1 &= (uint8_t)~(1u << (k - 8));     // also IN1 bits 0 to 3
            if (round % 3 == 2 && k >= 8 && k < 12) in1 &= (uint8_t)~(1u << (k - 8 + 5)); // IN1 bits 5 to 8 (start buttons)
        } else if (t >= 4500 && t < 4510) in0 &= (uint8_t)~0x20; // coin
        else if (t >= 4700 && t < 4704) in1 &= (uint8_t)~0x20;   // START
        else if (t >= 6800 && t < 6830) in1 &= (uint8_t)~0x10;   // service pressed in the middle of a game
        m.bus.in0 = in0;
        m.bus.in1 = in1;
        if (cov) probe::run_frame(m, [&](uint16_t pc) { cov->mark(pc); });
        else probe::run_frame(m, [](uint16_t) {});
        all += state_hash(m.bus);
        if ((fr & 4095) == 4095) all = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
        ++st.frames;
        if (++mf >= 9000) { // power off and on again
            mf = 0, ++round, ++st.games;
            mp.reset(new Machine(prog));
            mp->bus.dsw1 = 0xC9;
        }
    }
    st.checksum = sha1_hex(reinterpret_cast<const uint8_t *>(all.data()), all.size());
    return st;
}

} // namespace covplayer
