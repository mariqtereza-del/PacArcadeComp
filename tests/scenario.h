// X02: test scenarios. Runs the REAL machine (the ROM's own code) to a moment, writes a few bytes of RAM, and runs on, so extras can be tested on real
// machine behaviour (fruit, ghosts eaten, round clear, level 17) without playing for an hour. FOR TESTS ONLY: the game program never does these pokes
// (the later "assist" steps will, and then the game stops counting for records).
// RAM used (docs/notes/ram_map.md, section "Findings: X01 and X02"):
//   4E0E dots eaten (poke 69 = the fruit comes with the next dot; 243 = the next dot clears the round)     4E13 level - 1 (poke 15 = the next round is level 17)
//   4D08/4D09 Pac-Man position (y, x), 4D39/4D3A Pac-Man tile (a, b):  tile a = (y >> 3) + 32, tile b = (x >> 3) + 30; the centre of a tile is y = (a - 32) * 8 + 4, x = (b - 30) * 8 + 4
//   4D00..4D07 ghost positions (y, x per ghost, Blinky first), 4D0A..4D11 ghost tiles (a, b per ghost) -- poked equal to Pac-Man's to make a meeting
//   video RAM 4000 + 32 * (b - 30) + (a - 32): the tile code of the maze at tile (a, b): 10 dot, 14 energizer, 40 blank
#pragma once
#include <cstdint>
#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "../src/app/events.h"
#include "../src/app/replay.h"
#include "../src/board/machine.h"
#ifdef PAC_NO_Z80
#include "../src/rules/rules.h"   // N49c: the machine without the Z80 always runs the native routines
#endif

namespace scenario {

struct Tile {
    int a, b;
};
inline unsigned video_addr(Tile t) { return 0x4000 + 32 * (t.b - 30) + (t.a - 32); }
constexpr Tile kEnergizerTopLeft = {36, 33}; // video RAM 4064
constexpr Tile kBesideEnergizer = {35, 33};  // the tile above it; holding "down" walks Pac-Man onto the energizer in 4 frames
constexpr Tile kDotRight = {56, 49};         // a dot Pac-Man eats early in the S27 game (video RAM 4278 is where it is eaten in the replay)

class World {
public:
    // `prog` = the program ROM bytes; `replay` = the S27 replay (inputs that get a one-player game going). Runs until the maze is running (frame >= 700).
    using FrameHook = std::function<void(const events::Raw &, unsigned)>; // X24: called after every machine frame (also while the replay brings the game up)
    World(const std::vector<uint8_t> &prog, const std::string &replay_file, FrameHook hook = nullptr) : m_(new Machine(prog)), hook_(hook) {
#ifdef PAC_NO_Z80
        m_->rules = rules::table();
#endif
        std::string e;
        ok_ = rp_.load(replay_file, e);
        while (ok_ && (frame_ < 700 || rd(0x4E00) != 3) && frame_ < rp_.frames) step_replay();
        ok_ = ok_ && rd(0x4E00) == 3;
    }
    bool ok() const { return ok_; }
    int rd(unsigned a) { return (int)m_->bus.read(a); }
    void wr(unsigned a, int v) { m_->bus.write(a, (uint8_t)v); }
    unsigned frame() const { return frame_; }
    const events::Reader &reader() const { return reader_; }
    Machine &machine() { return *m_; }

    // One machine frame with neutral inputs (no joystick, no coin); the reader sees the finished frame.
    void step() {
        m_->bus.in0 = 0xFF;
        m_->bus.in1 = 0xFF;
        m_->bus.dsw1 = rp_.dip;
        run();
    }
    // One frame with the joystick held: 0x01 up, 0x02 left, 0x04 right, 0x08 down (the machine's own input bits, active low).
    void step_held(int dir_bit) {
        m_->bus.in0 = (uint8_t)(0xFF & ~dir_bit);
        m_->bus.in1 = 0xFF;
        m_->bus.dsw1 = rp_.dip;
        run();
    }
    void steps(int n) {
        for (int i = 0; i < n; ++i) step();
    }
    // Pac-Man is put on a tile (position and tile bytes), centred.
    void place_pac(Tile t) {
        wr(0x4D08, (t.a - 32) * 8 + 4);
        wr(0x4D09, (t.b - 30) * 8 + 4);
        wr(0x4D39, t.a);
        wr(0x4D3A, t.b);
    }
    // Ghost i (0 Blinky, 1 Pinky, 2 Inky, 3 Clyde) is put on the same tile as Pac-Man.
    void place_ghost_on_pac(int i) {
        wr(0x4D00 + 2 * i, rd(0x4D08));
        wr(0x4D01 + 2 * i, rd(0x4D09));
        wr(0x4D0A + 2 * i, rd(0x4D39));
        wr(0x4D0B + 2 * i, rd(0x4D3A));
    }
    int score() {
        int s = 0;
        for (int i = 2; i >= 0; --i) {
            int b = rd(0x4E80 + i);
            s = s * 100 + (b >> 4) * 10 + (b & 15);
        }
        return s;
    }

