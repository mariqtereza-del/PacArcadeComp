// S33: save state, rewind and the saved high score. None of this changes how the machine works: a saved state is a copy of everything the machine
// remembers between two frames (processor registers, work/video/colour/sprite RAM, the output latches, the sound registers and the sound chip's
// position in its waveforms), and loading it puts exactly that back. Running N frames from a loaded state therefore gives the same pictures,
// memory and sound as running them from the original (tests/test_state.cpp proves it frame by frame, also into a brand new machine).
// A state holds NO program or picture ROM data (the sound chip's waveform table is left out), so state files can be shared without sharing the ROM;
// loading one needs the same ROM in the program.
//
// High score: a file with one number. It is put into the game's own high-score memory (4E88-4E8A) and its on-screen digits ONCE, when the attract screen first
// appears after power-on (like the arcade's battery backup would). It is written when a game ends, only if that game's score beat the file's number AND the game
// was not assisted. Rewinding marks the running game as assisted (until the next game starts): only the best score reached BEFORE the rewind can become the record.
// Loading a state also marks it, and then the saved high score is the high score the state had when it was SAVED (if that game was plain play at the time; the
// state carries that fact as a tag), instead of what was reached before loading; nothing scored after a load or rewind can count.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../board/machine.h"
#include "../audio/wsg.h"
#include "assist.h" // X03: the shared assisted-game rule

namespace state {

// ---- save states
// tag: 0 = the game was played without any load/rewind when this state was saved; 1 = it was not (its high score must not count later)
void capture(Machine &m, const Wsg &wsg, std::vector<uint8_t> &out, uint8_t tag = 0); // everything needed to resume at this frame boundary
bool restore(
    Machine &m, Wsg &wsg, const std::vector<uint8_t> &in, std::string *error,
    uint8_t *tag_out =
        nullptr); // tag_out: the tag it was saved with (1 for states of the older version)   // false (and nothing changed) if the bytes are not a state of this build
bool save_file(const std::string &path, const std::vector<uint8_t> &bytes, std::string *error); // writes a temporary file, then renames it
bool load_file(const std::string &path, std::vector<uint8_t> &bytes, std::string *error);

// ---- X12: save slots. A slot file = a small header + the state above. Header: "PMSL", version 1, player (1/2), lives, level (1-based), score (u32), time (i64, seconds since 1970).
// A file without that header (the old quicksave.sav of S33) is an older-format state: it is read as it is, its date comes from the file's time and nothing else is known.
const int kSlots = 8;
struct SlotInfo {
    bool present = false, legacy = false;
    int64_t time = 0;
    uint32_t score = 0;
    int level = 0, lives = 0, player = 0;
};
void slot_pack(Machine &m, const std::vector<uint8_t> &state, int64_t time, std::vector<uint8_t> &out); // header (read from the machine's RAM) + state
bool slot_unpack(const std::vector<uint8_t> &in, std::vector<uint8_t> &state,
                 SlotInfo *info);                        // false if `in` is empty; a legacy file comes back unchanged with info->legacy
std::string slot_path(const std::string &dir, int n);    // slot 1 = quicksave.sav (the old quick save), 2..8 = slotN.sav
bool slot_info(const std::string &path, SlotInfo *info); // reads the file's header (false = no such file)
std::string slot_line(int n, const SlotInfo &info);      // "3 EMPTY", "3 10-14 0420 L2 12340" (the game font has no colon) (24 characters at most)

// The last N frames' states in a ring (rewind). push() once per frame; pop() gives the newest one and removes it.
class RewindBuffer {
public:
    explicit RewindBuffer(size_t capacity) : cap_(capacity) {}
    void push(const std::vector<uint8_t> &s);
    bool pop(std::vector<uint8_t> &s);
    size_t size() const { return count_; }
    void clear() {
        count_ = 0;
        head_ = 0;
    }

private:
    size_t cap_, count_ = 0, head_ = 0;
    std::vector<std::vector<uint8_t>> ring_;
};

// ---- high score
uint32_t high_score_ram(const Bus &bus);        // the number in 4E88-4E8A (BCD, little endian), 0..999999
void high_score_put(Bus &bus, uint32_t score);  // sets 4E88-4E8A and draws the digits the way the ROM's own routine (2A9B/2ABE) does
uint32_t score_ram(const Bus &bus, int player); // player 0 = 4E80-4E82, 1 = 4E84-4E86
bool high_score_read_file(const std::string &path, uint32_t *score);
bool high_score_write_file(const std::string &path, uint32_t score, std::string *error);

class HighScoreKeeper {
public:
    explicit HighScoreKeeper(const std::string &path);
    void after_tick(Machine &m);                                                   // call after every machine frame
    void note_assist(assist::Reason r = assist::kRewind) { rule_.mark(r); }        // the game was helped (default: rewound)
    void note_state_loaded(Machine &m, uint8_t tag, uint32_t high_score_in_state); // call right after restoring a state (also does note_state_replaced)
    void note_state_replaced(Machine &m); // call right after a load/rewind so the jump is not mistaken for a game starting or ending
    void finish(Machine &m);              // at program exit: a game still running counts if it was not assisted
    uint32_t stored() const { return stored_; }
    bool assisted() const { return !rule_.counts(); }
    const assist::Rule &rule() const { return rule_; } // X03: the reasons (label(), sentence())
    assist::Rule &rule() { return rule_; }             // later steps mark their own reason on it
    bool loaded() const { return loaded_; }
    const std::string &last_message() const { return msg_; } // what happened last (for the log), cleared by taking it
    std::string take_message() {
        std::string s;
        s.swap(msg_);
        return s;
    }

private:
    void commit(Machine &m);
    std::string path_, msg_;
    uint32_t stored_ = 0, legit_best_ = 0, floor_ = 0;
    bool loaded_ = false, armed_ = false;
    assist::Rule rule_;
    int prev_state_ = 0;
};

} // namespace state
