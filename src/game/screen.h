// N13: the screen address helpers, written natively (no Z80). ROM routines replaced (entries):
//   2000  add two position pairs:  L = (IY+0) + (IX+0), H = (IY+1) + (IX+1)
//   2018  pixel position to tile position:  L = L / 8 + 20h, H = H / 8 + 1Eh
//   0065  tile position to video RAM address (JP 202D):  HL = 4040h + (L - 20h) + 32 * (H - 20h)
//   2052  the same, plus 0400h (the matching colour RAM address)
//   200F  2000, then 0065, then A = the tile code at that address
//   205A  2052, then BC^ = 1 when the colour RAM byte there is 1Bh, else 0
// Each function takes the register values the ROM routine starts with and returns the registers and the exact cost (T-states and opcode fetches) of the ROM
// routine. The stack bytes the ROM leaves behind (return addresses of the CALLs inside, PUSH AF/BC) are written by the adapters in src/rules/rules.h
// (namespace screen). The proof is tests/test_native_screen.cpp. No ROM bytes anywhere.
#pragma once
#include <cstdint>
#include "helpers.h"

namespace game {
namespace flag {
inline uint8_t cp8(uint8_t a, uint8_t v) { // CP v
    const unsigned r = (unsigned)a - v;
    uint8_t f = N;
    if (r & 0x80) f |= S;
    if ((r & 0xFF) == 0) f |= Z;
    if ((a & 0x0F) < (v & 0x0F)) f |= H;
    if (((a ^ v) & (a ^ r) & 0x80) != 0) f |= PV;
    if (a < v) f |= C;
    f |= (uint8_t)(v & XY);
    return f;
}
inline uint8_t xor_a() { return (uint8_t)(Z | PV); } // XOR A
inline uint8_t add16(uint16_t hl, uint16_t de, uint8_t old_f, uint16_t *result) { // ADD HL,DE: S, Z, P/V keep their values
    const unsigned r = (unsigned)hl + de;
    uint8_t f = (uint8_t)(old_f & (S | Z | PV));
    f |= (uint8_t)((r >> 8) & XY);
    if (((hl & 0x0FFF) + (de & 0x0FFF)) > 0x0FFF) f |= H;
    if (r > 0xFFFF) f |= C;
    *result = (uint16_t)r;
    return f;
}
} // namespace flag

// ---- 2000: 94 T-states, 11 fetches. Out: HL, A = H, F = the flags of the second ADD.
constexpr unsigned kPosSumCycles = 94, kPosSumFetches = 11;
template <class Mem> inline void position_sum_2000(Mem &mem, Regs &r, uint16_t ix, uint16_t iy) {
    uint8_t l, h;
    flag::add8(mem.rd(iy), mem.rd(ix), 0, &l);
    r.f = flag::add8(mem.rd((uint16_t)(iy + 1)), mem.rd((uint16_t)(ix + 1)), 0, &h);
    r.l = l;
    r.h = h;
    r.a = h;
}

// ---- 2018: 88 T-states, 19 fetches (each SRL is a two-byte instruction). Out: HL, A = H, F = the flags of ADD A,1Eh.
constexpr unsigned kPixelToTileCycles = 88, kPixelToTileFetches = 19;
inline void pixel_to_tile_2018(Regs &r) {
    const uint8_t l = (uint8_t)((r.l >> 3) + 0x20);
    uint8_t h;
    r.f = flag::add8((uint8_t)(r.h >> 3), 0x1E, 0, &h);
    r.l = l;
    r.h = h;
    r.a = h;
}

// ---- 0065 (JP 202D): 198 T-states, 32 fetches (the JP, PUSH AF/BC, the arithmetic, POP BC/AF, RET). A, F, BC come back unchanged; only HL changes.
constexpr unsigned kScreenAddressCycles = 198, kScreenAddressFetches = 32;
inline uint16_t screen_address(uint8_t h, uint8_t l) {
    // the ROM's own arithmetic, step by step (the shifts lose the top bits of H exactly as the ROM's do)
    const uint8_t l1 = (uint8_t)(l - 0x20);
    unsigned b = 0;
    uint8_t hh = (uint8_t)(h - 0x20);
    unsigned carry = 0;
    for (int i = 0; i < 4; ++i) { carry = (hh >> 7) & 1; hh = (uint8_t)(hh << 1); }
    b = carry;                                      // RL B (B was 0)
    carry = (hh >> 7) & 1; hh = (uint8_t)(hh << 1); // SLA H
    b = ((b << 1) | carry) & 0xFF;                  // RL B
    const uint16_t bc = (uint16_t)(b << 8 | hh);    // LD C,H
    return (uint16_t)(l1 + bc + 0x4040);            // LD H,0 / ADD HL,BC / LD BC,4040 / ADD HL,BC
}

// ---- 2052: CALL 0065 / LD DE,0400 / ADD HL,DE / RET: 246 T-states, 36 fetches. Out: HL, DE = 0400, F = the flags of ADD HL,DE (S, Z, P/V from F before).
constexpr unsigned kColourAddressCycles = 246, kColourAddressFetches = 36;
inline void colour_address_2052(Regs &r) {
    uint16_t hl;
    r.f = flag::add16(screen_address(r.h, r.l), 0x0400, r.f, &hl);
    r.set_hl(hl);
    r.set_de(0x0400);
}

// ---- 200F: CALL 2000 / CALL 0065 / LD A,(HL) / AND A / RET: 17 + 94 + 17 + 198 + 7 + 4 + 10 = 347 T-states, 48 fetches. Out: HL, A = the tile code, F = flags of AND A.
constexpr unsigned kTileAtCycles = 347, kTileAtFetches = 48;
template <class Mem> inline void tile_at_200F(Mem &mem, Regs &r, uint16_t ix, uint16_t iy) {
    position_sum_2000(mem, r, ix, iy);
    r.set_hl(screen_address(r.h, r.l));
    r.a = mem.rd(r.hl());
    r.f = flag::and_a(r.a);
}

// ---- 205A: CALL 2052 / LD A,(HL) / CP 1Bh / JR NZ / ... : the byte (BC) = 1 when the colour RAM byte is 1Bh, else 0.
// 310 T-states when the byte is not 1Bh, 308 when it is; 43 fetches. Out: HL, DE = 0400, A, F as the ROM leaves them.
constexpr unsigned kColourTestCyclesNe = 310, kColourTestCyclesEq = 308, kColourTestFetches = 43;
constexpr uint8_t kColourTestValue = 0x1B;
template <class Mem> inline unsigned colour_test_205A(Mem &mem, Regs &r) {
    colour_address_2052(r);
    r.a = mem.rd(r.hl());
    r.f = flag::cp8(r.a, kColourTestValue);
    const bool equal = r.a == kColourTestValue;
    if (equal) r.a = 1; else { r.a = 0; r.f = flag::xor_a(); }
    mem.wr((uint16_t)(r.b << 8 | r.c), r.a);
    return equal ? kColourTestCyclesEq : kColourTestCyclesNe;
}

} // namespace game