    // ---- the scenarios (each returns true when the situation came about; the checks are in tests/test_scenarios.cpp)
    // FRUIT: the dot counter is set to 69; the next dot brings the fruit; Pac-Man is then put 3 tiles from the fruit and walks onto it (joystick held, the
    // game's own eating code runs).
    bool fruit_on_screen_then_eaten(int max_frames = 1500) {
        wr(0x4E0E, 69);
        place_pac(kDotRight);
        int f = 0;
        while (f < max_frames && rd(0x4DD2) == 0) {
            place_pac(kDotRight);
            step();
            ++f;
        }
        if (rd(0x4DD2) == 0) return false;
        place_pac({(rd(0x4DD2) >> 3) + 32, (rd(0x4DD3) >> 3) + 30 - 3});
        for (int k = 0; k < 200 && rd(0x4DD2) != 0; ++k) step_held(0x02);
        return rd(0x4DD2) == 0;
    }
    // GHOSTS: Pac-Man is put one tile above the top-left energizer and walks onto it (joystick held, the real eating code runs); once the ghosts are blue
    // (4DA6 = 1) they are put on Pac-Man one after the other, each only when the one before has been eaten.
    bool four_ghosts_eaten(int max_frames = 1500) {
        place_pac(kBesideEnergizer);
        int f = 0;
        while (f < max_frames && rd(0x4DA6) == 0) {
            step_held(0x08);
            ++f;
        }
        if (rd(0x4DA6) == 0) return false;
        for (int g = 0; g < 4; ++g) {
            int before = (int)reader_.count(events::kGhostEaten);
            for (int k = 0; k < 400 && (int)reader_.count(events::kGhostEaten) == before; ++k) {
                place_ghost_on_pac(g);
                step();
            }
            if ((int)reader_.count(events::kGhostEaten) == before) return false;
            steps(60); // the game freezes for a moment after a ghost is eaten; Pac-Man then keeps moving by himself
        }
        return true;
    }
    // ROUND: the dot counter is set to 243 (one dot left); Pac-Man eats the next dot: the round is cleared and the next level starts.
    bool last_dot_then_next_level(int max_frames = 1500) {
        wr(0x4E0E, 243);
        place_pac(kDotRight);
        int f = 0;
        while (f < max_frames && reader_.count(events::kLevel) == 0) {
            if (rd(0x4E0E) != 244) place_pac(kDotRight);
            step();
            ++f;
        }
        steps(300);
        return reader_.count(events::kRoundCleared) > 0 && reader_.count(events::kLevel) > 0;
    }
    // LEVEL 17: the level counter is set to 15 and the round is cleared: the next level is number 17 and the game loads its tables itself.
    // The level-order pointer 4E0A/4E0B (a pointer into the list at ROM 0068, one entry per round, stays at the end) is set to the entry before level 17's (0x68 + 15).
    bool level_17() {
        wr(0x4E13, 15);
        wr(0x4E0A, 0x68 + 15);
        wr(0x4E0B, 0);
        return last_dot_then_next_level();
    }

private:
    std::function<int(unsigned)> rdfn() {
        return [this](unsigned a) { return (int)m_->bus.read(a); };
    }
    void run() {
        m_->run_frame();
        ++frame_;
        reader_.feed(events::read_raw(rdfn()), frame_);
        if (hook_) hook_(events::read_raw(rdfn()), frame_);
    }
    void step_replay() {
        uint8_t a, b;
        rp_.inputs_at(frame_, a, b);
        m_->bus.in0 = a;
        m_->bus.in1 = b;
        m_->bus.dsw1 = rp_.dip;
        m_->run_frame();
        ++frame_;
        reader_.feed(events::read_raw(rdfn()), frame_);
        if (hook_) hook_(events::read_raw(rdfn()), frame_);
    }
    std::unique_ptr<Machine> m_;
    FrameHook hook_;
    Replay rp_;
    bool ok_ = false;
    unsigned frame_ = 0;
    events::Reader reader_;
};

} // namespace scenario
