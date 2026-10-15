// S15: run the Pac-Man program ROM from reset on the z80 core with simple RAM and stubbed I/O,
// printing PC and all registers before every instruction. tools/analysis/oracle_trace.py does the
// same on the Python prototype; tests/test_cpu_parity.sh compares the two logs.
// Board rules (identical on both sides): flat 64K memory, ROM image at 0000 (writes go through, as in the prototype),
// reads of 5000-50FF come from a small table (5000=FF, 5040=FF, 5080=C9, 50C0=FF, else FF; a read at a-address uses a&0x50C0),
// IN returns FF, OUT to port 0 sets the interrupt vector byte, a write to 5000 sets the interrupt enable (bit 0),
// and an interrupt is delivered (instead of the next instruction) when the CPU is halted, enable = 1 and IFF1 = 1:
// halted cleared, PC pushed, IFF1 = IFF2 = 0, PC = word at (I*256 + vector).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "z80.h"
using z80::fast_u16;
using z80::fast_u8;
using z80::least_u8;

class board : public z80::z80_cpu<board> {
public:
    typedef z80::z80_cpu<board> base;
    least_u8 mem[z80::address_space_size];
    int int_enable = 0;
    unsigned vec = 0;
    fast_u8 on_read(fast_u16 a) {
        if (a >= 0x5000 && a < 0x5100) {
            switch (a & 0x50C0) {
            case 0x5080: return 0xC9;
            default: return 0xFF;
            }
        }
        return mem[a];
    }
    void on_write(fast_u16 a, fast_u8 v) {
        mem[a] = static_cast<least_u8>(v);
        if (a == 0x5000) int_enable = v & 1;
    }
    fast_u8 on_input(fast_u16) { return 0xFF; }
    void on_output(fast_u16 port, fast_u8 v) {
        if ((port & 0xFF) == 0) vec = v;
    }
};

static void log_state(board &e) {
    std::printf("%04X %04X %04X %04X %04X %04X %04X %04X %02X %02X %d%d %04X %04X %04X %04X\n", (unsigned)e.on_get_pc(), (unsigned)e.on_get_af(), (unsigned)e.on_get_bc(),
                (unsigned)e.on_get_de(), (unsigned)e.on_get_hl(), (unsigned)e.on_get_ix(), (unsigned)e.on_get_iy(), (unsigned)e.on_get_sp(), (unsigned)e.on_get_i(),
                (unsigned)e.on_get_r(), (int)e.on_get_iff1(), (int)e.on_get_iff2(), (unsigned)e.get_alt_af(), (unsigned)e.get_alt_bc(), (unsigned)e.get_alt_de(),
                (unsigned)e.get_alt_hl());
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: boot_test prog.bin count\n");
        return 2;
    }
    static board e;
    std::memset(e.mem, 0, sizeof e.mem);
    FILE *f = std::fopen(argv[1], "rb");
    if (!f || std::fread(e.mem, 1, 0x4000, f) != 0x4000) {
        std::fprintf(stderr, "cannot read ROM\n");
        return 2;
    }
    std::fclose(f);
    long n = std::atol(argv[2]);
    for (long i = 0; i < n; ++i) {
        if (e.on_is_halted() && e.int_enable && e.on_get_iff1()) {
            e.on_set_is_halted(false);
            unsigned sp = (e.on_get_sp() - 2) & 0xFFFF, pc = e.on_get_pc();
            e.mem[sp] = pc & 0xFF;
            e.mem[(sp + 1) & 0xFFFF] = pc >> 8;
            e.on_set_sp(sp);
            e.on_set_iff1(false);
            e.on_set_iff2(false);
            unsigned t = (e.on_get_i() << 8) | e.vec;
            e.on_set_pc(e.mem[t] | (e.mem[(t + 1) & 0xFFFF] << 8));
        }
        log_state(e);
        e.on_step();
    }
    return 0;
}
