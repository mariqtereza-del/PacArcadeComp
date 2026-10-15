// S27: recording and replaying the player's inputs, and the per-frame state hash used to prove two runs (or the C++ port and the Python prototype)
// behave identically.
//
// Replay file (plain text, one header and then changes only):
//   PACMAN-REPLAY 1
//   dip C9            DIP switch 1 value (hex)
//   frames 7273       how many frames the replay covers
//   0 FF FF           from frame 0 on: IN0 = FF, IN1 = FF (active low, the bytes the CPU reads at 5000 and 5040)
//   400 DF FF         from frame 400 on: coin 1 pressed
//   ...               a line only when a value changes; frame numbers count from 0 and a value holds until the next line
// Lines starting with # are comments. The inputs of frame n are applied before the machine runs frame n (its hash is logged as frame n+1).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct ReplayEntry {
    uint32_t frame;
    uint8_t in0, in1;
};

class Replay {
public:
    uint8_t dip = 0xC9;
    uint32_t frames = 0;
    std::vector<ReplayEntry> entries; // sorted by frame, first entry at frame 0

    bool load(const std::string &path, std::string &error);
    bool save(const std::string &path, std::string &error) const;
    void inputs_at(uint32_t frame, uint8_t &in0, uint8_t &in1) const; // values in force at this frame
    bool operator==(const Replay &o) const;
};

// Records what the player does, one call per frame; stores a line only when something changes.
class Recorder {
public:
    explicit Recorder(uint8_t dip) { r_.dip = dip; }
    void frame(uint8_t in0, uint8_t in1); // call before running each frame
    const Replay &replay() const { return r_; }

private:
    Replay r_;
};

// SHA-1 (40 hex characters) of the machine state that matters for a frame: video RAM, colour RAM, 4800-4BFF (zeros), work RAM, sprite RAM
// (that is memory 4000-4FFF as the Python oracle sees it) followed by the 48 bytes written to 5040-506F (sound registers, sprite coordinates).
class Bus;
std::string state_hash(const Bus &bus);
