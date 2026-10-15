#!/usr/bin/env python3
"""S15 oracle: run the ROM from reset on the Python prototype's core with the same simple board as
src/board/boot_test.cpp and print the same trace (PC and registers before every instruction).
Usage: python3 tools/analysis/oracle_trace.py COUNT > python_trace.txt
Differences from tools/analysis/harness.py Board: none in the memory map or I/O; the interrupt is delivered
when the CPU is halted, enabled and IFF1 is set (instead of after 50,688 cycles) so that both sides need no cycle counter.
The CPU power-on registers are set to AF = SP = FFFF (the real Z80 reset values; the Python wrapper starts at AF=0002, SP=0)."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Board

def main(n):
    b = Board(); m = b.m
    m.af = 0xFFFF; m.sp = 0xFFFF
    out = []
    for _ in range(n):
        if m.halted and b.int_enable and m.iff1:
            m.halted = False
            sp = (m.sp - 2) & 0xFFFF; pc = m.pc
            b.mem[sp] = pc & 0xFF; b.mem[(sp + 1) & 0xFFFF] = pc >> 8
            m.sp = sp; m.iff1 = False; m.iff2 = False
            t = (m.i << 8) | b.vec
            m.pc = b.mem[t] | (b.mem[(t + 1) & 0xFFFF] << 8)
        out.append('%04X %04X %04X %04X %04X %04X %04X %04X %02X %02X %d%d %04X %04X %04X %04X' % (
            m.pc, m.af, m.bc, m.de, m.hl, m.ix, m.iy, m.sp, m.i, m.r, int(m.iff1), int(m.iff2),
            m.alt_af, m.alt_bc, m.alt_de, m.alt_hl))
        m.ticks_to_stop = 1
        m.run()
    sys.stdout.write('\n'.join(out) + '\n')

if __name__ == '__main__':
    main(int(sys.argv[1]))
