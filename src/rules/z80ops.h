// T02c: small helpers for hooks that must leave the Z80 registers, flags, timing and stack leftovers exactly as the ROM routine does.
// The interrupt handler pushes the CPU registers onto the stack, so stale register values end up in the work RAM; the game never reads them again,
// but the frame hashes of the tests include them. A hook therefore (1) runs the readable rule, (2) adds up the exact time of the ROM instructions it
// replaces with Cost::t(cycles, fetches), and (3) sets the registers and flags the ROM leaves behind with the flag functions below.
#pragma once
#include <cstdint>

namespace rules {
namespace z80 {

// Time of a stretch of ROM code: Z80 T-states and opcode fetches (the refresh register R grows by one per fetch; prefixes count as a fetch each).
struct Cost {
    unsigned cycles = 0, fetches = 0;
    void t(unsigned c, unsigned f = 1) { cycles += c; fetches += f; }
};

inline bool even_parity(uint8_t v) {
    v ^= v >> 4;
    v ^= v >> 2;
    v ^= v >> 1;
    return (v & 1) == 0;
}
constexpr uint8_t kS = 0x80, kZ = 0x40, kH = 0x10, kPV = 0x04, kN = 0x02, kC = 0x01, kXY = 0x28;

// INC r / INC (hl) on the value v; the carry flag keeps its old value (`old_f`).
inline uint8_t f_inc8(uint8_t v, uint8_t old_f) {
    const uint8_t r = (uint8_t)(v + 1);
    uint8_t f = (uint8_t)(old_f & kC);
    f |= (uint8_t)(r & (kS | kXY));
    if (r == 0) f |= kZ;
    if ((v & 0x0F) == 0x0F) f |= kH;
    if (v == 0x7F) f |= kPV;
    return f;
}
// AND with the result r (AND a, AND n): H is set, carry cleared.
inline uint8_t f_and(uint8_t r) {
    uint8_t f = (uint8_t)(kH | (r & (kS | kXY)));
    if (r == 0) f |= kZ;
    if (even_parity(r)) f |= kPV;
    return f;
}
// SRL r on v (result v >> 1).
inline uint8_t f_srl(uint8_t v) {
    const uint8_t r = (uint8_t)(v >> 1);
    uint8_t f = (uint8_t)((r & kXY) | (v & kC));
    if (r == 0) f |= kZ;
    if (even_parity(r)) f |= kPV;
    return f;
}
// RRCA on a: returns the new A; f is updated (S, Z, P/V stay, H and N cleared, C = the bit rotated out).
inline uint8_t rrca(uint8_t a, uint8_t &f) {
    const uint8_t r = (uint8_t)((a >> 1) | (a << 7));
    f = (uint8_t)((f & (kS | kZ | kPV)) | (r & kXY) | (a & kC));
    return r;
}
// XOR a (a = 0): the flags it leaves.
constexpr uint8_t kFlagsAfterXorA = kZ | kPV;
// The flags of CP a,v.
inline uint8_t f_cp(uint8_t a, uint8_t v) {
    const unsigned r = (unsigned)a - v;
    uint8_t f = kN;
    if (r & 0x80) f |= kS;
    if ((r & 0xFF) == 0) f |= kZ;
    if ((a & 0x0F) < (v & 0x0F)) f |= kH;
    if (((a ^ v) & (a ^ r) & 0x80) != 0) f |= kPV;
    if (a < v) f |= kC;
    f |= (uint8_t)(v & kXY);
    return f;
}

// ADD a,b (no carry in): the flags.
inline uint8_t f_add8(uint8_t a, uint8_t b) {
    const unsigned r = (unsigned)a + b;
    uint8_t f = (uint8_t)(r & (kS | kXY));
    if ((r & 0xFF) == 0) f |= kZ;
    if (((a & 0x0F) + (b & 0x0F)) > 0x0F) f |= kH;
    if ((~(a ^ b) & (a ^ r) & 0x80) != 0) f |= kPV;
    if (r > 0xFF) f |= kC;
    return f;
}

} // namespace z80
} // namespace rules
