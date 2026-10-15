"""S07 follow-up 2 (read-only)."""
import sys, os, collections, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from z80 import Z80Machine
import harness
from harness import Board
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
out = []
orig = Board._rd
hits = collections.Counter(); vals = collections.Counter()
def rd(self, a):
    a &= 0xFFFF
    v = orig(self, a)
    if 0x4800 <= a < 0x4C00: hits[(a, self.m.pc)] += 1; vals[v] += 1
    return v
Board._rd = rd
b = Board()
b.m.mark_addrs(0x4800, 0x400, Z80Machine.READ_MARK)
cells = collections.Counter(); firstf = None; fr_list = []
for f in range(4500):
    b.run_frame()
    n = [i for i in range(0x40, 0x3C0) if b.mem[0x4400 + i] & 0x20]
    if n:
        fr_list.append(f)
        for i in n: cells[b.mem[0x4400 + i]] += 1
pcs = collections.Counter(pc for (a, pc) in hits)
out.append("E10b reads in 4800-4BFF with read-mark on: total=%d, distinct addresses=%d, calling PCs=%s, values returned=%s" % (sum(hits.values()), len({a for a, _ in hits}), {f'{k:04X}': c for k, c in pcs.most_common(6)}, {f'{k:02X}': c for k, c in vals.items()}))
out.append("E11b frames with colour bit5 on visible cells: first=%s last=%s count=%d; values: %s" % (fr_list[:1], fr_list[-1:], len(fr_list), {f'{k:02X}': c for k, c in cells.most_common(8)}))
z = zipfile.ZipFile(os.path.join(root, 'roms', 'pacman.zip'))
p = z.read('82s126.4a')
out.append("E13 82s126.4a: size=%d upper half == lower half: %s ; bytes 0x00-0x1F: %s" % (len(p), p[:128] == p[128:], p[:32].hex()))
open(os.path.join(root, 'docs', 'evidence', 'S07_prototype_experiments.txt'), 'a').write("\n".join(out) + "\n")
print("\n".join(out))
