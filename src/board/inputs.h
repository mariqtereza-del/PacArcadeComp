// S23: the board's input ports and DIP switches (hardware_facts.md section 6). Every bit reads 0 when the control is active (active low).
// No gamepad or keyboard code here: a later step maps real devices onto these logical controls.
#pragma once
#include <cstdint>
#include "bus.h"

enum class Control {
    P1Up,
    P1Left,
    P1Right,
    P1Down,   // IN0 bits 0-3
    RackTest, // IN0 bit 4 (cheat switch, normally off)
    Coin1,
    Coin2,
    ServiceCoin, // IN0 bits 5-7
    P2Up,
    P2Left,
    P2Right,
    P2Down,      // IN1 bits 0-3 (cocktail table)
    ServiceMode, // IN1 bit 4
    Start1,
    Start2,   // IN1 bits 5-6
    Cocktail, // IN1 bit 7: pressed = 0 = cocktail cabinet; released = 1 = upright (the default)
    Count
};

// DIP switch 1 (read at 5080). Factory default 0xC9.
struct DipSettings {
    enum Coinage { FreePlay = 0, OneCoinOneCredit = 1, OneCoinTwoCredits = 2, TwoCoinsOneCredit = 3 };
    enum Lives { One = 0, Two = 1, Three = 2, Five = 3 };
    enum Bonus { At10000 = 0, At15000 = 1, At20000 = 2, NoBonus = 3 };
    Coinage coinage = OneCoinOneCredit; // bits 0-1
    Lives lives = Three;                // bits 2-3
    Bonus bonus = At10000;              // bits 4-5
    bool normal_difficulty = true;      // bit 6: 1 normal, 0 hard
    bool normal_ghost_names = true;     // bit 7: 1 normal, 0 alternate
    uint8_t to_byte() const;
    static DipSettings from_byte(uint8_t v);
};

class Inputs {
public:
    Inputs();                          // nothing pressed, upright cabinet, DIP defaults
    void set(Control c, bool pressed); // true = the control is active
    bool pressed(Control c) const;
    void release_all(); // idle; the cabinet switch keeps its setting
    void set_dip(const DipSettings &d) { dip_ = d; }
    const DipSettings &dip() const { return dip_; }
    uint8_t in0() const { return in0_; }            // the byte the CPU reads at 5000
    uint8_t in1() const { return in1_; }            // 5040
    uint8_t dsw1() const { return dip_.to_byte(); } // 5080
    void apply(Bus &bus) const;                     // call once per frame, before Machine::run_frame()
private:
    uint8_t in0_ = 0xFF, in1_ = 0xFF;
    uint8_t cocktail_ = 0; // 1 = cabinet switch set to cocktail
    DipSettings dip_;
};
