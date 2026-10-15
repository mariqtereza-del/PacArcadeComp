// S14 test: run a tiny known Z80 program on the z80 core and print the registers.
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "z80.h"
using z80::fast_u16;
using z80::fast_u8;
using z80::least_u8;
class emu : public z80::z80_cpu<emu> {
public:
    typedef z80::z80_cpu<emu> base;
    least_u8 mem[z80::address_space_size] = {
        0x21, 0x34, 0x12, // 0000 ld hl,0x1234
        0x3e, 0x07,       // 0003 ld a,7
        0x77,             // 0005 ld (hl),a
        0x06, 0x05,       // 0006 ld b,5
        0x86,             // 0008 loop: add a,(hl)
        0x10, 0xfd,       // 0009 djnz loop
        0x76,             // 000b halt
    };
    unsigned long ticks = 0;
    fast_u8 on_read(fast_u16 a) { return mem[a]; }
    void on_write(fast_u16 a, fast_u8 n) { mem[a] = static_cast<least_u8>(n); }
};
int main() {
    emu e;
    int steps = 0;
    while (e.mem[e.on_get_pc()] != 0x76) {
        e.on_step();
        ++steps;
    }
    unsigned a = e.on_get_a(), b = e.on_get_b(), f = e.on_get_f(), hl = e.on_get_hl(), pc = e.on_get_pc();
    unsigned m = e.mem[0x1234];
    std::printf("steps=%d A=%02X B=%02X F=%02X HL=%04X PC=%04X mem[1234]=%02X\n", steps, a, b, f, hl, pc, m);
    bool ok = steps == 14 && a == 0x2A && b == 0 && f == 0x28 && hl == 0x1234 && pc == 0x000B && m == 7;
    std::puts(ok ? "PASS z80 core test" : "FAIL z80 core test");
    return ok ? 0 : 1;
}
