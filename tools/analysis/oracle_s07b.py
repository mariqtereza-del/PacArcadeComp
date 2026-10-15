"""S07 follow-up experiments (read-only). Appends to docs/evidence/S07_prototype_experiments.txt"""
import sys, os, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import harness
from harness import Board
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
out = []
orig = Board._rd
hits = collections.Counter()
def rd(self, a):
    a &= 0xFFFF
    if 0x4800 <= a < 0x4C00: hits[(a, self.m.pc)] += 1
    return orig(self, a)
Board._rd = rd
b = Board()
bad_tile = bad_spr = 0; frames_bad_tile = 0
for f in range(4500):
    b.run_frame()
    n = 0
    for i in range(0x400):
        # visible cells only: skip the 2 top and 2 bottom rows (not drawn)
        if 0x40 <= i < 0x3C0 and (b.mem[0x4400 + i] & 0x20): n += 1
    bad_tile += n; frames_bad_tile += (n > 0)
    for s in range(8):
        if b.mem[0x4FF1 + 2*s] & 0x20 and b.mem[0x5060 + 2*s] | b.mem[0x5061 + 2*s]: bad_spr += 1
out.append("E10 reads in 4800-4BFF (address,pc: count): " + str(sorted(hits.items())[:12]) + (" ..." if len(hits) > 12 else "") + "  total distinct=%d" % len(hits))
out.append("E11 visible tile cells with colour-RAM bit5 set, summed over 4500 frames: %d (frames affected: %d)" % (bad_tile, frames_bad_tile))
out.append("E12 active sprites with palette bit5 set: %d" % bad_spr)
p = open(os.path.join(root, 'roms', 'pacman', '82s126.4a'), 'rb').read() if os.path.exists(os.path.join(root, 'roms', 'pacman', '82s126.4a')) else None
out.append("E13 82s126.4a upper half equals lower half: %s" % (None if p is None else p[:128] == p[128:]))
open(os.path.join(root, 'docs', 'evidence', 'S07_prototype_experiments.txt'), 'a').write("\n".join(out) + "\n")
print("\n".join(out))
