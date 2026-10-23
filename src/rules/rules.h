// T02a: the readable rules IN the game. Each "adapter" here takes the place of one ROM routine when the readable rules are switched on
// (Machine::rules, off by default and always off in Purist mode): it reads the same registers and memory as the ROM routine, calls the readable
// code in src/logic/, writes the results where the ROM routine would, and returns like the routine's RET. The proof that the game stays identical
// is in tests/test_rules_*.sh (frame hashes of the whole machine compared with the ROM run, every frame).
//
// To change a rule of the game: change the readable code in src/logic/ (not this file), rebuild, and start with the readable rules on.
#pragma once
#include "../game/state.h"
#include <cstdint>
#include "../board/machine.h"
#include "../game/helpers.h"
#include "../game/screen.h"
#include "../game/numbers.h"
#include "../game/text.h"
#include "../game/status.h"
#include "../game/sliced.h"
#include "../game/maze.h"
#include "../game/scan.h"
#include "../game/sprites.h"
#include "../game/sprite_pics.h"
#include "../game/clocks.h"
#include "../game/coins.h"
#include "../game/labels.h"
#include "../game/handler.h"
#include "../game/events.h"
#include "../game/attract.h"
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
inline unsigned long sliced_cuts = 0;   // N17: how many times a routine was run natively up to the end of the frame and left for the ROM to be cut
inline bool fits(const Machine &m, unsigned cycles) { return m.ticks_in_frame() + cycles < Machine::cycles_per_frame; }
namespace attract { inline void hook_budget(Machine &m); }   // defined further down
// N48: where the hook cannot take the whole routine in one piece (it does not fit into the rest of the frame, or a state that never happens), the instruction runner of the
// N steps carries on from this very address instead of the ROM (it runs the same instructions with the same cycles up to the frame end, proved by the N45j-N47b cut tests).
// Only an address that the instruction list does not contain is left to the ROM (counted in `fallbacks`; the N48 test shows the count is zero).
#ifndef PAC_NO_Z80
inline bool rom_step_fallback = false;   // N49c, tests only: the tests of one routine against the ROM let the ROM (the Z80 core) carry on one instruction at a time where a hook hands over; the program (no Z80) always continues natively
#endif
inline void run_rom_instruction(Machine &m) {
#ifndef PAC_NO_Z80
    if (rom_step_fallback) { ++fallbacks; m.on_step(); return; }
#endif
    if (game::att::find((uint16_t)m.on_get_pc())) return attract::hook_budget(m);
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
        m.on_set_hl((uint16_t)((game::ram::high_score + 2) - moved));
        return return_like_ret(m, cycles, fetches);
    }
    fill_scratch(m, kA | kF | kBC | kDE | kHL);
    // the ROM now draws the new high score from 2ABE: DE = the last high score byte, BC = 3 digit pairs of 4 (rows x ...), HL = where it is drawn
    m.on_set_de((game::ram::high_score + 2));
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
    m.on_set_hl(game::ram::voice2_effect_a);
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
        hl = game::ram::voice3_effect_a;
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
    if (out.ate) m.on_set_ix(out.energizer ? 0x4C00 : game::ram::dots_eaten);
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
namespace attract { inline void hook(Machine &m); }   // N45h: defined further down; the ghost routines use it where the model falls back
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
                st.push16(game::ram::choice_ghost_tile); c.t(15, 2);                               // push iy
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
    o.iy = game::ram::choice_ghost_tile;
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
    if (!chooser_inputs_ok(ghost, dir)) return attract::hook(m);   // N45h: the native instruction runner takes over where the model falls back
    uint8_t work[logic::kWorkRamSize], video[logic::kVideoRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    std::memcpy(video, m.bus.videoram, sizeof video);
    logic::ChooserTrace tr;
    const logic::Choice choice = logic::choose_direction(logic::WorkRam{work}, logic::VideoRam{video}, ghost, target, dir, &tr);
    Cost c;
    Stack st((uint16_t)m.on_get_sp());
    const ChooserOut o = chooser_2966(c, st, ghost, target, (uint16_t)m.on_get_bc(), tr, choice.direction);
    if (!fits(m, c.cycles)) return attract::hook(m);   // N45h: the native instruction runner takes over where the model falls back
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
    if (!chooser_inputs_ok(ghost0, dir0)) return who == kBlinky ? attract::hook(m) : run_rom_instruction(m);   // N45h: only Blinky's task (2730) is native here; the other three tasks stay ROM code until N45i
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
            bc_in = (uint16_t)(ram[game::ram::blinky_tile] | (ram[(game::ram::blinky_tile + 1)] << 8)); // B:C = Blinky's tile
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
    if (!fits(m, c.cycles)) return who == kBlinky ? attract::hook(m) : run_rom_instruction(m);   // N45h: only Blinky's task (2730) is native here; the other three tasks stay ROM code until N45i
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
namespace attract { inline void hook(Machine &m); }   // N45f: defined further down; the ghost movers use it where the model falls back
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
    if (s.stray || ex == sim::Exit::kFallback || !fits(m, s.k.cycles)) return attract::hook(m);   // N45g: the native instruction runner takes over where the model falls back
    uint8_t work[logic::kWorkRamSize];
    std::memcpy(work, m.bus.workram, sizeof work);
    logic::pacman_move(logic::WorkRam{work}, logic::Screen{m.bus.videoram, m.bus.colorram}, logic::JoystickPorts{m.bus.read(0x5000), m.bus.read(0x5040)}, false);
    finish(m, s, ex, work);
}
template <unsigned G> inline void ghost_move(Machine &m) {
    sim::Sim s(m);
    const sim::Exit ex = s.ghost(G);
    if (s.stray || !fits(m, s.k.cycles)) return attract::hook(m);   // N45f: the native instruction runner (N45f) takes over where the model falls back; it runs the ROM instruction itself near the end of the frame
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


// ---- N12: the helper routines 0008, 0010, 0018, 0020, 0028, 0030, 0042 (src/game/helpers.h). They are entered with the return address on the stack (called
// by RST or CALL), take exactly the time and leave exactly the registers the ROM routine leaves; there are no scratch registers here.
namespace helpers {
struct BusMem {
    Machine &m;
    uint16_t note_at = 0;   // where a CALL inside the routine pushes its return address (0 = not used)
    uint8_t rd(uint16_t a) { return m.bus.read(a); }
    void wr(uint16_t a, uint8_t v) { m.bus.write(a, v); }
    void stack_note(uint16_t ret) { if (note_at) { m.bus.write(note_at, ret & 255); m.bus.write((uint16_t)(note_at + 1), ret >> 8); } }
};
inline game::Regs load(Machine &m) {
    game::Regs r;
    const uint16_t af = (uint16_t)m.on_get_af(), bc = (uint16_t)m.on_get_bc(), de = (uint16_t)m.on_get_de(), hl = (uint16_t)m.on_get_hl();
    r.a = af >> 8; r.f = af & 255; r.b = bc >> 8; r.c = bc & 255; r.d = de >> 8; r.e = de & 255; r.h = hl >> 8; r.l = hl & 255;
    return r;
}
inline void store(Machine &m, const game::Regs &r) {
    m.on_set_af((uint16_t)(r.a << 8 | r.f));
    m.on_set_bc((uint16_t)(r.b << 8 | r.c));
    m.on_set_de(r.de());
    m.on_set_hl(r.hl());
}
inline uint16_t stack_top(Machine &m) {
    const uint16_t sp = (uint16_t)m.on_get_sp();
    return (uint16_t)(m.bus.read(sp) | (m.bus.read((uint16_t)(sp + 1)) << 8));
}
// Leave through JP (HL) after the replaced stretch: the program counter is `pc`, WZ is `wz` (the Z80 leaves WZ alone at JP (HL)).
inline void jump_to(Machine &m, uint16_t pc, uint16_t wz, unsigned cycles, unsigned fetches) {
    continue_at(m, pc, cycles, fetches);
    m.on_set_wz(wz);
}

inline void fill_0008(Machine &m) {
    game::Regs r = load(m);
    const unsigned n = game::fill_count_0008(r.b);
    if (!fits(m, game::fill_cycles_0008(n))) {   // N17: the fill would be cut by the interrupt: do the rounds that end before it, the ROM does the rest
        game::Cost c;
        BusMem mem{m};
        const unsigned k = game::sliced::fill_cut(mem, r, Machine::cycles_per_frame - m.ticks_in_frame(), c);
        if (k == 0) return run_rom_instruction(m);
        ++sliced_cuts;
        store(m, r);
        return continue_at(m, 0x0008, c.cycles, c.fetches);
    }
    BusMem mem{m};
    game::fill_0008(mem, r);
    store(m, r);
    return_like_ret(m, game::fill_cycles_0008(n), game::fill_fetches_0008(n));
}
inline void table_byte_0010(Machine &m) {
    if (!fits(m, game::kTableByteCycles)) return run_rom_instruction(m);
    game::Regs r = load(m);
    game::Cost c;
    BusMem mem{m};
    game::table_byte_0010(mem, r, c);
    store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
// The RST 10h inside 0018 and 0020 pushes its return address on the stack; the bytes stay there (the interrupt handler's stack hash sees them), so they are written.
inline void push_leftover(Machine &m, uint16_t at, uint16_t value) {
    m.bus.write(at, value & 255);
    m.bus.write((uint16_t)(at + 1), value >> 8);
}
inline void table_word_0018(Machine &m) {
    if (!fits(m, game::kTableWordCycles)) return run_rom_instruction(m);
    push_leftover(m, (uint16_t)(m.on_get_sp() - 2), 0x001B);   // the RST 10h at 001A
    game::Regs r = load(m);
    game::Cost c;
    BusMem mem{m};
    game::table_word_0018(mem, r, c);
    store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void dispatch_0020(Machine &m) {
    if (!fits(m, game::kDispatchCycles)) return run_rom_instruction(m);
    game::Regs r = load(m);
    game::Cost c;
    BusMem mem{m};
    const uint16_t table = stack_top(m);
    push_leftover(m, (uint16_t)m.on_get_sp(), 0x0023);          // POP HL, then the RST 10h at 0022 pushes into the slot just freed
    const uint16_t target = game::dispatch_0020(mem, r, table, c);
    store(m, r);
    m.on_set_sp((uint16_t)(m.on_get_sp() + 2));
    jump_to(m, target, game::kDispatchWz, c.cycles, c.fetches);
}
inline void queue_push_0042(Machine &m) {
    if (!fits(m, 78)) return run_rom_instruction(m);
    game::Regs r = load(m);
    game::Cost c;
    BusMem mem{m};
    game::queue_push_0042(mem, r, c);
    store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void queue_inline_0028(Machine &m) {
    if (!fits(m, 59 + 78)) return run_rom_instruction(m);
    game::Regs r = load(m);
    game::Cost c;
    BusMem mem{m};
    const uint16_t sp = (uint16_t)m.on_get_sp();
    game::queue_inline_0028(mem, r, stack_top(m), c, [&](uint16_t next) { m.bus.write(sp, next & 255); m.bus.write((uint16_t)(sp + 1), next >> 8); });
    store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void timed_event_0030(Machine &m) {
    if (!fits(m, 830)) return run_rom_instruction(m);   // the longest case: a free slot only at the end, 43 * 15 + 177 = 822 T-states (no free slot: 720)
    game::Regs r = load(m);
    game::Cost c;
    BusMem mem{m};
    const game::SlotResult s = game::timed_event_0030(mem, r, stack_top(m), c);
    store(m, r);
    if (!s.found) return return_like_ret(m, c.cycles, c.fetches);
    m.on_set_sp((uint16_t)(m.on_get_sp() + 2));
    jump_to(m, s.next, game::timed_event_wz(r), c.cycles, c.fetches);
}
} // namespace helpers


// ---- N13: the screen address helpers 2000, 2018, 0065, 2052, 200F, 205A (src/game/screen.h). All are called with CALL; no scratch registers: every register
// and every stack byte the ROM leaves (the return addresses of the CALLs inside, PUSH AF / PUSH BC under them) is reproduced.
namespace screen {
inline void put_word(Machine &m, uint16_t at, uint16_t v) { m.bus.write(at, v & 255); m.bus.write((uint16_t)(at + 1), v >> 8); }
// PUSH AF then PUSH BC with the stack pointer at `sp`: A and F, then B and C go below it.
inline void push_af_bc(Machine &m, uint16_t sp, const game::Regs &r) {
    m.bus.write((uint16_t)(sp - 1), r.a);
    m.bus.write((uint16_t)(sp - 2), r.f);
    m.bus.write((uint16_t)(sp - 3), r.b);
    m.bus.write((uint16_t)(sp - 4), r.c);
}
inline void position_sum_2000(Machine &m) {
    if (!fits(m, game::kPosSumCycles)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    helpers::BusMem mem{m};
    game::position_sum_2000(mem, r, (uint16_t)m.on_get_ix(), (uint16_t)m.on_get_iy());
    helpers::store(m, r);
    return_like_ret(m, game::kPosSumCycles, game::kPosSumFetches);
}
inline void pixel_to_tile_2018(Machine &m) {
    if (!fits(m, game::kPixelToTileCycles)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    game::pixel_to_tile_2018(r);
    helpers::store(m, r);
    return_like_ret(m, game::kPixelToTileCycles, game::kPixelToTileFetches);
}
inline void screen_address_0065(Machine &m) {
    if (!fits(m, game::kScreenAddressCycles)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    const uint16_t sp = (uint16_t)m.on_get_sp();
    push_af_bc(m, sp, r);
    m.on_set_hl(game::screen_address(r.h, r.l));
    return_like_ret(m, game::kScreenAddressCycles, game::kScreenAddressFetches);
}
inline void colour_address_2052(Machine &m) {
    if (!fits(m, game::kColourAddressCycles)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    const uint16_t sp = (uint16_t)m.on_get_sp();
    put_word(m, (uint16_t)(sp - 2), 0x2055);   // the CALL 0065 inside
    push_af_bc(m, (uint16_t)(sp - 2), r);
    game::colour_address_2052(r);
    helpers::store(m, r);
    return_like_ret(m, game::kColourAddressCycles, game::kColourAddressFetches);
}
inline void tile_at_200F(Machine &m) {
    if (!fits(m, game::kTileAtCycles)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    const uint16_t sp = (uint16_t)m.on_get_sp();
    helpers::BusMem mem{m};
    // in the ROM's order: the CALL 2000 pushes 2012 (then the reads through IX and IY), the CALL 0065 pushes 2015 into the same place and PUSH AF / PUSH BC
    // go below it (A and F as 2000 left them), then the tile is read
    put_word(m, (uint16_t)(sp - 2), 0x2012);
    game::position_sum_2000(mem, r, (uint16_t)m.on_get_ix(), (uint16_t)m.on_get_iy());
    put_word(m, (uint16_t)(sp - 2), 0x2015);
    push_af_bc(m, (uint16_t)(sp - 2), r);
    r.set_hl(game::screen_address(r.h, r.l));
    r.a = mem.rd(r.hl());
    r.f = game::flag::and_a(r.a);
    helpers::store(m, r);
    return_like_ret(m, game::kTileAtCycles, game::kTileAtFetches);
}
inline void colour_test_205A(Machine &m) {
    if (!fits(m, game::kColourTestCyclesNe)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    const uint16_t sp = (uint16_t)m.on_get_sp();
    put_word(m, (uint16_t)(sp - 2), 0x205D);   // the CALL 2052
    put_word(m, (uint16_t)(sp - 4), 0x2055);   // its CALL 0065
    push_af_bc(m, (uint16_t)(sp - 4), r);
    helpers::BusMem mem{m};
    const unsigned cycles = game::colour_test_205A(mem, r);
    helpers::store(m, r);
    return_like_ret(m, cycles + 0, game::kColourTestFetches);
}
} // namespace screen


// ---- N14: the number helpers 2ACE, 2ABE, 2AAF, 2B0B, 02DF (src/game/numbers.h). Called with CALL (2ABE also by JR from 2AAF); every register and stack byte is reproduced.
namespace numbers {
inline void digit_tile_2ace(Machine &m) {
    if (!fits(m, 68)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    game::Cost c;
    helpers::BusMem mem{m};
    game::digit_tile_2ace(mem, r, c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline unsigned row_bound(uint8_t b) { return (b ? b : 256u) * (83 + 68 + 68) + 10; }
inline void digit_row_2abe(Machine &m) {
    game::Regs r = helpers::load(m);
    if (!fits(m, row_bound(r.b))) return run_rom_instruction(m);
    game::Cost c;
    helpers::BusMem mem{m, (uint16_t)(m.on_get_sp() - 2)};
    game::digit_row_2abe(mem, r, c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void score_draw_2aaf(Machine &m) {
    if (!fits(m, row_bound(3) + 54)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    game::Cost c;
    helpers::BusMem mem{m, (uint16_t)(m.on_get_sp() - 2)};
    game::score_draw_2aaf(mem, r, c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void score_pointer_2b0b(Machine &m) {
    if (!fits(m, 52)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    game::Cost c;
    helpers::BusMem mem{m};
    game::score_pointer_2b0b(mem, r, c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void coin_counter_02df(Machine &m) {
    if (!fits(m, 146)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m);
    game::Cost c;
    helpers::BusMem mem{m};
    game::coin_counter_02df(mem, r, c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
} // namespace numbers

namespace attract { inline void hook_budget(Machine &m); }   // N45k: defined further down
namespace text {
inline void print_2c5e(Machine &m) {
    game::Regs r = helpers::load(m);
    helpers::BusMem probe{m};
    const unsigned bound = game::text_bound_2c5e(probe, r);
    if (bound == 0 || !fits(m, bound)) return attract::hook_budget(m);
    game::Cost c;
    helpers::BusMem mem{m};
    uint16_t ix = 0;
    game::text_2c5e(mem, r, ix, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    m.on_set_ix(ix);
    return_like_ret(m, c.cycles, c.fetches);
}
} // namespace text

namespace status {
// Each hook: the whole routine at once when it fits into the rest of the frame (an upper bound is checked first), otherwise the ROM runs one instruction.
inline unsigned text_bound(Machine &m, uint8_t index) {
    game::Regs r = helpers::load(m);
    r.b = index;
    helpers::BusMem probe{m};
    return game::text_bound_2c5e(probe, r);
}
inline void block_2b80(Machine &m) {
    if (!fits(m, 113)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::block_2b80(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void block_2b7e(Machine &m) {
    if (!fits(m, 120)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::block_2b7e(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void block_2b8f(Machine &m) {
    if (!fits(m, 125)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::block_2b8f(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline unsigned rect_bound(Machine &m, int fixed_data = -1) {   // the data behind the CALL: rows (A) of B bytes
    helpers::BusMem probe{m};
    const uint16_t p = fixed_data >= 0 ? (uint16_t)fixed_data : game::get_word(probe, (uint16_t)m.on_get_sp());
    const unsigned b = probe.rd((uint16_t)(p + 3)), a = probe.rd((uint16_t)(p + 4));
    return 200 + (a ? a : 256u) * (60 + 26 * (b ? b : 256u));
}
inline void fill_rect_2bcd(Machine &m) {
    if (!fits(m, rect_bound(m))) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::fill_rect_2bcd(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
constexpr unsigned kLivesBound = 7 * 6 + 10 + 7 * 380;
inline void lives_2b4a(Machine &m) {
    if (!fits(m, kLivesBound)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::lives_2b4a(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void extra_life_2b33(Machine &m) {
    if (!fits(m, kLivesBound + 200)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    const uint16_t sp = (uint16_t)m.on_get_sp();
    game::extra_life_2b33(mem, r, (uint16_t)m.on_get_wz(), sp, c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void lives_area_2b6a(Machine &m) {
    if (!fits(m, rect_bound(m, 0x2B73) + kLivesBound + 200)) return run_rom_instruction(m);   // the data of the CALL at 2B70 is at 2B73
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::lives_area_2b6a(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void credits_2ba1(Machine &m) {
    const unsigned t1 = text_bound(m, 1), t2 = text_bound(m, 2);
    if (t1 == 0 || t2 == 0 || !fits(m, (t1 > t2 ? t1 : t2) + 400)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    uint16_t ix = (uint16_t)m.on_get_ix();
    game::credits_2ba1(mem, r, ix, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    m.on_set_ix(ix);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void score_area_2ae0(Machine &m) {
    const unsigned t0 = text_bound(m, 0);
    if (t0 == 0 || !fits(m, t0 + 2 * numbers::row_bound(3) + 600)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    uint16_t ix = (uint16_t)m.on_get_ix();
    game::score_area_2ae0(mem, r, ix, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    m.on_set_ix(ix);
    return_like_ret(m, c.cycles, c.fetches);
}
inline void fruit_row_2bea(Machine &m) {
    const unsigned n = [&] { const unsigned b = (m.bus.read(game::ram::level) + 1u) & 255u; return b >= 8 ? 7u : (b ? b : 256u); }();
    if (!fits(m, 500 + n * 380 + 8 * 340)) return run_rom_instruction(m);
    game::Regs r = helpers::load(m); game::Cost c; helpers::BusMem mem{m};
    game::fruit_row_2bea(mem, r, (uint16_t)m.on_get_sp(), c);
    helpers::store(m, r);
    return_like_ret(m, c.cycles, c.fetches);
}
} // namespace status

namespace clear {
// N17: the screen clears. kind: where the routine starts (setup instructions first) or the pass loop head (after a cut / a return).
struct Kind { uint8_t a; uint16_t hl; uint8_t b, c; unsigned setup_cycles, setup_fetches; uint16_t ret_pc; bool xor_a; };
inline void run(Machine &m, const Kind &k, bool with_setup) {
    const unsigned room = Machine::cycles_per_frame - m.ticks_in_frame();
    game::Regs r = helpers::load(m);
    game::Cost c;
    if (with_setup) {
        if (k.setup_cycles >= room) return run_rom_instruction(m);   // not even the three set-up instructions end before the limit
        r.a = k.a; r.set_hl(k.hl); r.b = k.b; r.c = k.c;
        if (k.xor_a) r.f = game::flag::xor_a();
        c.add(k.setup_cycles, k.setup_fetches);
    }
    helpers::BusMem mem{m};
    const uint16_t sp = (uint16_t)m.on_get_sp();
    const game::sliced::ClearResult res = game::sliced::clear_passes(mem, r, sp, k.ret_pc, room, c);
    helpers::store(m, r);
    if (res.finished) return return_like_ret(m, c.cycles, c.fetches);
    ++sliced_cuts;
    m.on_set_sp((uint16_t)(sp - 2));
    continue_at(m, 0x0008, c.cycles, c.fetches);
}
constexpr Kind k23f3{0x40, 0x4000, 0, 4, 27, 3, 0x23FC, false}, k2400{0x40, 0x4040, 0x80, 4, 27, 3, 0x2409, false}, k240d{0, 0x4400, 0, 4, 24, 3, 0x2415, true};
inline void tiles_23f3(Machine &m) { run(m, k23f3, true); }
inline void tiles_23fb(Machine &m) { run(m, k23f3, false); }
inline void lower_2400(Machine &m) { run(m, k2400, true); }
inline void lower_2408(Machine &m) { run(m, k2400, false); }
inline void colour_240d(Machine &m) { run(m, k240d, true); }
inline void colour_2414(Machine &m) { run(m, k240d, false); }
} // namespace clear

namespace attract { inline void hook_budget(Machine &m); }   // N45j: defined further down; the maze loops use it where the frame end cuts them
namespace maze {
// N18: the maze, the dots and the maze colours. `with_setup` = the hook is at the routine's first instruction, otherwise at the loop head (after a cut / a return).
inline void finish(Machine &m, bool finished, unsigned units, uint16_t head, uint16_t wz, uint16_t wz_before, const game::Cost &c) {
    if (finished) return return_like_ret(m, c.cycles, c.fetches);
    ++sliced_cuts;
    continue_at(m, head, c.cycles, c.fetches);
    m.on_set_wz(units > 0 ? wz : wz_before);
}
inline void draw_2419(Machine &m, bool with_setup) {
    const unsigned room = Machine::cycles_per_frame - m.ticks_in_frame();
    game::Regs r = helpers::load(m);
    game::Cost c;
    const uint16_t wz_before = (uint16_t)m.on_get_wz();
    if (with_setup) {
        if (20 >= room) return attract::hook_budget(m);
        r.set_hl(0x4000); r.b = 0x34; r.c = 0x35;
        c.add(20, 2);
    }
    helpers::BusMem mem{m};
    const game::LoopResult res = game::maze_loop_241f(mem, r, (uint16_t)m.on_get_sp(), room, c);
    if (!res.finished && res.units == 0 && !with_setup) return attract::hook_budget(m);
    if (!res.finished && res.units == 0 && with_setup) return attract::hook_budget(m);
    helpers::store(m, r);
    finish(m, res.finished, res.units, 0x241F, res.wz, wz_before, c);
}
inline void maze_2419(Machine &m) { draw_2419(m, true); }
inline void maze_241f(Machine &m) { draw_2419(m, false); }

inline void dots(Machine &m, bool with_setup, bool collect) {
    const unsigned room = Machine::cycles_per_frame - m.ticks_in_frame();
    game::LoopRegs l;
    l.r = helpers::load(m);
    l.ix = (uint16_t)m.on_get_ix(); l.iy = (uint16_t)m.on_get_iy();
    game::Cost c;
    const uint16_t wz_before = (uint16_t)m.on_get_wz();
    uint16_t head_wz = wz_before;
    helpers::BusMem mem{m};
    if (with_setup) {
        if ((collect ? 59 : 78) >= room) return collect ? run_rom_instruction(m) : attract::hook_budget(m);
        l.r.set_hl(0x4000); l.ix = game::kDotBits; l.iy = game::kDotOffsetTable; l.r.d = 0; l.r.b = 0x1E; l.r.c = 8;
        if (collect) c.add(59, 8);
        else { l.r.a = mem.rd(l.ix); c.add(78, 10); head_wz = l.ix; }
    }
    const game::LoopResult res = collect ? game::collect_loop_2498(mem, l, room, c) : game::dots_loop_245c(mem, l, room, c);
    if (!res.finished && res.units == 0 && !with_setup) return collect ? run_rom_instruction(m) : attract::hook_budget(m);
    helpers::store(m, l.r);
    m.on_set_ix(l.ix); m.on_set_iy(l.iy);
    finish(m, res.finished, res.units, collect ? 0x2498 : 0x245C, res.wz, head_wz, c);
}
inline void place_2448(Machine &m) { dots(m, true, false); }
inline void place_245c(Machine &m) { dots(m, false, false); }
inline void collect_2487(Machine &m) { dots(m, true, true); }
inline void collect_2498(Machine &m) { dots(m, false, true); }

// 24C9: the dot bits are all set again (30 bytes of FFh at 4E16), the 4 bytes behind them get 14h.
inline void reset_24c9(Machine &m) {
    const unsigned cycles = 10 + 7 + 7 + 11 + game::fill_cycles_0008(30) + 7 + 7 + 11 + game::fill_cycles_0008(4) + 10;
    const unsigned fetches = 4 + game::fill_fetches_0008(30) + 3 + game::fill_fetches_0008(4) + 1;
    if (!fits(m, cycles)) return run_rom_instruction(m);
    const uint16_t sp = (uint16_t)m.on_get_sp();
    game::Regs r = helpers::load(m);
    helpers::BusMem mem{m};
    game::put_word(mem, (uint16_t)(sp - 2), 0x24D1);
    for (unsigned i = 0; i < 30; ++i) mem.wr((uint16_t)(game::kDotBits + i), 0xFF);
    game::put_word(mem, (uint16_t)(sp - 2), 0x24D6);
    for (unsigned i = 0; i < 4; ++i) mem.wr((uint16_t)(game::kDotBits + 30 + i), 0x14);
    r.set_hl((uint16_t)(game::kDotBits + 34)); r.a = 0x14; r.b = 0;
    helpers::store(m, r);
    return_like_ret(m, cycles, fetches);
}

// 24D7: the colour RAM of the maze. The three set-up instructions plus the pass loop (24E7, 4 passes of 128 bytes) and the fill of the last 64 bytes natively; the long
// part for the first maze (B = 1) after it is left to the ROM.
inline void colours(Machine &m, bool with_setup) {
    const unsigned room = Machine::cycles_per_frame - m.ticks_in_frame();
    game::Regs r = helpers::load(m);
    game::Cost c;
    const uint16_t sp = (uint16_t)m.on_get_sp();
    uint8_t e = r.e;
    if (with_setup) {
        const bool z = r.b == 2;
        const unsigned setup_cycles = z ? 54 : 56, setup_fetches = z ? 7 : 8;
        if (setup_cycles >= room) return run_rom_instruction(m);
        e = r.b;
        r.e = e;
        r.f = game::flag::cp8(r.b, 2);
        r.a = z ? 0x1F : 0x10;
        r.set_hl(0x4440); r.b = 0x80; r.c = 4;
        c.add(setup_cycles, setup_fetches);
    }
    helpers::BusMem mem{m};
    const game::sliced::ClearResult res = game::sliced::clear_passes(mem, r, sp, 0x24E8, room, c, false);
    if (res.finished) {
        // 24EB ... 24F6: LD A,0Fh / LD B,40h / LD HL,47C0h / RST 08h (64 bytes) / LD A,E / CP 1 / RET NZ
        const unsigned tail = 7 + 7 + 10 + 11 + game::fill_cycles_0008(64) + 4 + 7 + (e != 1 ? 11 : 5);
        const unsigned tail_f = 3 + 1 + game::fill_fetches_0008(64) + 3;
        if (c.cycles + tail < room) {
            game::put_word(mem, (uint16_t)(sp - 2), 0x24F3);
            for (unsigned i = 0; i < 64; ++i) mem.wr((uint16_t)(0x47C0 + i), 0x0F);
            r.a = e; r.b = 0; r.set_hl(0x4800); r.f = game::flag::cp8(e, 1);
            c.add(tail, tail_f);
            helpers::store(m, r);
            if (e != 1) return return_like_ret(m, c.cycles, c.fetches);
            continue_at(m, 0x24F7, c.cycles, c.fetches);
            return m.on_set_wz(0x24F3);
        }
        helpers::store(m, r);
        continue_at(m, 0x24EB, c.cycles, c.fetches);
        return m.on_set_wz(0x24E8);   // (the WZ of the last pass's RET)
    }
    helpers::store(m, r);
    ++sliced_cuts;
    m.on_set_sp((uint16_t)(sp - 2));
    continue_at(m, 0x0008, c.cycles, c.fetches);
}
inline void colours_24d7(Machine &m) { colours(m, true); }
inline void colours_24e7(Machine &m) { colours(m, false); }
} // namespace maze

namespace scan {
// N19: the video-RAM clean-up loop (task 13). The hooks sit on every instruction of the loop (2A38 to 2A57): the frame can end between any two of them, and the ROM then
// goes on at the next instruction after the interrupt. At the loop head 2A38 whole rounds are run in one go while they end before the frame limit; every other
// instruction (and the head, when its round does not fit) is run on its own, exactly like the ROM instruction (registers, flags, WZ, R, T-states).
inline void clean(Machine &m, bool with_setup) {
    const unsigned room = Machine::cycles_per_frame - m.ticks_in_frame();
    game::Regs r = helpers::load(m);
    game::Cost c;
    if (with_setup) {
        if (10 >= room) return run_rom_instruction(m);
        r.set_de(0x4040);
        c.add(10, 1);
    }
    helpers::BusMem mem{m};
    const game::LoopResult res = game::scan_loop_2a38(mem, r, room, c);
    if (!res.finished && res.units == 0) return run_rom_instruction(m);
    helpers::store(m, r);
    maze::finish(m, res.finished, res.units, 0x2A38, res.wz, 0, c);
}
inline void clean_2a35(Machine &m) { clean(m, true); }
inline void advance(Machine &m, uint16_t pc, unsigned cycles, unsigned fetches) {
    m.on_set_pc(pc);
    const unsigned rr = (unsigned)m.on_get_r();
    m.on_set_r((rr & 0x80) | ((rr + fetches) & 0x7F));
    m.on_set_is_int_disabled(false);
    m.on_tick(cycles);
}
inline void instruction(Machine &m) {
    const uint16_t pc = (uint16_t)m.on_get_pc();
    if (pc == 0x2A38) {
        const unsigned room = Machine::cycles_per_frame - m.ticks_in_frame();
        game::Regs r = helpers::load(m);
        game::Cost c;
        helpers::BusMem mem{m};
        const game::LoopResult res = game::scan_loop_2a38(mem, r, room, c);
        if (res.finished || res.units > 0) {
            helpers::store(m, r);
            maze::finish(m, res.finished, res.units, 0x2A38, res.wz, 0, c);
            return;
        }
    }
    game::Regs r = helpers::load(m);
    const uint16_t de = r.de();
    const uint16_t sp = (uint16_t)m.on_get_sp();
    switch (pc) {
    case 0x2A38: r.set_hl(game::kScanEnd); helpers::store(m, r); return advance(m, 0x2A3B, 10, 1);
    case 0x2A3B: r.f = game::flag::and_a(r.a); helpers::store(m, r); return advance(m, 0x2A3C, 4, 1);
    case 0x2A3C: {
        uint16_t t;
        const uint16_t hl = r.hl();
        r.f = game::flag::sbc16(hl, de, r.f & game::flag::C, &t);
        r.set_hl(t);
        helpers::store(m, r);
        m.on_set_wz((uint16_t)(hl + 1));
        return advance(m, 0x2A3E, 15, 2);
    }
    case 0x2A3E:
        if (r.f & game::flag::Z) return return_like_ret(m, 11, 1);
        return advance(m, 0x2A3F, 5, 1);
    case 0x2A3F: r.a = m.bus.read(de); helpers::store(m, r); m.on_set_wz((uint16_t)(de + 1)); return advance(m, 0x2A40, 7, 1);
    case 0x2A40: case 0x2A45: case 0x2A4A: {
        const uint8_t v = pc == 0x2A40 ? 0x10 : pc == 0x2A45 ? 0x12 : 0x14;
        r.f = game::flag::cp8(r.a, v);
        helpers::store(m, r);
        return advance(m, (uint16_t)(pc + (pc == 0x2A40 ? 2 : 2)), 7, 1);
    }
    case 0x2A42: case 0x2A47: case 0x2A4C:
        m.on_set_wz(0x2A53);
        return advance(m, (r.f & game::flag::Z) ? 0x2A53 : (uint16_t)(pc + 3), 10, 1);
    case 0x2A4F: r.set_de((uint16_t)(de + 1)); helpers::store(m, r); return advance(m, 0x2A50, 6, 1);
    case 0x2A50: case 0x2A57: m.on_set_wz(0x2A38); return advance(m, 0x2A38, 10, 1);
    case 0x2A53: r.a = 0x40; helpers::store(m, r); return advance(m, 0x2A55, 7, 1);
    case 0x2A55: m.bus.write(de, r.a); m.on_set_wz((uint16_t)(((de + 1) & 0xFF) | (r.a << 8))); return advance(m, 0x2A56, 7, 1);
    case 0x2A56: r.set_de((uint16_t)(de + 1)); helpers::store(m, r); return advance(m, 0x2A57, 6, 1);
    default: return run_rom_instruction(m);
    }
    (void)sp;
}
constexpr uint16_t kLoopPcs[17] = {0x2A38, 0x2A3B, 0x2A3C, 0x2A3E, 0x2A3F, 0x2A40, 0x2A42, 0x2A45, 0x2A47, 0x2A4A, 0x2A4C, 0x2A4F, 0x2A50, 0x2A53, 0x2A55, 0x2A56, 0x2A57};
} // namespace scan

namespace sprites {
// N20: the staging of the sprite registers in the interrupt handler (ROM 009D to 018B). The handler runs at the start of a frame, so the whole block (at most about 2,900
// T-states) fits; if the machine is so late in the frame that it might not, the ROM runs its own instruction.
inline void stage_009d(Machine &m) {
    if (!fits(m, 3500)) return run_rom_instruction(m);
    game::SpriteState st;
    const game::Regs r = helpers::load(m);
    st.a = r.a; st.f = r.f; st.b = r.b; st.c = r.c; st.d = r.d; st.e = r.e; st.h = r.h; st.l = r.l;
    st.ix = (uint16_t)m.on_get_ix();
    st.wz = (uint16_t)m.on_get_wz();
    helpers::BusMem mem{m};
    const game::Cost c = game::sprite_stage_009d(mem, st);
    game::Regs o = r;
    o.a = st.a; o.f = st.f; o.b = st.b; o.c = st.c; o.d = st.d; o.e = st.e; o.h = st.h; o.l = st.l;
    helpers::store(m, o);
    m.on_set_ix(st.ix);
    continue_at(m, 0x018C, c.cycles, c.fetches);
    m.on_set_wz(st.wz);
}
} // namespace sprites

namespace pics {
// N21: the sprite pictures. One hook per block entry (see src/game/sprite_pics.h). The blocks run inside the interrupt handler at the start of a frame; if the machine is
// so late in the frame that one might not fit (only in the tests), the ROM runs its own instruction.
using Seg = game::spp::Result (*)(game::spp::Z<helpers::BusMem> &);
template <Seg F> inline void run(Machine &m) {
    if (!fits(m, 1500)) return run_rom_instruction(m);
    helpers::BusMem mem{m};
    game::spp::Z<helpers::BusMem> z(mem);
    const game::Regs r = helpers::load(m);
    z.a = r.a; z.f = r.f; z.b = r.b; z.c = r.c; z.d = r.d; z.e = r.e; z.h = r.h; z.l = r.l;
    z.ix = (uint16_t)m.on_get_ix();
    z.iy = (uint16_t)m.on_get_iy();
    z.wz = (uint16_t)m.on_get_wz();
    const uint16_t sp = (uint16_t)m.on_get_sp();
    const game::spp::Result res = F(z);
    if (res.leftover) game::put_word(mem, (uint16_t)(sp - 2), res.leftover);
    game::Regs o = r;
    o.a = z.a; o.f = z.f; o.b = z.b; o.c = z.c; o.d = z.d; o.e = z.e; o.h = z.h; o.l = z.l;
    helpers::store(m, o);
    m.on_set_ix(z.ix);
    m.on_set_iy(z.iy);
    switch (res.kind) {
    case game::spp::kRet: return return_like_ret(m, z.cost.cycles, z.cost.fetches);
    case game::spp::kContinue: continue_at(m, res.next, z.cost.cycles, z.cost.fetches); return m.on_set_wz(z.wz);
    case game::spp::kCall:
        game::put_word(mem, (uint16_t)(sp - 2), res.ret_addr);
        m.on_set_sp((uint16_t)(sp - 2));
        continue_at(m, res.next, z.cost.cycles, z.cost.fetches);
        return m.on_set_wz(z.wz);
    case game::spp::kRst20:
        game::put_word(mem, (uint16_t)(sp - 2), 0x151C);
        game::put_word(mem, (uint16_t)(sp - 4), 0x1514);
        m.on_set_sp((uint16_t)(sp - 4));
        continue_at(m, 0x0020, z.cost.cycles, z.cost.fetches);
        return m.on_set_wz(z.wz);
    }
}
inline game::spp::Result s141f(game::spp::Z<helpers::BusMem> &z) { return game::spp::positions(z, true); }
inline game::spp::Result s1490(game::spp::Z<helpers::BusMem> &z) { return game::spp::positions(z, false); }
inline game::spp::Result s14fe(game::spp::Z<helpers::BusMem> &z) { return game::spp::decide_14fe(z); }
inline game::spp::Result s151c(game::spp::Z<helpers::BusMem> &z) { return game::spp::after_mouth_151c(z); }
inline game::spp::Result s154b(game::spp::Z<helpers::BusMem> &z) { return game::spp::ghosts_154b(z); }
inline game::spp::Result s15b4(game::spp::Z<helpers::BusMem> &z) { return game::spp::call_to(z, 0x15E6, 0x15B7); }
inline game::spp::Result s15b7(game::spp::Z<helpers::BusMem> &z) { return game::spp::call_to(z, 0x162D, 0x15BA); }
inline game::spp::Result s15ba(game::spp::Z<helpers::BusMem> &z) { return game::spp::call_to(z, 0x1652, 0x15BD); }
inline game::spp::Result s15bd(game::spp::Z<helpers::BusMem> &z) { return game::spp::tail_15bd(z); }
inline game::spp::Result s15e6(game::spp::Z<helpers::BusMem> &z) { return game::spp::fruit_15e6(z); }
inline game::spp::Result s162d(game::spp::Z<helpers::BusMem> &z) { return game::spp::score_162d(z); }
inline game::spp::Result s1652(game::spp::Z<helpers::BusMem> &z) { return game::spp::death_1652(z); }
inline game::spp::Result s168c(game::spp::Z<helpers::BusMem> &z) { return game::spp::mouth_168c(z); }
inline game::spp::Result s16b1(game::spp::Z<helpers::BusMem> &z) { return game::spp::mouth_16b1(z); }
inline game::spp::Result s16d6(game::spp::Z<helpers::BusMem> &z) { return game::spp::mouth_16d6(z); }
inline game::spp::Result s16f7(game::spp::Z<helpers::BusMem> &z) { return game::spp::mouth_16f7(z); }
inline game::spp::Result s01dc(game::spp::Z<helpers::BusMem> &z) { return game::clk::counters_01dc(z); }
inline game::spp::Result s02fd(game::spp::Z<helpers::BusMem> &z) { return game::clk::blink_02fd(z); }
inline game::spp::Result s0267(game::spp::Z<helpers::BusMem> &z) { return game::coin::coins_0267(z); }
inline game::spp::Result s02ad(game::spp::Z<helpers::BusMem> &z) { return game::coin::pulse_02ad(z); }
inline game::spp::Result s0369(game::spp::Z<helpers::BusMem> &z) { return game::lab::label_writer(z, 0); }
inline game::spp::Result s0376(game::spp::Z<helpers::BusMem> &z) { return game::lab::label_writer(z, 1); }
inline game::spp::Result s0383(game::spp::Z<helpers::BusMem> &z) { return game::lab::label_writer(z, 2); }
inline game::spp::Result s0390(game::spp::Z<helpers::BusMem> &z) { return game::lab::label_writer(z, 3); }
inline game::spp::Result s039d(game::spp::Z<helpers::BusMem> &z) { return game::lab::bonus_039d(z); }
inline void install(Machine::RuleHook *t) {
    t[0x0369] = run<s0369>; t[0x0376] = run<s0376>; t[0x0383] = run<s0383>; t[0x0390] = run<s0390>; t[0x039D] = run<s039d>;
    t[0x01DC] = run<s01dc>; t[0x02FD] = run<s02fd>; t[0x0267] = run<s0267>; t[0x02AD] = run<s02ad>;
    t[0x141F] = run<s141f>; t[0x1490] = run<s1490>; t[0x14FE] = run<s14fe>; t[0x151C] = run<s151c>; t[0x154B] = run<s154b>;
    t[0x15B4] = run<s15b4>; t[0x15B7] = run<s15b7>; t[0x15BA] = run<s15ba>; t[0x15BD] = run<s15bd>;
    t[0x15E6] = run<s15e6>; t[0x162D] = run<s162d>; t[0x1652] = run<s1652>;
    t[0x168C] = run<s168c>; t[0x16B1] = run<s16b1>; t[0x16D6] = run<s16d6>; t[0x16F7] = run<s16f7>;
}
constexpr uint16_t kEntries[16] = {0x141F, 0x1490, 0x14FE, 0x151C, 0x154B, 0x15B4, 0x15B7, 0x15BA, 0x15BD, 0x15E6, 0x162D, 0x1652, 0x168C, 0x16B1, 0x16D6, 0x16F7};
} // namespace pics

namespace hnd {
// N25: the rest of the interrupt handler's own code (see src/game/handler.h). The hooks continue at the next ROM address. When the machine is so late in the frame that a block
// might not fit (only in the tests) the ROM runs its own instruction.
using Seg = game::hnd::Out (*)(game::spp::Z<helpers::BusMem> &, uint16_t &);
template <Seg F, unsigned Fit = 400> inline void run(Machine &m) {
    if (!fits(m, Fit)) return run_rom_instruction(m);
    helpers::BusMem mem{m};
    game::spp::Z<helpers::BusMem> z(mem);
    const game::Regs r = helpers::load(m);
    z.a = r.a; z.f = r.f; z.b = r.b; z.c = r.c; z.d = r.d; z.e = r.e; z.h = r.h; z.l = r.l;
    z.ix = (uint16_t)m.on_get_ix();
    z.iy = (uint16_t)m.on_get_iy();
    z.wz = (uint16_t)m.on_get_wz();
    uint16_t sp = (uint16_t)m.on_get_sp();
    const game::hnd::Out o = F(z, sp);
    game::Regs q = r;
    q.a = z.a; q.f = z.f; q.b = z.b; q.c = z.c; q.d = z.d; q.e = z.e; q.h = z.h; q.l = z.l;
    helpers::store(m, q);
    m.on_set_ix(z.ix);
    m.on_set_iy(z.iy);
    m.on_set_sp(sp);
    if (o.iff >= 0) { m.on_set_iff1(o.iff != 0); m.on_set_iff2(o.iff != 0); }
    continue_at(m, o.next, z.cost.cycles, z.cost.fetches);
    m.on_set_wz(z.wz);
}
template <uint16_t Target, uint16_t Ret> inline game::hnd::Out call_seg(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::hnd::call_site(z, sp, Target, Ret); }
inline game::hnd::Out h008d(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::hnd::entry_008d(z, sp); }
inline game::hnd::Out h0195(game::spp::Z<helpers::BusMem> &z, uint16_t &) { return game::hnd::check_0195(z); }
inline game::hnd::Out h01ad(game::spp::Z<helpers::BusMem> &z, uint16_t &) { return game::hnd::state1_01ad(z); }
inline game::hnd::Out h01bf(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::hnd::exit_01bf(z, sp); }
inline game::hnd::Out h01d4(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::hnd::exit_01d4(z, sp); }
inline void install(Machine::RuleHook *t) {
    t[0x008D] = run<h008d>; t[0x0195] = run<h0195>; t[0x01AD] = run<h01ad>; t[0x01BF] = run<h01bf>; t[0x01D4] = run<h01d4>;
    t[0x018C] = run<call_seg<0x01DC, 0x018F>>; t[0x018F] = run<call_seg<0x0221, 0x0192>>; t[0x0192] = run<call_seg<0x03C8, 0x0195>>;
    t[0x019B] = run<call_seg<0x039D, 0x019E>>; t[0x019E] = run<call_seg<0x1490, 0x01A1>>; t[0x01A1] = run<call_seg<0x141F, 0x01A4>>;
    t[0x01A4] = run<call_seg<0x0267, 0x01A7>>; t[0x01A7] = run<call_seg<0x02AD, 0x01AA>>; t[0x01AA] = run<call_seg<0x02FD, 0x01AD>>;
    t[0x01B9] = run<call_seg<0x2D0C, 0x01BC>>; t[0x01BC] = run<call_seg<0x2CC1, 0x01BF>>;
}
constexpr uint16_t kEntries[16] = {0x008D, 0x018C, 0x018F, 0x0192, 0x0195, 0x019B, 0x019E, 0x01A1, 0x01A4, 0x01A7, 0x01AA, 0x01AD, 0x01B9, 0x01BC, 0x01BF, 0x01D4};
} // namespace hnd

namespace evt {
// N26: the timed events (see src/game/events.h). The loop hooks (0221, and 025B where a handler comes back) continue at the next ROM address; the small handlers return like a RET.
inline game::hnd::Out e0221(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::evt::slots_loop(z, sp, false); }
inline game::hnd::Out e025b(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::evt::slots_loop(z, sp, true); }
template <uint16_t Addr> inline game::spp::Result inc(game::spp::Z<helpers::BusMem> &z) { return game::evt::inc_byte<Addr>(z); }
inline game::spp::Result c1000(game::spp::Z<helpers::BusMem> &z) { return game::evt::clear_1000(z); }
inline game::spp::Result c1004(game::spp::Z<helpers::BusMem> &z) { return game::evt::clear_1004(z); }
template <uint16_t State, uint16_t Table> inline game::hnd::Out dispatch(game::spp::Z<helpers::BusMem> &z, uint16_t &sp) { return game::hnd::mode_dispatch(z, sp, State, Table); }
inline void install(Machine::RuleHook *t) {
    t[0x03C8] = hnd::run<dispatch<game::ram::main_state, 0x03CC>>; t[0x03D4] = hnd::run<dispatch<game::ram::reset_state, 0x03D8>>;
    t[0x05E5] = hnd::run<dispatch<game::ram::credit_ready_state, 0x05E9>>; t[0x06BE] = hnd::run<dispatch<game::ram::level_state, 0x06C2>>;
    t[0x0221] = hnd::run<e0221, 2500>; t[0x025B] = hnd::run<e025b, 2500>;
    t[0x0894] = pics::run<inc<game::ram::level_state>>; t[0x06A3] = pics::run<inc<game::ram::credit_ready_state>>; t[0x058E] = pics::run<inc<game::ram::intro_state>>; t[0x1272] = pics::run<inc<game::ram::byte_4DD1>>;
    t[0x212B] = pics::run<inc<game::ram::byte_4E06>>; t[0x21F0] = pics::run<inc<game::ram::byte_4E07>>; t[0x22B9] = pics::run<inc<game::ram::byte_4E08>>;
    t[0x1000] = pics::run<c1000>; t[0x1004] = pics::run<c1004>;
}
constexpr uint16_t kEntries[11] = {0x0221, 0x025B, 0x0894, 0x06A3, 0x058E, 0x1272, 0x1000, 0x1004, 0x212B, 0x21F0, 0x22B9};
} // namespace evt

namespace attract {
// N28: the attract-mode introduction (see src/game/attract.h). One hook function serves every start address; it reads the address from the machine.
inline void hook(Machine &m) {
    if (!fits(m, 2500)) return run_rom_instruction(m);
    helpers::BusMem mem{m};
    game::spp::Z<helpers::BusMem> z(mem);
    const game::Regs r = helpers::load(m);
    z.a = r.a; z.f = r.f; z.b = r.b; z.c = r.c; z.d = r.d; z.e = r.e; z.h = r.h; z.l = r.l;
    z.ix = (uint16_t)m.on_get_ix();
    z.iy = (uint16_t)m.on_get_iy();
    z.wz = (uint16_t)m.on_get_wz();
    uint16_t sp = (uint16_t)m.on_get_sp();
    const game::hnd::Out o = game::att::run(z, sp, (uint16_t)m.on_get_pc());
    game::Regs q = r;
    q.a = z.a; q.f = z.f; q.b = z.b; q.c = z.c; q.d = z.d; q.e = z.e; q.h = z.h; q.l = z.l;
    helpers::store(m, q);
    m.on_set_ix(z.ix);
    m.on_set_iy(z.iy);
    m.on_set_sp(sp);
    continue_at(m, o.next, z.cost.cycles, z.cost.fetches);
    m.on_set_wz(z.wz);
}
// N45j: the same runner for the maze-drawing and dot-placing loops, which the frame end cuts in the middle: no fits() test; the run executes the ROM's instructions natively up to the
// first instruction that starts at or after the end of the frame and leaves the rest to the ROM (exactly where the ROM is cut by the interrupt).
inline void hook_budget(Machine &m) {
    if (m.on_is_halted()) { m.on_step(); return; }   // N47: a halted CPU takes one idle step of 4 cycles per call, as in the ROM run (the hook at the address behind HALT must not run the code)
    helpers::BusMem mem{m};
    game::spp::Z<helpers::BusMem> z(mem);
    const game::Regs r = helpers::load(m);
    z.a = r.a; z.f = r.f; z.b = r.b; z.c = r.c; z.d = r.d; z.e = r.e; z.h = r.h; z.l = r.l;
    z.ix = (uint16_t)m.on_get_ix();
    z.iy = (uint16_t)m.on_get_iy();
    z.wz = (uint16_t)m.on_get_wz();
    uint16_t sp = (uint16_t)m.on_get_sp();
    z.abc = (uint16_t)m.get_alt_bc(); z.ade = (uint16_t)m.get_alt_de(); z.ahl = (uint16_t)m.get_alt_hl();   // N47a
    const unsigned t0 = m.ticks_in_frame();
    const unsigned room = t0 < Machine::cycles_per_frame ? Machine::cycles_per_frame - t0 : 0xFFFFFFFFu;   // (past the end of the frame only in the tests, which do not deliver interrupts)
    const game::hnd::Out o = game::att::run(z, sp, (uint16_t)m.on_get_pc(), room);
    game::Regs q = r;
    q.a = z.a; q.f = z.f; q.b = z.b; q.c = z.c; q.d = z.d; q.e = z.e; q.h = z.h; q.l = z.l;
    helpers::store(m, q);
    m.on_set_ix(z.ix);
    m.on_set_iy(z.iy);
    m.on_set_sp(sp);
    continue_at(m, o.next, z.cost.cycles, z.cost.fetches);
    m.on_set_wz(z.wz);
    m.set_alt_bc(z.abc); m.set_alt_de(z.ade); m.set_alt_hl(z.ahl);   // N47a
    if (o.iff >= 0) { m.on_set_iff1(o.iff != 0); m.on_set_iff2(o.iff != 0); }   // N47: DI / EI
    if (o.im >= 0) m.on_set_int_mode((unsigned)o.im);
    if (o.ireg >= 0) m.on_set_i((uint8_t)o.ireg);
    if (o.port0 >= 0) m.on_output(0, (uint8_t)o.port0);
    if (o.halted) m.on_set_is_halted(true);
    if (o.ei_last) m.on_set_is_int_disabled(true);
    if (o.prefix) { m.set_prefix(o.prefix == 1 ? Machine::Prefix::ix : Machine::Prefix::iy); m.on_set_is_int_disabled(true); }   // N45j: stopped between the prefix byte and the opcode, as the core does
}
// N48: the frame end stopped the core between the prefix byte (DD / FD) and the opcode of an indexed instruction (the core takes the prefix as a step of its own). This hook is on the address
// right behind that prefix byte: it finishes the instruction natively (the instruction list holds it at the address one byte lower) and leaves out the prefix's 4 cycles and its one
// refresh increment, which the core already counted.
inline void hook_prefix(Machine &m) {
    const uint16_t pc = (uint16_t)m.on_get_pc();
    const Machine::Prefix kind = m.get_prefix();
    if ((kind != Machine::Prefix::ix && kind != Machine::Prefix::iy) || !game::att::find((uint16_t)(pc - 1))) { run_rom_instruction(m); return; }
    m.set_prefix(Machine::Prefix::hl);
    helpers::BusMem mem{m};
    game::spp::Z<helpers::BusMem> z(mem);
    const game::Regs r = helpers::load(m);
    z.a = r.a; z.f = r.f; z.b = r.b; z.c = r.c; z.d = r.d; z.e = r.e; z.h = r.h; z.l = r.l;
    z.ix = (uint16_t)m.on_get_ix();
    z.iy = (uint16_t)m.on_get_iy();
    z.wz = (uint16_t)m.on_get_wz();
    uint16_t sp = (uint16_t)m.on_get_sp();
    const game::hnd::Out o = game::att::run(z, sp, (uint16_t)(pc - 1), 1, true);
    game::Regs q = r;
    q.a = z.a; q.f = z.f; q.b = z.b; q.c = z.c; q.d = z.d; q.e = z.e; q.h = z.h; q.l = z.l;
    helpers::store(m, q);
    m.on_set_ix(z.ix);
    m.on_set_iy(z.iy);
    m.on_set_sp(sp);
    continue_at(m, o.next, z.cost.cycles - 4, z.cost.fetches - 1);
    m.on_set_wz(z.wz);
}
// N45j: a frame that ends inside one of these loops is resumed in the middle of an instruction sequence; every instruction address of the N45j code is a place to resume (the frame end can cut anywhere).
inline void install_resume(Machine::RuleHook *t) {
    for (uint16_t pc = 0x23ED; pc < 0x2487; ++pc) if ((pc < 0x23EF || pc >= 0x2415) && game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x26D0; pc < 0x2728; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;   // N45k
    for (uint16_t pc = 0x2C5E; pc < 0x2CC1; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    // N46: the main loop, the fill and table routines and the places after the RST 8 / CALL that the frame end can cut
    for (uint16_t pc = 0x0008; pc < 0x000C; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x0018; pc < 0x0020; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x1291; pc < 0x1295; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x238D; pc < 0x23A8; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x3000; pc < 0x3154; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;   // N47a: the self test
    for (uint16_t pc = 0x3174; pc < 0x32F9; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;   // N47b: the service-switch test screen
    for (uint16_t pc = 0x3AF4; pc < 0x3B08; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc : {0x0010, 0x0011, 0x0012, 0x0014, 0x0015, 0x0016, 0x0017}) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;   // N48: the rst 10h table routine and the routines the old hooks leave to the ROM
    for (uint16_t pc = 0x1AD9; pc < 0x1AF8; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x276C; pc < 0x27F1; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x2B7E; pc < 0x2C44; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (uint16_t pc = 0x230B; pc < 0x238D; ++pc) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;   // N47: the boot
    for (uint16_t pc : {0x24E8, 0x2A86}) if (game::att::find(pc) && !t[pc]) t[pc] = hook_budget;
    for (int i = 0; i < game::att::kPrefixMidN; ++i) if (!t[game::att::kPrefixMid[i]]) t[game::att::kPrefixMid[i]] = hook_prefix;   // N48
    for (uint32_t pc = 0; pc < 0x4000; ++pc) if (game::att::find((uint16_t)pc) && !t[pc]) t[pc] = hook_budget;   // N49c: every instruction of the program runs natively (the program has no ROM to fall back to)
    for (uint16_t pc : {0x000C, 0x24E8}) if (t[pc]) t[pc] = hook_budget;   // N46: these two were reached with the fits test of the older hooks; at a frame end they fell back to the ROM
}
inline void install(Machine::RuleHook *t) { for (int i = 0; i < game::att::kStartsN; ++i) t[game::att::kStarts[i]] = hook; for (int i = 0; i < game::att::kStartsDemoN; ++i) t[game::att::kStartsDemo[i]] = hook; for (int i = 0; i < game::att::kStartsGameN; ++i) t[game::att::kStartsGame[i]] = hook; for (int i = 0; i < game::att::kStartsLevelN; ++i) t[game::att::kStartsLevel[i]] = hook; for (int i = 0; i < game::att::kStartsFrameN; ++i) t[game::att::kStartsFrame[i]] = hook; for (int i = 0; i < game::att::kStartsMeetN; ++i) t[game::att::kStartsMeet[i]] = hook; for (int i = 0; i < game::att::kStartsEatenN; ++i) t[game::att::kStartsEaten[i]] = hook; for (int i = 0; i < game::att::kStartsDeathN; ++i) t[game::att::kStartsDeath[i]] = hook; for (int i = 0; i < game::att::kStartsClearN; ++i) t[game::att::kStartsClear[i]] = hook; for (int i = 0; i < game::att::kStartsFruitN; ++i) t[game::att::kStartsFruit[i]] = hook; for (int i = 0; i < game::att::kStartsOverN; ++i) t[game::att::kStartsOver[i]] = hook; for (int i = 0; i < game::att::kStartsInter1N; ++i) t[game::att::kStartsInter1[i]] = hook; for (int i = 0; i < game::att::kStartsInter2N; ++i) t[game::att::kStartsInter2[i]] = hook; for (int i = 0; i < game::att::kStartsInter3N; ++i) t[game::att::kStartsInter3[i]] = hook; for (int i = 0; i < game::att::kStartsSndN; ++i) t[game::att::kStartsSnd[i]] = hook; for (int i = 0; i < game::att::kStartsTuneN; ++i) t[game::att::kStartsTune[i]] = hook; for (int i = 0; i < game::att::kStartsSchedN; ++i) t[game::att::kStartsSched[i]] = hook; for (int i = 0; i < game::att::kStarts45aN; ++i) t[game::att::kStarts45a[i]] = hook; for (int i = 0; i < game::att::kStarts45bN; ++i) t[game::att::kStarts45b[i]] = hook; for (int i = 0; i < game::att::kStarts45cN; ++i) t[game::att::kStarts45c[i]] = hook; for (int i = 0; i < game::att::kStarts45dN; ++i) t[game::att::kStarts45d[i]] = hook; for (int i = 0; i < game::att::kStarts45eN; ++i) t[game::att::kStarts45e[i]] = hook; for (int i = 0; i < game::att::kStarts45fN; ++i) t[game::att::kStarts45f[i]] = hook; for (int i = 0; i < game::att::kStarts45gN; ++i) t[game::att::kStarts45g[i]] = hook; for (int i = 0; i < game::att::kStarts45hN; ++i) t[game::att::kStarts45h[i]] = hook; for (int i = 0; i < game::att::kStarts45iN; ++i) t[game::att::kStarts45i[i]] = hook; for (int i = 0; i < game::att::kStarts45jN; ++i) t[game::att::kStarts45j[i]] = hook_budget; for (int i = 0; i < game::att::kStarts45kN; ++i) t[game::att::kStarts45k[i]] = hook_budget; for (int i = 0; i < game::att::kStarts46N; ++i) t[game::att::kStarts46[i]] = hook_budget; for (int i = 0; i < game::att::kStarts47N; ++i) t[game::att::kStarts47[i]] = hook_budget; for (int i = 0; i < game::att::kStarts47aN; ++i) t[game::att::kStarts47a[i]] = hook_budget; for (int i = 0; i < game::att::kStarts47bN; ++i) t[game::att::kStarts47b[i]] = hook_budget; install_resume(t); }   // N28 to N47a
} // namespace attract

// The table Machine::rules points to. One entry per ROM routine that has a readable version in use.
inline const Machine::RuleHook *table() {
    static Machine::RuleHook t[0x10000] = {};
    static bool made = false;
    if (!made) {
        t[0x2A23] = random_byte_2A23;
        t[0x0008] = helpers::fill_0008;
        t[0x0010] = helpers::table_byte_0010;
        t[0x0018] = helpers::table_word_0018;
        t[0x0020] = helpers::dispatch_0020;
        t[0x0028] = helpers::queue_inline_0028;
        t[0x0030] = helpers::timed_event_0030;
        t[0x0042] = helpers::queue_push_0042;
        t[0x2000] = screen::position_sum_2000;
        t[0x2018] = screen::pixel_to_tile_2018;
        t[0x0065] = screen::screen_address_0065;
        t[0x2052] = screen::colour_address_2052;
        t[0x200F] = screen::tile_at_200F;
        t[0x205A] = screen::colour_test_205A;
        t[0x2ACE] = numbers::digit_tile_2ace;
        t[0x2ABE] = numbers::digit_row_2abe;
        t[0x2AAF] = numbers::score_draw_2aaf;
        t[0x2B0B] = numbers::score_pointer_2b0b;
        t[0x02DF] = numbers::coin_counter_02df;
        t[0x2C5E] = text::print_2c5e;
        t[0x2B80] = status::block_2b80; t[0x2B7E] = status::block_2b7e; t[0x2B8F] = status::block_2b8f; t[0x2BCD] = status::fill_rect_2bcd;
        t[0x2B4A] = status::lives_2b4a; t[0x2B33] = status::extra_life_2b33; t[0x2B6A] = status::lives_area_2b6a;
        t[0x23F3] = clear::tiles_23f3; t[0x23FB] = clear::tiles_23fb; t[0x2400] = clear::lower_2400; t[0x2408] = clear::lower_2408; t[0x240D] = clear::colour_240d; t[0x2414] = clear::colour_2414;
        t[0x2419] = maze::maze_2419; t[0x241F] = maze::maze_241f; t[0x2448] = maze::place_2448; t[0x245C] = maze::place_245c;
        t[0x2487] = maze::collect_2487; t[0x2498] = maze::collect_2498; t[0x24C9] = maze::reset_24c9; t[0x24D7] = maze::colours_24d7; t[0x24E7] = maze::colours_24e7;
        t[0x2A35] = scan::clean_2a35; for (uint16_t pc : scan::kLoopPcs) t[pc] = scan::instruction;
        t[0x009D] = sprites::stage_009d;
        pics::install(t);
        hnd::install(t);
        evt::install(t);
        attract::install(t);
        t[0x2BA1] = status::credits_2ba1; t[0x2AE0] = status::score_area_2ae0; t[0x2BEA] = status::fruit_row_2bea;
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

// N49b: the table the machine runs. With the Z80 core (the tests) the original rules are "no table": the ROM runs. Without it (the window program) there is no ROM to run:
// the original rules are the same table with the standard tuning (proved identical to the ROM routines frame by frame, tests/test_native_machine.sh).
inline const Machine::RuleHook *table_for(bool readable) {
#ifdef PAC_NO_Z80
    (void)readable;
    return table();
#else
    return readable ? table() : nullptr;
#endif
}

} // namespace rules
