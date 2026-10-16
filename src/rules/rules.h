// T02a: the readable rules IN the game. Each "adapter" here takes the place of one ROM routine when the readable rules are switched on
// (Machine::rules, off by default and always off in Purist mode): it reads the same registers and memory as the ROM routine, calls the readable
// code in src/logic/, writes the results where the ROM routine would, and returns like the routine's RET. The proof that the game stays identical
// is in tests/test_rules_*.sh (frame hashes of the whole machine compared with the ROM run, every frame).
//
// To change a rule of the game: change the readable code in src/logic/ (not this file), rebuild, and start with the readable rules on.
#pragma once
#include <cstdint>
#include "../board/machine.h"
#include "../logic/random_number.h"
#include "../logic/scoring.h"
#include "../logic/dots_and_fruit.h"
#include "../logic/ghost_targeting.h"
#include "../logic/ghost_house.h"
#include "rules_file.h"
#include "z80ops.h"
#include "z80sim.h"
#include <cstring>

namespace rules {

// Registers an adapter does not need to set because no caller reads them afterwards. In the normal game they keep their entry values; the
// tests fill them with garbage (poison != 0) to prove that the game never reads them.
enum Scratch : unsigned { kF = 1, kBC = 2, kDE = 4, kHL = 8, kIX = 16, kIY = 32, kA = 64 };
inline uint32_t poison = 0; // tests only: when not 0, the seed of the garbage written into the scratch registers

inline void fill_scratch(Machine &m, unsigned scratch) {
    if (!poison) return;
    auto next = []() {
        poison ^= poison << 13;
        poison ^= poison >> 17;
        poison ^= poison << 5;
        return (uint16_t)poison;
    };
    if (scratch & kF) m.on_set_f(next() & 0xFF);
    if (scratch & kA) m.on_set_a(next() & 0xFF);
    if (scratch & kBC) m.on_set_bc(next());
    if (scratch & kDE) m.on_set_de(next());
    if (scratch & kHL) m.on_set_hl(next());
    if (scratch & kIX) m.on_set_ix(next());
    if (scratch & kIY) m.on_set_iy(next());
}

// Leave the routine like its final RET: take the return address from the stack, and account for the time and the instruction count the ROM
// routine would have taken (cycles, and the refresh register R, which counts instruction fetches; the game never reads R, it is kept for tidiness).
inline void return_like_ret(Machine &m, unsigned cycles, unsigned instructions) {
    const uint16_t sp = (uint16_t)m.on_get_sp();
    const uint16_t ret = (uint16_t)(m.bus.read(sp) | (m.bus.read((uint16_t)(sp + 1)) << 8));
    m.on_set_sp((uint16_t)(sp + 2));
    m.on_set_pc(ret);
    m.on_set_wz(ret);
    const unsigned r = (unsigned)m.on_get_r();
    m.on_set_r((r & 0x80) | ((r + instructions) & 0x7F));
    m.on_set_is_int_disabled(false); // an instruction ran, so a preceding EI's one-instruction delay is over
    m.on_tick(cycles);
}

// T02b: a hook may only take the place of a ROM routine when the whole routine fits into what is left of the frame. The ROM loop ends the frame
// at the first instruction that reaches 50,688 cycles; a hook that runs a routine in one piece must not step over that point (the ROM would have
// been cut in the middle: the interrupt would arrive between two of its instructions). When it does not fit, the hook lets the ROM run its own
// instruction instead (m.on_step()), which is always correct. `rules::fallbacks` counts these times (a few per million frames).
inline unsigned long fallbacks = 0;
inline bool fits(const Machine &m, unsigned cycles) { return m.ticks_in_frame() + cycles < Machine::cycles_per_frame; }
inline void run_rom_instruction(Machine &m) {
    ++fallbacks;
    m.on_step();
}

// Carry on in the ROM at `pc` as if the ROM routine had run up to there: the time and the instruction count of the part that was replaced are
// added (r_increments = how much the refresh register R grows; it counts opcode fetches, ED/CB/DD/FD prefixes count twice).
inline void continue_at(Machine &m, uint16_t pc, unsigned cycles, unsigned r_increments) {
    m.on_set_pc(pc);
    m.on_set_wz(pc);
    const unsigned r = (unsigned)m.on_get_r();
    m.on_set_r((r & 0x80) | ((r + r_increments) & 0x7F));
    m.on_set_is_int_disabled(false);
    m.on_tick(cycles);
}

// ---- ROM 2A23: the random number (src/logic/random_number.h). Called from 2926 only (a frightened ghost chooses a direction).
// In: the seed in RAM 4DC9/4DCA. Out: the random byte in A, the new seed in RAM. Scratch: F, DE, HL (the caller overwrites them at once).
// The ROM routine always takes 13 instructions and 111 cycles.
inline void random_byte_2A23(Machine &m) {
    if (!fits(m, 111)) return run_rom_instruction(m); // T02b: not when the frame ends inside the routine
    uint16_t seed = (uint16_t)(m.bus.read(logic::kRandomSeedAddress) | (m.bus.read(logic::kRandomSeedAddress + 1) << 8));
    const uint8_t answer = logic::get_random_byte(seed, m.bus.rom);
    m.bus.write(logic::kRandomSeedAddress, seed & 0xFF);
    m.bus.write(logic::kRandomSeedAddress + 1, seed >> 8);
    m.on_set_a(answer);
    fill_scratch(m, kF | kDE | kHL);
    return_like_ret(m, 111, 13);
}


// The flags the Z80's CP instruction leaves: A compared with `v` (A - v, nothing stored). Needed to leave the registers exactly as the ROM does.
inline uint8_t cp_flags(uint8_t a, uint8_t v) {
    const unsigned r = (unsigned)a - v;
    uint8_t f = 0x02; // N
    if (r & 0x80) f |= 0x80;                       // S
    if ((r & 0xFF) == 0) f |= 0x40;                // Z
    if ((a & 0x0F) < (v & 0x0F)) f |= 0x10;        // H
    if (((a ^ v) & (a ^ r) & 0x80) != 0) f |= 0x04; // P/V = overflow
    if (a < v) f |= 0x01;                           // C
    f |= (uint8_t)(v & 0x28);                       // the two undocumented bits come from the operand
    return f;
}

// ---- ROM 2A5A: add_score (src/logic/scoring.h). B = the points kind. Called from 1780 and from the task table (23DA).
// The routine is cut into three parts; the rules are done here, the drawing of the digits and of the little Pac-Man pictures stays in the ROM:
//   part 1 (this hook, entry 2A5A): the demo check, adding the points, the extra life rule. Then the ROM carries on at 2A86 (draw the score), or, when a
//           life was given now, at 2B4A (draw the life pictures; the ROM returns from there to 2A86 because this hook pushes that return address).
//   part 2 (hook at 2A89, after the score is drawn): the high score rule (compare, copy). When the high score was raised the ROM carries on at 2ABE
//           (draw the high score); otherwise the routine ends here.
// Cycle counts (Z80 instruction times, checked against the ROM by tests/test_rules_scoring.cpp part 1): see the constants below.
constexpr unsigned kAddScoreDemoCycles = 31, kAddScoreDemoFetches = 3;          // ld a,(4E00); cp 1; ret z
constexpr unsigned kAddScoreP1Cycles = 363, kAddScoreP1Fetches = 51;           // up to the call at 2A86, no extra life call, player 1
constexpr unsigned kAddScoreP2ExtraCycles = 14, kAddScoreP2ExtraFetches = 2;   // player 2: the score pointer routine 2B0B takes 14 cycles and 2 instructions more
constexpr unsigned kExtraLifeAlreadyCycles = 50, kExtraLifeAlreadyFetches = 7; // "call c" taken (+7) and 2B33 ends at its first RET NZ (43)
constexpr unsigned kExtraLifeGivenCycles = 133, kExtraLifeGivenFetches = 17;   // "call c" taken (+7) and 2B33 up to 2B4A (126)

inline void add_score_2A5A(Machine &m) {
    logic::WorkRam live{m.bus.workram};
    if (live[logic::kGameStateAddress] == logic::kStateDemo) {
        if (!fits(m, kAddScoreDemoCycles)) return run_rom_instruction(m);
        m.on_set_a(logic::kStateDemo); // the ROM leaves A = the state (1) and the flags of CP 1
        m.on_set_f(cp_flags(logic::kStateDemo, 1));
        return return_like_ret(m, kAddScoreDemoCycles, kAddScoreDemoFetches);
    }
    const unsigned index = (m.on_get_bc() >> 8) & 255;
    if (index >= logic::kPointsKinds) return run_rom_instruction(m); // the ROM would read past its table; never happens in the game
    const bool player2 = live[logic::kCurrentPlayerAddress] != 0;
    uint8_t copy[logic::kWorkRamSize];
    std::memcpy(copy, m.bus.workram, sizeof copy);
    const logic::ExtraLife life = tuning().active ? add_points_tuned(logic::WorkRam{copy}, index, tuning()) : logic::add_points(logic::WorkRam{copy}, index); // T02i: the rules file
    unsigned cycles = kAddScoreP1Cycles + (player2 ? kAddScoreP2ExtraCycles : 0);
    unsigned fetches = kAddScoreP1Fetches + (player2 ? kAddScoreP2ExtraFetches : 0);
    if (life == logic::kExtraLifeAlreadyGiven) cycles += kExtraLifeAlreadyCycles, fetches += kExtraLifeAlreadyFetches;
    if (life == logic::kExtraLifeGivenNow) cycles += kExtraLifeGivenCycles, fetches += kExtraLifeGivenFetches;
    if (!fits(m, cycles)) return run_rom_instruction(m);
    std::memcpy(m.bus.workram, copy, sizeof copy);
    const uint16_t score = (uint16_t)logic::kScoreAddress[player2 ? 1 : 0];
    fill_scratch(m, kA | kF | kBC | kHL);
    m.on_set_de((uint16_t)(score + 2)); // the ROM keeps a pointer to the third score byte in DE
    if (life == logic::kExtraLifeGivenNow) {
        // the ROM's "call 2B33" pushed 2A86; 2B33 returns there after drawing the life pictures from 2B4A (B = the lives shown)
        const uint16_t sp = (uint16_t)(m.on_get_sp() - 2);
        m.bus.write(sp, 0x86);
        m.bus.write((uint16_t)(sp + 1), 0x2A);
        m.on_set_sp(sp);
        m.on_set_bc((uint16_t)((live[logic::kLivesShownAddress] << 8) | (m.on_get_bc() & 0xFF)));
        return continue_at(m, 0x2B4A, cycles, fetches);
    }
    continue_at(m, 0x2A86, cycles, fetches);
}

// Part 2, entry 2A89 (the ROM has just come back from drawing the score; DE points to the byte before the player's score).
constexpr unsigned kHighScoreSetupCycles = 35, kHighScoreSetupFetches = 5;       // inc de x3; ld hl,4E8A; ld b,3
constexpr unsigned kHighScoreEqualByteCycles = 51, kHighScoreEqualByteFetches = 7; // one byte equal, not the last
constexpr unsigned kHighScoreLowerCycles = 25, kHighScoreLowerFetches = 3;       // my byte is lower: RET C
constexpr unsigned kHighScoreHigherCycles = 31, kHighScoreHigherFetches = 4;     // my byte is higher: JR NZ to 2A9B
constexpr unsigned kHighScoreAllEqualLastCycles = 46 + 10, kHighScoreAllEqualLastFetches = 7 + 1; // last byte equal, then RET
constexpr unsigned kHighScoreRaiseCycles = 171, kHighScoreRaiseFetches = 17;     // 2A9B..2AAD for player 1 (copy, 3 x LDIR, set up the drawing): player 2 +14, +2

inline void high_score_2A89(Machine &m) {
    uint8_t copy[logic::kWorkRamSize];
    std::memcpy(copy, m.bus.workram, sizeof copy);
    const bool player2 = copy[logic::kCurrentPlayerAddress - logic::kWorkRamStart] != 0;
    unsigned equal = 0;
    const bool raised = logic::update_high_score(logic::WorkRam{copy}, &equal);
    unsigned cycles = kHighScoreSetupCycles, fetches = kHighScoreSetupFetches;
    if (equal == logic::kScoreBytes) { // all three bytes equal: nothing to do
        cycles += 2 * kHighScoreEqualByteCycles + kHighScoreAllEqualLastCycles;
        fetches += 2 * kHighScoreEqualByteFetches + kHighScoreAllEqualLastFetches;
    } else {
        cycles += equal * kHighScoreEqualByteCycles + (raised ? kHighScoreHigherCycles : kHighScoreLowerCycles);
        fetches += equal * kHighScoreEqualByteFetches + (raised ? kHighScoreHigherFetches : kHighScoreLowerFetches);
    }
    if (raised) {
        cycles += kHighScoreRaiseCycles + (player2 ? kAddScoreP2ExtraCycles : 0);
        fetches += kHighScoreRaiseFetches + (player2 ? kAddScoreP2ExtraFetches : 0);
    }
    if (!fits(m, cycles)) return run_rom_instruction(m);
    std::memcpy(m.bus.workram, copy, sizeof copy);
    if (!raised) {
        // leave the registers as the ROM's compare loop does: A = my byte, F = CP, B = loop counter, DE and HL = the pointers
        const unsigned score = logic::kScoreAddress[player2 ? 1 : 0];
        const unsigned deciding = equal < logic::kScoreBytes ? equal : logic::kScoreBytes - 1; // loop turns that ran (0-based) for the byte last compared
        const unsigned last_pos = logic::kScoreBytes - 1 - deciding;                          // which byte of the score that was
        const uint8_t mine = m.bus.read((uint16_t)(score + last_pos)), best = m.bus.read((uint16_t)(logic::kHighScoreAddress + last_pos));
        m.on_set_a(mine);
        m.on_set_f(cp_flags(mine, best));
        const bool all_equal = equal == logic::kScoreBytes;
        const unsigned moved = all_equal ? logic::kScoreBytes : equal; // pointers stepped back this many times
        m.on_set_bc((uint16_t)(((logic::kScoreBytes - moved) << 8) | (m.on_get_bc() & 0xFF)));
        m.on_set_de((uint16_t)(score + 2 - moved));
        m.on_set_hl((uint16_t)(0x4E8A - moved));
        return return_like_ret(m, cycles, fetches);
    }
    fill_scratch(m, kA | kF | kBC | kDE | kHL);
    // the ROM now draws the new high score from 2ABE: DE = the last high score byte, BC = 3 digit pairs of 4 (rows x ...), HL = where it is drawn
    m.on_set_de(0x4E8A);
    m.on_set_bc(0x0304);
    m.on_set_hl(0x43F2);
    continue_at(m, 0x2ABE, cycles, fetches);
}

// ---- T02c: dots, energizers, the ghost house counters, the power pill and the fruit (src/logic/dots_and_fruit.h)
// Three hooks cover the four ROM routines: 19CD (Pac-Man's tile; it contains the dot counting 1B08 and, for an energizer, the power pill start 1A70 as its
// own parts), 1A70 (the power pill start, also called from 2153) and 0EAD (the fruit, once every frame). The instruction times below are the Z80 times
// of the ROM instructions they replace (listed in the comments); tests/test_rules_dots.cpp part 1 proves them against the ROM on millions of calls.
namespace dots {
using z80::Cost;

// The ROM's power pill start (1A70 up to its RET): 40 instructions.
constexpr unsigned kPowerPillCycles = 415, kPowerPillFetches = 40;

// Leave the registers as the ROM does after "ld a,0; xor a; ...; ret" of 1A70, and run the rule.
inline void power_pill_state(Machine &m) {
    m.on_set_a(0);
    m.on_set_f(z80::kFlagsAfterXorA);
    m.on_set_hl(0x4EAC);
    m.on_set_ix(0x4C00);
}

// ---- ROM 1A70: the power pill starts (the ghosts turn blue). In: nothing. Out: A = 0, flags of XOR A, HL = 4EAC, IX = 4C00.
inline void power_pill_1A70(Machine &m) {
    if (!fits(m, kPowerPillCycles)) return run_rom_instruction(m);
    logic::start_power_pill(logic::WorkRam{m.bus.workram});
    power_pill_state(m);
    return_like_ret(m, kPowerPillCycles, kPowerPillFetches);
}

// ---- ROM 19CD: Pac-Man has moved; if he stands on a dot or an energizer, eat it. Returns to the caller of the movement routine.
inline void eat_19CD(Machine &m) {
    logic::WorkRam live{m.bus.workram};
    const unsigned at = logic::video_address_of_tile(live[logic::kPacmanTileAddress], live[logic::kPacmanTileAddress + 1]);
    if (at < logic::kVideoRamStart || at >= logic::kVideoRamStart + logic::kVideoRamSize) return run_rom_instruction(m); // never in the maze
    uint8_t work[logic::kWorkRamSize], video[logic::kVideoRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    std::memcpy(video, m.bus.videoram, sizeof video);
    const uint8_t tile = video[at - logic::kVideoRamStart];
    const logic::EatOutcome out = logic::pacman_eats_on_tile(logic::WorkRam{work}, logic::VideoRam{video});
    const uint8_t dots_after = work[logic::kDotsEatenAddress - logic::kWorkRamStart];

    Cost c;
    c.t(7);       // ld a,0FF
    c.t(13);      // ld (4D9D),a
    c.t(16);      // ld hl,(4D39)
    c.t(17);      // call 0065
    c.t(10);      // jp 202D
    c.t(188, 31); // 202D: the tile's video address (push af, push bc, 20 instructions, pop bc, pop af, ret)
    c.t(7);       // ld a,(hl)
    c.t(7);       // cp 10
    const uint16_t af = (uint16_t)m.on_get_af(), bc = (uint16_t)m.on_get_bc();
    uint8_t a, f;
    uint16_t hl = (uint16_t)at;
    uint16_t new_bc = bc;
    uint16_t sp_top = 0; // the return address of the last CALL made (for the stack leftovers)
    if (!out.ate) {
        c.t(7); // jr z (not taken)
        c.t(7); // cp 14
        c.t(11); // ret nz (taken)
        a = tile;
        f = z80::f_cp(tile, logic::kTileEnergizer);
        sp_top = 0x19D8;
    } else {
        if (!out.energizer) c.t(12); // jr z (taken)
        else { c.t(7); c.t(7); c.t(5); } // jr z (not taken); cp 14; ret nz (not taken)
        c.t(14, 2); // ld ix,4E0E
        c.t(23, 2); // inc (ix+0)
        c.t(7);     // and 0F
        c.t(8, 2);  // srl a
        c.t(7);     // ld b,40
        c.t(7);     // ld (hl),b
        c.t(7);     // ld b,19
        c.t(4);     // ld c,a
        c.t(8, 2);  // srl c
        c.t(17);    // call 0042
        c.t(16); c.t(7); c.t(4); c.t(7); c.t(4); // 0042: ld hl,(4C80); ld (hl),b; inc l; ld (hl),c; inc l
        if (!out.queue_wrapped) c.t(12); else { c.t(7); c.t(7); } // jr nz (taken) / (not taken) and ld l,0C0
        c.t(16); c.t(10); // ld (4C80),hl; ret
        c.t(4); c.t(7); c.t(10); // inc a; cp 1; jp z (dot: taken; energizer: not taken, 10 either way)
        if (out.energizer) c.t(4); // add a,a
        c.t(13); // ld (4D9D),a
        c.t(17); // call 1B08
        // 1B08, which ghost counts this dot (the same times for Pinky and Inky)
        switch (out.counter) {
        case logic::kCountsCommon: c.t(13); c.t(4); c.t(10); c.t(10); c.t(11); c.t(10); break; // ld a,(4E12); and a; jp z; ld hl; inc (hl); ret
        case logic::kCountsNobody: c.t(13); c.t(4); c.t(10); c.t(13); c.t(4); c.t(11); break;  // ...; ld a,(4DA3); and a; ret nz (taken)
        case logic::kCountsClyde: c.t(13); c.t(4); c.t(10); c.t(13); c.t(4); c.t(5); c.t(13); c.t(4); c.t(10); c.t(10); c.t(11); c.t(10); break;
        default: c.t(13); c.t(4); c.t(10); c.t(13); c.t(4); c.t(5); c.t(13); c.t(4); c.t(10); c.t(13); c.t(4); c.t(10); c.t(10); c.t(11); c.t(10); break;
        }
        c.t(17); // call 1A6A
        c.t(13); c.t(7); // ld a,(4D9D); cp 6
        uint8_t f_before_rrca;
        if (!out.energizer) {
            c.t(11); // ret nz (taken)
            f_before_rrca = z80::f_cp(1, 6);
        } else {
            c.t(5);  // ret nz (not taken): the power pill start follows
            c.t(kPowerPillCycles, kPowerPillFetches);
            f_before_rrca = z80::kFlagsAfterXorA;
        }
        c.t(10); c.t(13); c.t(4); // ld hl,4EBC; ld a,(4E0E); rrca
        c.t((dots_after & 1) ? 12 : 7);                        // jr c
        c.t(15, 2); c.t(15, 2); c.t(10); // set/res (hl) twice and ret
        f = f_before_rrca;
        a = z80::rrca(dots_after, f);
        hl = 0x4EBC;
        new_bc = (uint16_t)((0x19 << 8) | (out.energizer ? 1 : 0));
        sp_top = 0x1A06;
    }
    if (!fits(m, c.cycles)) return run_rom_instruction(m);
    std::memcpy(m.bus.workram, work, sizeof work);
    std::memcpy(m.bus.videoram, video, sizeof video);
    // the stack leftovers of the ROM's CALLs and PUSHes (the registers pushed by 202D, and the last return address)
    const uint16_t sp = (uint16_t)m.on_get_sp();
    m.bus.write((uint16_t)(sp - 1), (uint8_t)(sp_top >> 8));
    m.bus.write((uint16_t)(sp - 2), (uint8_t)(sp_top & 0xFF));
    m.bus.write((uint16_t)(sp - 3), 0xFF);
    m.bus.write((uint16_t)(sp - 4), (uint8_t)(af & 0xFF));
    m.bus.write((uint16_t)(sp - 5), (uint8_t)(bc >> 8));
    m.bus.write((uint16_t)(sp - 6), (uint8_t)(bc & 0xFF));
    m.on_set_af((uint16_t)((a << 8) | f));
    m.on_set_hl(hl);
    m.on_set_bc(new_bc);
    if (out.ate) m.on_set_ix(out.energizer ? 0x4C00 : 0x4E0E);
    return_like_ret(m, c.cycles, c.fetches);
}

// ---- ROM 0EAD: called once every frame: show the fruit when 70 or 170 dots have been eaten.
inline void fruit_0EAD(Machine &m) {
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    unsigned slot = 0;
    bool clamped = false;
    const uint8_t dying = work[logic::kPacmanDyingAddress - logic::kWorkRamStart], on_screen = work[logic::kFruitPointsAddress - logic::kWorkRamStart];
    const uint8_t dots = work[logic::kDotsEatenAddress - logic::kWorkRamStart];
    const uint8_t shown70 = work[logic::kFirstFruitShownAddress - logic::kWorkRamStart], shown170 = work[logic::kSecondFruitShownAddress - logic::kWorkRamStart];
    const logic::FruitOutcome out = logic::maybe_show_fruit(logic::WorkRam{work}, &slot, &clamped);
    if (out == logic::kFruitNoFreeSlot) return run_rom_instruction(m); // the ROM would run on into its data; never happens in the game
    Cost c;
    uint8_t a, f = (uint8_t)m.on_get_af();
    uint16_t hl = (uint16_t)m.on_get_hl(), de = (uint16_t)m.on_get_de(), bc = (uint16_t)m.on_get_bc();
    bool early = true;
    c.t(13); c.t(4); // ld a,(4DA5); and a
    if (out == logic::kFruitDying) { c.t(11); a = dying; f = z80::f_and(a); }
    else {
        c.t(5); c.t(13); c.t(4); // ld a,(4DD4); and a
        if (out == logic::kFruitOnScreen) { c.t(11); a = on_screen; f = z80::f_and(a); }
        else {
            c.t(5); c.t(13); c.t(7); // ld a,(4E0E); cp 46
            if (dots == logic::kFruitFirstAtDots) {
                c.t(12); c.t(13); c.t(4); // jr z (taken); ld a,(4E0C); and a
                if (out == logic::kFruit70AlreadyShown) { c.t(11); a = shown70; f = z80::f_and(a); }
                else { c.t(5); c.t(10); c.t(11); early = false; } // ld hl,4E0C; inc (hl)
            } else {
                c.t(7); c.t(7); // jr z (not taken); cp AA
                if (dots != logic::kFruitSecondAtDots) { c.t(11); a = dots; f = z80::f_cp(dots, logic::kFruitSecondAtDots); }
                else {
                    c.t(5); c.t(13); c.t(4); // ret nz (not taken); ld a,(4E0D); and a
                    if (out == logic::kFruit170AlreadyShown) { c.t(11); a = shown170; f = z80::f_and(a); }
                    else { c.t(5); c.t(10); c.t(11); c.t(12); early = false; } // ld hl,4E0D; inc (hl); jr l0ED5
                }
            }
        }
    }
    uint16_t sp_top = 0;
    if (!early) {
        c.t(10); c.t(16); c.t(10); c.t(13); c.t(7); // ld hl,8094; ld (4DD2),hl; ld hl,0EFD; ld a,(4E13); cp 14
        if (clamped) { c.t(7); c.t(7); } else c.t(12); // jr c (not taken); ld a,14 / jr c (taken)
        c.t(4); c.t(4); c.t(4); c.t(11); // ld b,a; add a,a; add a,b; rst 10h
        c.t(4); c.t(4); c.t(7); c.t(4); c.t(4); c.t(7); c.t(10); // 0010: add a,l; ld l,a; ld a,0; adc a,h; ld h,a; ld a,(hl); ret
        c.t(13); c.t(6); c.t(7); c.t(13); c.t(6); c.t(7); c.t(13); // ld (4C0C),a; inc hl; ld a,(hl); ld (4C0D),a; inc hl; ld a,(hl); ld (4DD4),a
        c.t(11);                                                  // rst 30h
        c.t(10); c.t(7); c.t(10);                                 // 0030: ld de,4C90; ld b,16; jp 0051
        for (unsigned k = 0; k < slot; ++k) { c.t(7); c.t(4); c.t(7); c.t(4); c.t(4); c.t(4); c.t(13); } // an occupied slot: ld a,(de); and a; jr z; inc e x3; djnz
        c.t(7); c.t(4); c.t(12);                                  // the free slot: ld a,(de); and a; jr z (taken)
        c.t(10); c.t(7);                                          // pop hl; ld b,3
        for (unsigned k = 0; k < 3; ++k) { c.t(7); c.t(7); c.t(6); c.t(4); c.t(k < 2 ? 13 : 8); } // ld a,(hl); ld (de),a; inc hl; inc e; djnz
        c.t(4);                                                   // jp (hl)  -> 0EFC
        c.t(10);                                                  // ret
        const uint8_t e0 = (uint8_t)(0x90 + 3 * slot);
        a = logic::kFruitTimeoutEvent[2];
        f = z80::f_inc8((uint8_t)(e0 + 2), 0); // the last INC E of the copy loop (the carry flag is clear from the AND A of the scan)
        hl = 0x0EFC;
        de = (uint16_t)(0x4C00 | (uint8_t)(e0 + 3));
        bc = (uint16_t)(bc & 0x00FF); // B = 0 after the loop
        sp_top = 0x0EF9;
    }
    if (!fits(m, c.cycles)) return run_rom_instruction(m);
    std::memcpy(m.bus.workram, work, sizeof work);
    if (!early) {
        const uint16_t sp = (uint16_t)m.on_get_sp();
        m.bus.write((uint16_t)(sp - 1), (uint8_t)(sp_top >> 8)); // the return address of RST 30h
        m.bus.write((uint16_t)(sp - 2), (uint8_t)(sp_top & 0xFF));
        m.on_set_hl(hl);
        m.on_set_de(de);
        m.on_set_bc(bc);
    }
    m.on_set_af((uint16_t)((a << 8) | f));
    return_like_ret(m, c.cycles, c.fetches);
}
} // namespace dots

// ---- T02d: the ghosts' targets and the direction chooser (src/logic/ghost_targeting.h)
// ROM 2966 (the direction chooser, with the distance helper 29EA and the multiplication helper 2A12 inside it) is also called from other places, so it
// has a hook of its own. The four target tasks (Blinky 2730, Pinky 276C, Inky 27A9, Clyde 27F1) are hooked as whole tasks and use the same chooser model.
// The model below follows the ROM instruction by instruction ONLY to leave the time, the registers and the stack leftovers exactly as the ROM does; the
// rules (which way a ghost goes, where it aims) are the readable code in src/logic/ghost_targeting.h.
namespace ghosts {
using z80::Cost;

using Stack = rules::Stack;

inline unsigned popcount8(uint8_t v) {
    unsigned c = 0;
    for (; v; v >>= 1) c += v & 1;
    return c;
}
// ROM 2A12: A*A in HL by shift and add. The time depends on the number of 1 bits in A.
inline void square_cost(Cost &c, uint8_t a) { c.t(316 + 11 * popcount8(a), 38 + popcount8(a)); }

// ROM 29EA: the squared distance of the tiles at IX (x) and IY (y), called with the return address `ret`. Out: the first square (= B:C afterwards) and the sum.
struct Dist {
    uint16_t sq1, sum;
};
inline Dist distance_29EA(Cost &c, Stack &st, uint16_t ret, logic::Tile x, logic::Tile y) {
    st.push16(ret);
    c.t(17); // call 29EA
    uint16_t sq[2];
    const uint8_t xs[2] = {x.a, x.b}, ys[2] = {y.a, y.b};
    for (int k = 0; k < 2; ++k) {
        c.t(19, 2); c.t(19, 2); c.t(4); c.t(10); // ld a,(ix+k); ld b,(iy+k); sub b; jp nc
        const uint8_t diff = xs[k] >= ys[k] ? (uint8_t)(xs[k] - ys[k]) : (uint8_t)(ys[k] - xs[k]);
        if (xs[k] < ys[k]) { c.t(4); c.t(19, 2); c.t(4); } // ld a,b; ld b,(ix+k); sub b
        st.push16(k == 0 ? 0x29FC : 0x2A0F);
        c.t(17); // call 2A12
        square_cost(c, diff);
        st.pop16();
        sq[k] = (uint16_t)(diff * diff);
        if (k == 0) { c.t(11); st.push16(sq[0]); } // push hl
    }
    c.t(10); st.pop16(); // pop bc
    c.t(11); c.t(10);    // add hl,bc; ret
    st.pop16();
    return Dist{sq[0], (uint16_t)(sq[0] + sq[1])};
}

struct ChooserOut {
    uint8_t a, f;
    uint16_t bc, de, hl, ix, iy;
};
// ROM 2966 from its first instruction to its RET (the CALL that got here is the caller's). `bc_in` = B:C at entry: they stay if no way is open.
inline ChooserOut chooser_2966(Cost &c, Stack &st, logic::Tile ghost, logic::Tile target, uint16_t bc_in, const logic::ChooserTrace &tr, uint8_t best) {
    c.t(16); c.t(20, 2); c.t(13); c.t(7); c.t(13); c.t(10); c.t(16); c.t(14, 2); c.t(14, 2); c.t(10); c.t(10); // 2966-2988: set up
    uint16_t bc = bc_in;
    for (unsigned d = 0; d < 4; ++d) {
        c.t(13); c.t(7); c.t(10); // ld a,(4D3D); cp (hl); jp z
        if (!tr.way[d].is_reverse) {
            const logic::Tile next = tr.way[d].next;
            st.push16(0x2992); c.t(17); // call 2000
            c.t(19, 2); c.t(19, 2); c.t(4); c.t(19, 2); c.t(19, 2); c.t(4); c.t(10);
            st.pop16();
            const uint8_t a_after = (uint8_t)(ghost.b + logic::kStepB[d]);
            const uint8_t f_after = z80::f_add8(ghost.b, (uint8_t)logic::kStepB[d]);
            c.t(16);                                                         // ld (4D42),hl
            st.push16(0x2998); c.t(17); c.t(10);                             // call 0065; jp 202D
            st.push16((uint16_t)((a_after << 8) | f_after)); st.push16(bc); // push af; push bc
            c.t(188, 31);                                                    // the rest of 202D, with its pops and RET
            st.pop16(); st.pop16(); st.pop16();
            c.t(7); c.t(7); c.t(7); // ld a,(hl); and C0; sub C0
            if (tr.way[d].is_wall) c.t(12); // jr z (taken)
            else {
                c.t(7);                                                      // jr z (not taken)
                st.push16((uint16_t)(0x32FF + 2 * d)); c.t(15, 2);           // push ix
                st.push16(0x4D3E); c.t(15, 2);                               // push iy
                c.t(14, 2); c.t(14, 2);                                      // ld ix,4D40; ld iy,4D42
                const Dist dist = distance_29EA(c, st, 0x29AE, target, next);
                c.t(14, 2); c.t(14, 2); st.pop16(); st.pop16();              // pop iy; pop ix
                bc = dist.sq1;
                c.t(4); c.t(16); c.t(4); c.t(15, 2); c.t(10);                // ex de,hl; ld hl,(4D44); and a; sbc hl,de; jp c
                if (tr.way[d].taken_as_best) { c.t(20, 2); c.t(13); c.t(13); } // ld (4D44),de; ld a,(4DC7); ld (4D3B),a
            }
        }
        c.t(10, 2); c.t(10, 2); c.t(10); c.t(11); c.t(7); c.t(7); c.t(10); // inc ix x2; ld hl; inc (hl); ld a,4; cp (hl); jp nz
    }
    c.t(13); c.t(4); c.t(4); c.t(7); c.t(14, 2); c.t(15, 2); c.t(19, 2); c.t(19, 2); c.t(8, 2); c.t(10); // 29D4-29E9
    ChooserOut o;
    o.a = best;
    o.f = z80::f_srl((uint8_t)(2 * best));
    o.bc = bc;
    o.de = (uint16_t)(2 * best);
    o.hl = logic::step_word(best);
    o.ix = (uint16_t)(0x32FF + 2 * best);
    o.iy = 0x4D3E;
    return o;
}

inline void set_registers(Machine &m, const ChooserOut &o) {
    m.on_set_af((uint16_t)((o.a << 8) | o.f));
    m.on_set_bc(o.bc);
    m.on_set_de(o.de);
    m.on_set_hl(o.hl);
    m.on_set_ix(o.ix);
    m.on_set_iy(o.iy);
}

// A tile that is not inside the video RAM, or a direction that is not 0-3, never happens in the maze; the ROM runs instead.
inline bool chooser_inputs_ok(const logic::Tile ghost, uint8_t dir) {
    if (dir > 3) return false;
    for (unsigned d = 0; d < 4; ++d) {
        if (d == (unsigned)(dir ^ 2)) continue;
        const logic::Tile next = logic::tile_after_step(ghost, d);
        const unsigned at = logic::video_address_of_tile(next.a, next.b);
        if (at < logic::kVideoRamStart || at >= logic::kVideoRamStart + logic::kVideoRamSize) return false;
    }
    return true;
}

// ---- ROM 2966 on its own (called from the tasks below and from 2848 and others)
inline void chooser_hook_2966(Machine &m) {
    const uint16_t hl = (uint16_t)m.on_get_hl(), de = (uint16_t)m.on_get_de();
    const uint8_t dir = (uint8_t)(m.on_get_af() >> 8);
    const logic::Tile ghost = logic::from_word(hl), target = logic::from_word(de);
    if (!chooser_inputs_ok(ghost, dir)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize], video[logic::kVideoRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    std::memcpy(video, m.bus.videoram, sizeof video);
    logic::ChooserTrace tr;
    const logic::Choice choice = logic::choose_direction(logic::WorkRam{work}, logic::VideoRam{video}, ghost, target, dir, &tr);
    Cost c;
    Stack st((uint16_t)m.on_get_sp());
    const ChooserOut o = chooser_2966(c, st, ghost, target, (uint16_t)m.on_get_bc(), tr, choice.direction);
    if (!fits(m, c.cycles)) return run_rom_instruction(m);
    std::memcpy(m.bus.workram, work, sizeof work);
    st.apply(m);
    set_registers(m, o);
    return_like_ret(m, c.cycles, c.fetches);
}

enum Task { kBlinky = 0, kPinky = 1, kInky = 2, kClyde = 3 };
// ---- the four target tasks (ROM 2730, 276C, 27A9, 27F1). Each ends with the RET of the task.
inline void target_task(Machine &m, Task who) {
    uint8_t work[logic::kWorkRamSize], video[logic::kVideoRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    std::memcpy(video, m.bus.videoram, sizeof video);
    logic::WorkRam ram{work};
    const bool bit0 = (ram[logic::kPhaseAddress] & 1) != 0;
    const bool elroy = ram[logic::kElroyAddress] != 0;
    const bool running = ram[logic::kLevelStateAddress] == logic::kLevelStateRunning;
    const logic::Tile pac = logic::tile_at(ram, logic::kPacmanTileAddress);
    const logic::Tile clyde_tile = logic::tile_at(ram, logic::kGhostTileAddress[3]);
    logic::TaskReport rep;
    // the ghost's tile and direction must be checked BEFORE the rule changes anything
    const logic::Tile ghost0 = logic::tile_at(ram, logic::kGhostTileAddress[who]);
    const uint8_t dir0 = ram[logic::kGhostDirectionAddress[who]];
    if (!chooser_inputs_ok(ghost0, dir0)) return run_rom_instruction(m);
    switch (who) {
    case kBlinky: logic::blinky_target_task(ram, logic::VideoRam{video}, &rep); break;
    case kPinky: logic::pinky_target_task(ram, logic::VideoRam{video}, &rep); break;
    case kInky: logic::inky_target_task(ram, logic::VideoRam{video}, &rep); break;
    case kClyde: logic::clyde_target_task(ram, logic::VideoRam{video}, &rep); break;
    }
    Cost c;
    Stack st((uint16_t)m.on_get_sp());
    uint16_t bc_in = (uint16_t)m.on_get_bc();
    uint16_t call_ret = 0, store_cost_hl = 16;
    (void)store_cost_hl;
    // ---- the part before the CALL 2966
    switch (who) {
    case kBlinky: {
        c.t(13); c.t(8, 2); c.t(10); // ld a,(4DC1); bit 0,a; jp nz
        bool scatter_path = false;
        if (!bit0) {
            c.t(13); c.t(4); // ld a,(4DB6); and a
            if (elroy) c.t(12);
            else {
                c.t(7); c.t(13); c.t(7); // jr nz (not taken); ld a,(4E04); cp 3
                if (!running) c.t(12); else { c.t(7); scatter_path = true; }
            }
        }
        if (scatter_path) { c.t(16); c.t(13); c.t(10); call_ret = 0x2751; }          // ld hl,(4D0A); ld a,(4D2C); ld de,221D
        else { c.t(16); c.t(20, 2); c.t(13); call_ret = 0x2765; }                   // ld hl,(4D0A); ld de,(4D39); ld a,(4D2C)
        break;
    }
    case kPinky: {
        c.t(13); c.t(8, 2); c.t(10);
        bool scatter_path = false;
        if (!bit0) {
            c.t(13); c.t(7);
            if (!running) c.t(12); else { c.t(7); scatter_path = true; }
        }
        if (scatter_path) { c.t(16); c.t(13); c.t(10); call_ret = 0x2787; }
        else { c.t(20, 2); c.t(16); c.t(11); c.t(11); c.t(11); c.t(4); c.t(16); c.t(13); call_ret = 0x27A2; } // ld de,(4D39); ld hl,(4D1C); add hl,hl x2; add hl,de; ex de,hl; ld hl,(4D0C); ld a,(4D2D)
        break;
    }
    case kInky: {
        c.t(13); c.t(8, 2); c.t(10);
        bool scatter_path = false;
        if (!bit0) {
            c.t(13); c.t(7);
            if (!running) c.t(12); else { c.t(7); scatter_path = true; }
        }
        if (scatter_path) { c.t(16); c.t(13); c.t(10); call_ret = 0x27C4; }
        else {
            // ld bc,(4D0A); ld de,(4D39); ld hl,(4D1C); add hl,hl; add hl,de; ld a,l; add a,a; sub c; ld l,a; ld a,h; add a,a; sub b; ld h,a; ex de,hl; ld hl,(4D0E); ld a,(4D2E)
            c.t(20, 2); c.t(20, 2); c.t(16); c.t(11); c.t(11); c.t(4); c.t(4); c.t(4); c.t(4); c.t(4); c.t(4); c.t(4); c.t(4); c.t(4); c.t(16); c.t(13);
            call_ret = 0x27EA;
            bc_in = (uint16_t)(ram[0x4D0A] | (ram[0x4D0B] << 8)); // B:C = Blinky's tile
        }
        break;
    }
    case kClyde: {
        c.t(13); c.t(8, 2); c.t(10);
        bool scatter_path = false;
        if (!bit0) {
            c.t(13); c.t(7);
            if (!running) c.t(12); else { c.t(7); scatter_path = true; }
        }
        if (scatter_path) { c.t(16); c.t(13); c.t(10); call_ret = 0x280C; }
        else {
            c.t(14, 2); c.t(14, 2); // ld ix,4D39; ld iy,4D10
            const Dist dist = distance_29EA(c, st, 0x281E, pac, clyde_tile);
            bc_in = dist.sq1;
            c.t(10); c.t(4); c.t(15, 2); c.t(10); // ld de,0040; and a; sbc hl,de; jp c
            if (rep.near_pacman) { c.t(16); c.t(13); c.t(10); call_ret = 0x280C; }
            else { c.t(16); c.t(20, 2); c.t(13); call_ret = 0x2834; }
        }
        break;
    }
    }
    st.push16(call_ret); c.t(17); // call 2966
    const ChooserOut o = chooser_2966(c, st, rep.ghost, rep.target, bc_in, rep.trace, rep.choice.direction);
    c.t(16); c.t(13); c.t(10); // ld (vector),hl; ld (direction),a; ret
    if (!fits(m, c.cycles)) return run_rom_instruction(m);
    std::memcpy(m.bus.workram, work, sizeof work);
    st.apply(m);
    set_registers(m, o);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void blinky_2730(Machine &m) { target_task(m, kBlinky); }
inline void pinky_276C(Machine &m) { target_task(m, kPinky); }
inline void inky_27A9(Machine &m) { target_task(m, kInky); }
inline void clyde_27F1(Machine &m) { target_task(m, kClyde); }
} // namespace ghosts


// ---- T02e: the movement of Pac-Man (ROM 1806) and the four ghosts (1B36, 1C4B, 1D22, 1DF9), every update of the game (several times per frame).
// The readable rules (src/logic/movement.h) change the game state; rules::sim::Sim (z80sim.h) walks the same instructions as the ROM routine to find the
// time and the leftovers (registers, flags, stack). When the model meets something it does not cover (a tile address outside the screen RAMs, the
// fruit being eaten, the end of the frame inside the routine) the ROM runs its own instruction instead.
namespace movement {
inline void set_registers(Machine &m, const sim::Sim &s) {
    m.on_set_af(s.af());
    m.on_set_bc(s.bc());
    m.on_set_de(s.de());
    m.on_set_hl(s.hl());
    m.on_set_ix(s.ix);
    m.on_set_iy(s.iy);
}
inline void finish(Machine &m, sim::Sim &s, sim::Exit ex, uint8_t *work) {
    std::memcpy(m.bus.workram, work, sizeof s.work);
    s.st.apply(m);
    set_registers(m, s);
    if (ex == sim::Exit::kTo19CD) continue_at(m, 0x19CD, s.k.cycles, s.k.fetches);
    else if (ex == sim::Exit::kTo2BEA) continue_at(m, 0x0792, s.k.cycles, s.k.fetches);
    else return_like_ret(m, s.k.cycles, s.k.fetches);
}
inline void pacman_1806(Machine &m) {
    sim::Sim s(m);
    const sim::Exit ex = s.pacman();
    if (s.stray || ex == sim::Exit::kFallback || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::pacman_move(logic::WorkRam{work}, logic::Screen{m.bus.videoram, m.bus.colorram}, logic::JoystickPorts{m.bus.read(0x5000), m.bus.read(0x5040)}, false);
    finish(m, s, ex, work);
}
template <unsigned G> inline void ghost_move(Machine &m) {
    sim::Sim s(m);
    const sim::Exit ex = s.ghost(G);
    if (s.stray || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::ghost_move(logic::WorkRam{work}, logic::Screen{m.bus.videoram, m.bus.colorram}, G);
    finish(m, s, ex, work);
}
} // namespace movement


// ---- T02f: the scatter / chase timer (ROM 0E36, every frame), the frightened timer (1376, every frame) and the level start (070E).
// The level start's last part, the drawing of the level's fruits (CALL 2BEA), stays ROM code: the hook does everything up to that CALL and carries on there.
namespace timers {
inline void timer_0E36(Machine &m) {
    sim::Sim s(m);
    s.timer_0E36();
    if (s.stray || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::mode_timer_tick(logic::WorkRam{work});
    movement::finish(m, s, sim::Exit::kRet, work);
}
inline void frightened_1376(Machine &m) {
    sim::Sim s(m);
    s.frightened_1376();
    if (s.stray || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::frightened_timer_tick(logic::WorkRam{work});
    movement::finish(m, s, sim::Exit::kRet, work);
}
inline void level_070E(Machine &m) {
    sim::Sim s(m);
    const unsigned level = s.level_070E();
    if (s.stray || level >= logic::kLevelCount || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::start_level_timers(logic::WorkRam{work}, level);
    if (tuning().active) apply_level(logic::WorkRam{work}, level, tuning()); // T02i: the rules file
    movement::finish(m, s, sim::Exit::kTo2BEA, work);
}
} // namespace timers

// ---- T02h: the ghost house release: the eaten ghost waiting (ROM 1066), Pinky, Inky and Clyde leaving the house (2069, 208C, 20AF) and the
// inactivity timer (13DD).
namespace house {
inline void eaten_1066(Machine &m) {
    sim::Sim s(m);
    s.eaten_ghost_1066();
    if (s.stray || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::eaten_ghost_waiting_tick(logic::WorkRam{work});
    movement::finish(m, s, sim::Exit::kRet, work);
}
template <unsigned G> inline void release(Machine &m) {
    sim::Sim s(m);
    s.house_release(G);
    if (s.stray || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::house_release_check(logic::WorkRam{work}, G);
    movement::finish(m, s, sim::Exit::kRet, work);
}
inline void inactivity_13DD(Machine &m) {
    sim::Sim s(m);
    s.inactivity_13DD();
    if (s.stray || !fits(m, s.k.cycles)) return run_rom_instruction(m);
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::inactivity_timer_tick(logic::WorkRam{work});
    movement::finish(m, s, sim::Exit::kRet, work);
}
} // namespace house

// The table Machine::rules points to. One entry per ROM routine that has a readable version in use.
inline const Machine::RuleHook *table() {
    static Machine::RuleHook t[0x10000] = {};
    static bool made = false;
    if (!made) {
        t[0x2A23] = random_byte_2A23;
        t[0x2A5A] = add_score_2A5A;
        t[0x2A89] = high_score_2A89;
        t[0x19CD] = dots::eat_19CD;
        t[0x1A70] = dots::power_pill_1A70;
        t[0x0EAD] = dots::fruit_0EAD;
        t[0x2966] = ghosts::chooser_hook_2966;
        t[0x2730] = ghosts::blinky_2730;
        t[0x276C] = ghosts::pinky_276C;
        t[0x27A9] = ghosts::inky_27A9;
        t[0x27F1] = ghosts::clyde_27F1;
    t[0x0E36] = timers::timer_0E36;
    t[0x1376] = timers::frightened_1376;
    t[0x070E] = timers::level_070E;
    t[0x1806] = movement::pacman_1806;
    t[0x1B36] = movement::ghost_move<0>;
    t[0x1C4B] = movement::ghost_move<1>;
    t[0x1D22] = movement::ghost_move<2>;
    t[0x1DF9] = movement::ghost_move<3>;
    t[0x1066] = house::eaten_1066;
    t[0x2069] = house::release<0>;
    t[0x208C] = house::release<1>;
    t[0x20AF] = house::release<2>;
    t[0x13DD] = house::inactivity_13DD;
        made = true;
    }
    return t;
}

} // namespace rules
