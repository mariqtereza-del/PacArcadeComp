#!/usr/bin/env python3
"""S18 oracle: SHA-1 of RAM 4000-4FFF after each of frames 1..N, from the Python prototype's core and board (tools/analysis/harness.py),
with ONE correction to the board: address mirrors (hardware_facts.md section 2, MAME pacman_map).
Finding of S18: harness.Board has a flat 64K memory, so a write through a mirror address (the ROM draws the "HIGH SCORE" text through
C3xx at frame 297, pc 2C8C) is lost; the real board stores it at 43xx. MirrorBoard below adds the mirrors and nothing else.
Two sections are written to tests/data/oracle_ram_hashes.txt:
  [exact]            frames of exactly 50,688 cycles (the instruction that crosses the limit completes, the excess is not carried over)
  [prototype_frames] harness.Board.run_frame() frame logic unchanged. The z80 package ends a frame early whenever its own free-running
                     tick counter passes a multiple of 100,000 (_z80module.cpp: ticks_per_frame = 100 * 1000), which shortens about every
                     second frame, and harness.py adds 1 to PC when interrupting a halted CPU. Kept so the C++ cycle counting can be
                     proved equal to the prototype under a second, irregular frame pattern (machine_dump --quirks).
Usage: python3 tools/analysis/oracle_dump.py [FRAMES=1500] [--write]   (without --write prints a summary only)"""
import sys, os, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from harness import Board, CYCLES_PER_FRAME
from z80 import Z80Machine
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'data', 'oracle_ram_hashes.txt')

class MirrorBoard(Board):
    """harness.Board plus the address mirrors: ROM copy at 8000-BFFF; with bit 14 set, bits 13 and 15 are ignored; I/O decoded on the low byte
    (0x00-0x3F IN0, 0x40-0x7F IN1, 0x80-0xBF DSW1, 0xC0-0xFF DSW2 for reads) in all of 5000-5FFF; latch bit 5000 sets the interrupt enable."""
    def __init__(self, *a, **k):
        super().__init__(*a, **k)
        both = Z80Machine.READ_MARK | Z80Machine.WRITE_MARK
        self.m.mark_addrs(0x5100, 0xF00, both)       # rest of the I/O mirror area
        self.m.mark_addrs(0x6000, 0xA000, both)      # 6000-FFFF: mirrors
    @staticmethod
    def canon(a):
        a &= 0xFFFF
        return a & 0x3FFF if not (a & 0x4000) else a & 0x5FFF
    def _rd(self, a):
        c = self.canon(a)
        if 0x5000 <= c < 0x6000: return self.ioread.get(0x5000 | (c & 0xC0), 0xFF)
        return self.mem[c]
    def _wr(self, a, v):
        if not (a & 0x4000): return                  # ROM copy: read only
        c = self.canon(a)
        if 0x5000 <= c < 0x6000:
            if (c & 0xFF) < 0x40 and (c & 7) == 0: self.int_enable = v & 1
            self.io_writes.append((self.frame, c, v)); self.mem[c] = v
        else:
            self.mem[c] = v
            if self.watch_writes: self.watch_writes(c, v, self.m.pc)

def run_frame_exact(b):
    m = b.m
    m.ticks_to_stop = CYCLES_PER_FRAME
    while not (m.run() & Z80Machine._TICKS_LIMIT_HIT):   # ignore the wrapper's own END_OF_FRAME (100,000-tick) event
        pass
    b.frame += 1
    if b.int_enable and m.iff1 and not m.int_disabled:   # same interrupt rule as harness.py, without the halted PC+1
        if m.halted: m.halted = False
        sp = (m.sp - 2) & 0xFFFF
        b.mem[sp] = m.pc & 0xFF; b.mem[(sp + 1) & 0xFFFF] = m.pc >> 8
        m.sp = sp; m.iff1 = False; m.iff2 = False
        t = (m.i << 8) | b.vec
        m.pc = b.mem[t] | (b.mem[(t + 1) & 0xFFFF] << 8)

def hashes(n, exact):
    b = MirrorBoard(); out = []
    for f in range(1, n + 1):
        run_frame_exact(b) if exact else b.run_frame()
        out.append('%d %s' % (f, hashlib.sha1(bytes(b.mem[0x4000:0x5000])).hexdigest()))
    return out

if __name__ == '__main__':
    n = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 1500
    ex, pr = hashes(n, True), hashes(n, False)
    same = sum(1 for a, c in zip(ex, pr) if a == c)
    print('frames %d: exact and prototype-frame hashes equal in %d frames' % (n, same))
    if '--write' in sys.argv:
        with open(OUT, 'w') as f:
            f.write('# SHA-1 of RAM 4000-4FFF after each frame (4096 bytes, video+colour+4800-4BFF+work+sprite RAM), made by tools/analysis/oracle_dump.py\n')
            f.write('# Format: frame sha1. Section meanings: see the docstring of tools/analysis/oracle_dump.py\n')
            f.write('[exact]\n' + '\n'.join(ex) + '\n[prototype_frames]\n' + '\n'.join(pr) + '\n')
        print('wrote', os.path.normpath(OUT))
