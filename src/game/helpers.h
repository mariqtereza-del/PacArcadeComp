// N12: the small helper routines that everything uses, written natively (no Z80). ROM routines replaced (entries): 0008 block fill, 0010 table byte,
// 0018 table word, 0020 jump-table dispatch, 0028 queue a task with inline data, 0030 timed event with inline data, 0042 queue a task.
// Each function is the whole routine: it takes the register values the ROM routine starts with, uses the memory through `Mem`, and returns the register
// values and the exact cost (T-states and opcode fetches) the ROM routine would have had. The adapters that connect them to the machine are in
// src/rules/rules.h (namespace helpers); the proof is tests/test_native_helpers.cpp. No ROM bytes: the tables the helpers read are the player's ROM at run time.
//
// Mem: any type with  uint8_t rd(uint16_t)  and  void wr(uint16_t, uint8_t).
#pragma once
#include "state.h"
#include <cstdint>

namespace game {

// ---- flags the Z80 leaves (S Z Y H X P/V N C) for the few instructions the helpers use -----------------------------------------------------------------
namespace flag {
constexpr uint8_t S = 0x80, Z = 0x40, H = 0x10, PV = 0x04, N = 0x02, C = 0x01, XY = 0x28;
inline uint8_t add8(uint8_t a, uint8_t b, unsigned carry, uint8_t *result) { // ADD / ADC
    const unsigned r = (unsigned)a + b + carry;
    uint8_t f = (uint8_t)(r & (S | XY));
    if ((r & 0xFF) == 0) f |= Z;
    if (((a & 0x0F) + (b & 0x0F) + carry) > 0x0F) f |= H;
    if ((~(a ^ b) & (a ^ r) & 0x80) != 0) f |= PV;
    if (r > 0xFF) f |= C;
    *result = (uint8_t)r;
    return f;
}
inline uint8_t inc8(uint8_t v, uint8_t old_f) { // INC r: the carry keeps its value
    const uint8_t r = (uint8_t)(v + 1);
    uint8_t f = (uint8_t)(old_f & C);
    f |= (uint8_t)(r & (S | XY));
    if (r == 0) f |= Z;
    if ((v & 0x0F) == 0x0F) f |= H;
    if (v == 0x7F) f |= PV;
    return f;
}
inline uint8_t and_a(uint8_t a) { // AND A
    uint8_t f = (uint8_t)(H | (a & (S | XY)));
    if (a == 0) f |= Z;
    uint8_t p = a;
    p ^= p >> 4; p ^= p >> 2; p ^= p >> 1;
    if ((p & 1) == 0) f |= PV;
    return f;
}
} // namespace flag

struct Cost {
    unsigned cycles = 0, fetches = 0;
    void add(unsigned c, unsigned f) { cycles += c; fetches += f; }
};

// The registers the helpers use.
struct Regs {
    uint8_t a = 0, f = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    uint16_t hl() const { return (uint16_t)(h << 8 | l); }
    uint16_t de() const { return (uint16_t)(d << 8 | e); }
    void set_hl(uint16_t v) { h = (uint8_t)(v >> 8); l = (uint8_t)v; }
    void set_de(uint16_t v) { d = (uint8_t)(v >> 8); e = (uint8_t)v; }
};

// ---- 0010 (RST 10h): A = the byte at HL + A.  ADD A,L / LD L,A / LD A,0 / ADC A,H / LD H,A / LD A,(HL) / RET.  Out: HL = HL + A (16 bits), A = that byte,
// F = the flags of the ADC. 7 instructions, 40 T-states (the RST that calls it, 11 T-states, is counted by the caller).
constexpr unsigned kTableByteCycles = 40, kTableByteFetches = 7;
template <class Mem> inline void table_byte_0010(Mem &mem, Regs &r, Cost &cost) {
    uint8_t low;
    const uint8_t f1 = flag::add8(r.a, r.l, 0, &low);
    uint8_t high;
    r.f = flag::add8(0, r.h, f1 & flag::C, &high);
    r.l = low;
    r.h = high;
    r.a = mem.rd(r.hl());
    cost.add(kTableByteCycles, kTableByteFetches);
}

// ---- 0018 (RST 18h): HL = the word at HL + 2 * B.  LD A,B / ADD A,A / RST 10h / LD E,A / INC HL / LD D,(HL) / EX DE,HL / RET.
// Out: HL = the word, DE = the address of its second byte, A = its first byte, F = the flags of 0010's ADC. 15 instructions, 11 + 40 + 39 = 90 T-states
// (including the RST 10h inside, not the RST 18h that calls it).
constexpr unsigned kTableWordCycles = 90, kTableWordFetches = 15;
template <class Mem> inline void table_word_0018(Mem &mem, Regs &r, Cost &cost) {
    r.a = (uint8_t)(r.b + r.b);
    Cost inner;
    table_byte_0010(mem, r, inner);
    r.e = r.a;
    r.set_hl((uint16_t)(r.hl() + 1));
    r.d = mem.rd(r.hl());
    const uint16_t second = r.hl();
    r.set_hl(r.de());
    r.set_de(second);
    cost.add(kTableWordCycles, kTableWordFetches);
}

// ---- 0020 (RST 20h): jump through the table that follows the call.  POP HL / ADD A,A / RST 10h / LD E,A / INC HL / LD D,(HL) / EX DE,HL / JP (HL).
// In: A = the entry number, the stack top = the table address (the caller's return address). Out: `target` = where the code continues, SP + 2.
// 15 instructions, 90 T-states.
constexpr unsigned kDispatchCycles = 90, kDispatchFetches = 15;
constexpr uint16_t kDispatchWz = 0x0023; // WZ after the inner RET (the address after the RST 10h at 0022)
template <class Mem> inline uint16_t dispatch_0020(Mem &mem, Regs &r, uint16_t table, Cost &cost) {
    r.set_hl(table);
    r.a = (uint8_t)(r.a + r.a);
    Cost inner;
    table_byte_0010(mem, r, inner);
    r.e = r.a;
    r.set_hl((uint16_t)(r.hl() + 1));
    r.d = mem.rd(r.hl());
    const uint16_t second = r.hl();
    r.set_hl(r.de());
    r.set_de(second);
    cost.add(kDispatchCycles, kDispatchFetches);
    return r.hl();
}

// ---- 0008 (RST 08h): fill B bytes (0 = 256) from HL with A.  LD (HL),A / INC HL / DJNZ 0008 ... RET.   3 instructions per byte, 26 T-states per byte
// (the last one 21), then RET 10: 26 * n + 5 T-states, 3 * n + 1 instructions. Out: HL + n, B = 0; A and F unchanged.
inline unsigned fill_count_0008(uint8_t b) { return b ? b : 256u; }
inline unsigned fill_cycles_0008(unsigned n) { return 26 * n + 5; }
inline unsigned fill_fetches_0008(unsigned n) { return 3 * n + 1; }
template <class Mem> inline void fill_0008(Mem &mem, Regs &r) {
    unsigned n = fill_count_0008(r.b);
    uint16_t hl = r.hl();
    while (n--) mem.wr(hl++, r.a);
    r.set_hl(hl);
    r.b = 0;
}

// ---- 0042: put the task B,C into the task queue (write pointer in RAM at 4C80/4C81, ring 4CC0-4CFF, two bytes per task).
// LD HL,(4C80) / LD (HL),B / INC L / LD (HL),C / INC L / JR NZ,004D / [LD L,C0] / LD (4C80),HL / RET
// Normal: 76 T-states, 8 instructions. When the pointer wraps (L = 0 after the second INC L): 78 T-states, 9 instructions, L = C0.
constexpr uint16_t kQueuePointer = ram::task_queue_write_ptr;
template <class Mem> inline void queue_push_0042(Mem &mem, Regs &r, Cost &cost) {
    r.l = mem.rd(kQueuePointer);
    r.h = mem.rd(kQueuePointer + 1);
    mem.wr(r.hl(), r.b);
    r.f = flag::inc8(r.l, r.f);
    r.l = (uint8_t)(r.l + 1);
    mem.wr(r.hl(), r.c);
    r.f = flag::inc8(r.l, r.f);
    r.l = (uint8_t)(r.l + 1);
    if (r.l == 0) {
        r.l = 0xC0;
        cost.add(78, 9);
    } else {
        cost.add(76, 8);
    }
    mem.wr(kQueuePointer, r.l);
    mem.wr(kQueuePointer + 1, r.h);
}

// ---- 0028 (RST 28h): queue the task whose two bytes follow the call.  POP HL / LD B,(HL) / INC HL / LD C,(HL) / INC HL / PUSH HL / JR 0042 -> 0042.
// In: `inline_at` = the stack top (the caller's return address, where the two bytes are). Out: B, C = the two bytes, HL as 0042 leaves it, the new return
// address inline_at + 2 (written to the stack by the adapter). 59 T-states and 7 instructions before 0042, then 0042's cost.
constexpr unsigned kQueueInlineCycles = 59, kQueueInlineFetches = 7;
// `push(new_return_address)` is called between reading the two bytes and the queue writes, where the ROM executes PUSH HL (so the stack write comes first).
template <class Mem, class Push> inline uint16_t queue_inline_0028(Mem &mem, Regs &r, uint16_t inline_at, Cost &cost, Push &&push) {
    r.b = mem.rd(inline_at);
    r.c = mem.rd((uint16_t)(inline_at + 1));
    const uint16_t next = (uint16_t)(inline_at + 2);
    r.set_hl(next);
    push(next);
    cost.add(kQueueInlineCycles, kQueueInlineFetches);
    queue_push_0042(mem, r, cost);
    return next;
}

// ---- 0030 (RST 30h): find a free slot (first byte 0) among the 16 timed-event slots of 3 bytes at 4C90-4CBF and copy the three bytes that follow the call
// into it. Found: the code continues after the three bytes (JP (HL)); not found: RET to the caller (the inline bytes are then not skipped, as in the ROM).
constexpr uint16_t kSlots = ram::timed_events;
constexpr unsigned kSlotCount = 16;
struct SlotResult {
    bool found = false;
    uint16_t next = 0; // found: where the code continues (inline_at + 3)
};
template <class Mem> inline SlotResult timed_event_0030(Mem &mem, Regs &r, uint16_t inline_at, Cost &cost) {
    SlotResult res;
    cost.add(27, 3);                      // LD DE,4C90 / LD B,10 / JP 0051
    r.set_de(kSlots);
    r.b = kSlotCount;
    for (;;) {
        r.a = mem.rd(r.de());
        r.f = flag::and_a(r.a);           // AND A
        if (r.a == 0) {                   // JR Z taken
            cost.add(7 + 4 + 12, 3);
            res.found = true;
            break;
        }
        cost.add(7 + 4 + 7, 3);           // LD A,(DE) / AND A / JR Z not taken
        for (int i = 0; i < 3; ++i) {     // INC E x3
            r.f = flag::inc8(r.e, r.f);
            r.e = (uint8_t)(r.e + 1);
            cost.add(4, 1);
        }
        --r.b;                            // DJNZ
        if (r.b == 0) { cost.add(8, 1); break; }
        cost.add(13, 1);
    }
    if (!res.found) {                     // RET
        cost.add(10, 1);
        return res;
    }
    cost.add(10 + 7, 2);                  // POP HL / LD B,3
    r.set_hl(inline_at);
    r.b = 3;
    for (;;) {
        r.a = mem.rd(r.hl());
        mem.wr(r.de(), r.a);
        r.set_hl((uint16_t)(r.hl() + 1));
        r.f = flag::inc8(r.e, r.f);
        r.e = (uint8_t)(r.e + 1);
        cost.add(7 + 7 + 6 + 4, 4);
        if (--r.b == 0) { cost.add(8, 1); break; }
        cost.add(13, 1);
    }
    cost.add(4, 1);                       // JP (HL)
    res.next = r.hl();
    return res;
}
// WZ when a slot was found: the last LD (DE),A sets it to A in the high byte and E + 1 in the low byte (E is already incremented by then)
inline uint16_t timed_event_wz(const Regs &r) { return (uint16_t)(r.a << 8 | r.e); }

} // namespace game
