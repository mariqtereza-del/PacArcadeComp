"""Throwaway analysis harness for Phase 3 (NOT the port's CPU core).
Runs the Pac-Man program ROM on the 'z80' Python package with just enough board:
memory map, inputs, interrupt enable, IM2 vector, 50688 cycles per frame."""
import zipfile, os
from z80 import Z80Machine

ROOT = os.path.join(os.path.dirname(__file__), '..', '..')
def load_roms(path=os.path.join(ROOT, 'roms', 'pacman.zip')):
    z = zipfile.ZipFile(path)
    prog = b''.join(z.read(n) for n in ['pacman.6e', 'pacman.6f', 'pacman.6h', 'pacman.6j'])
    return prog, {n: z.read(n) for n in ['pacman.5e', 'pacman.5f', '82s123.7f', '82s126.4a', '82s126.1m', '82s126.3m']}

CYCLES_PER_FRAME = 50688   # 3,072,000 Hz / 60.606 Hz
class Board:
    def __init__(self, dip=0xC9):
        self.prog, self.gfx = load_roms()
        self.m = Z80Machine()
        self.mem = self.m.memory
        self.m.set_memory_block(0, self.prog)
        self.ioread = {0x5000: 0xFF, 0x5040: 0xFF, 0x5080: dip, 0x50C0: 0xFF}
        self.io_writes = []          # (frame, addr, value)
        self.int_enable = 0
        self.vec = 0
        self.frame = 0
        self.watch_writes = None     # callable(addr, val, pc) for RAM write tracing
        self.m.mark_addrs(0x5000, 0x100, Z80Machine.READ_MARK | Z80Machine.WRITE_MARK)
        self.m.mark_addrs(0x4000, 0x1000, Z80Machine.WRITE_MARK)
        self.m.set_read_callback(self._rd)
        self.m.set_write_callback(self._wr)
        self.m.set_output_callback(self._out)
        self.m.set_input_callback(lambda port: 0xFF)
        self.m.set_get_int_vector_callback(lambda: self.vec)
    def _rd(self, a):
        a &= 0xFFFF
        if 0x5000 <= a < 0x5100:
            return self.ioread.get(a & 0x50C0, 0xFF)
        return self.mem[a]
    def _wr(self, a, v):
        a &= 0xFFFF
        if 0x5000 <= a < 0x5100:
            if a == 0x5000: self.int_enable = v & 1
            self.io_writes.append((self.frame, a, v))
            self.mem[a] = v
        else:
            self.mem[a] = v
            if self.watch_writes: self.watch_writes(a, v, self.m.pc)
    def _out(self, port, v):
        if (port & 0xFF) == 0: self.vec = v
    def run_frame(self):
        m = self.m
        m.ticks_to_stop = CYCLES_PER_FRAME
        while True:
            ev = m.run()
            if ev & Z80Machine._TICKS_LIMIT_HIT or ev & Z80Machine._END_OF_FRAME: break
        self.frame += 1
        if self.int_enable and m.iff1 and not m.int_disabled:
            if m.halted: m.halted = False; m.pc = (m.pc + 1) & 0xFFFF
            sp = (m.sp - 2) & 0xFFFF
            self.mem[sp] = m.pc & 0xFF; self.mem[sp + 1] = m.pc >> 8
            m.sp = sp; m.iff1 = False; m.iff2 = False
            t = (m.i << 8) | self.vec
            m.pc = self.mem[t] | (self.mem[t + 1] << 8)
if __name__ == '__main__':
    b = Board()
    for f in range(5):
        b.run_frame(); print(f, hex(b.m.pc), b.int_enable, hex(b.vec), len(b.io_writes))

# ---- helpers shared by the analysis scripts ----
from render import tile_offset
def screen_text(b):
    """Return the 28x36 text screen as ASCII (digits/letters shown, blank '.', other tiles '#')."""
    rows = []
    for sy in range(36):
        s = ''
        for sx in range(28):
            c = b.mem[0x4000 + tile_offset(sy, 27 - sx)]
            s += chr(c) if (0x30 <= c <= 0x39 or 0x41 <= c <= 0x5A) else ('.' if c in (0x40, 0x20) else ('#' if c else '.'))
        rows.append(s)
    return '\n'.join(rows)
def run(b, n):
    for _ in range(n): b.run_frame()
def hold(b, addr, mask_low, frames):
    """Hold the active-low bits in mask_low pressed on input port addr for some frames, then release."""
    b.ioread[addr] = 0xFF & ~mask_low
    run(b, frames)
    b.ioread[addr] = 0xFF
