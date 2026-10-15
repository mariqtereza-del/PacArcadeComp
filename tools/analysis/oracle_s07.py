"""S07 experiments on the prototype (read-only: does not change harness.py). Writes docs/evidence/S07_prototype_experiments.txt"""
import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from z80 import Z80Machine
import harness
from harness import Board
reads = collections.Counter(); out = []
orig_rd = Board._rd
def rd(self, a):
    a &= 0xFFFF
    if 0x4800 <= a < 0x4C00 or 0x5000 <= a < 0x5100: reads[a] += 1
    return orig_rd(self, a)
Board._rd = rd
b = Board()
b.m.mark_addrs(0x4800, 0x400, Z80Machine.READ_MARK | Z80Machine.WRITE_MARK)
wr_4800 = collections.Counter(); ram_hi_writes = collections.Counter()
def watch(a, v, pc):
    if 0x4800 <= a < 0x4C00: wr_4800[a] += 1
b.watch_writes = watch
N = 4500; maxcol = 0; maxspr = 0; colvals = set(); sprvals = set()
for f in range(N):
    b.run_frame()
    cr = bytes(b.mem[0x4400:0x4800]); colvals.update(cr); maxcol = max(maxcol, max(cr))
    sp = [b.mem[0x4FF1 + 2*n] for n in range(8)]; sprvals.update(sp); maxspr = max(maxspr, max(sp))
out.append(f"Ran {N} frames of the prototype (attract mode and demo).")
out.append("E1 I/O read addresses touched by the ROM (address: count): " + ", ".join(f"{a:04X}:{c}" for a, c in sorted(reads.items()) if a >= 0x5000))
out.append("E2 reads/writes in 4800-4BFF: reads=%d writes=%d" % (sum(c for a, c in reads.items() if a < 0x4C00), sum(wr_4800.values())))
w = collections.defaultdict(collections.Counter)
for fr, a, v in b.io_writes: w[a][v] += 1
out.append("E3 I/O write addresses and the values written (value:count): " + "; ".join(f"{a:04X} -> " + ",".join(f"{v:02X}:{c}" for v, c in sorted(w[a].items())) for a in sorted(w) if a < 0x5040 or a >= 0x50C0))
out.append("E3b other I/O write ranges seen: " + ", ".join(f"{a:04X}" for a in sorted(w) if 0x5040 <= a < 0x50C0)[:600])
first = [(fr, a, v) for fr, a, v in b.io_writes if a == 0x5000][:12]
out.append("E4 first writes to 5000 (frame, value): " + str([(fr, v) for fr, a, v in first]))
out.append("E5 colour RAM (4400-47FF) values seen: max=%02X, distinct=%d, any above 0x1F: %s, any above 0x0F: %s" % (maxcol, len(colvals), any(v > 0x1F for v in colvals), any(v > 0x0F for v in colvals)))
out.append("E6 sprite palette bytes (4FF1,4FF3,...) values seen: max=%02X set=%s" % (maxspr, sorted(f'{v:02X}' for v in sprvals)))
rom = b.prog
out.append("E7 ROM interrupt vector table: word at 3FFA = %04X, word at 3FFC = %04X, I register = %02X" % (rom[0x3FFA] | rom[0x3FFB] << 8, rom[0x3FFC] | rom[0x3FFD] << 8, b.m.i))
out.append("E8 OUT port 0 vector byte at end: %02X; interrupt enable flag at end: %d" % (b.vec, b.int_enable))
out.append("E9 sprite regs at frame %d: " % N + " ".join(f"s{n}:code/flip={b.mem[0x4FF0+2*n]:02X} pal={b.mem[0x4FF1+2*n]:02X} r60={b.mem[0x5060+2*n]:02X} r61={b.mem[0x5061+2*n]:02X}" for n in range(8)))
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'docs', 'evidence', 'S07_prototype_experiments.txt'), 'w').write("\n".join(out) + "\n")
print("\n".join(out))
