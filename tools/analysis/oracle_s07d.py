"""S07 experiment: does returning 0xBF (MAME's value) for reads of 4800-4BFF change anything? (read-only on harness)"""
import sys, os, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from z80 import Z80Machine
import harness
from harness import Board, run
from render import render
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
def go(patch):
    if patch:
        orig = Board._rd
        Board._rd = lambda self, a: 0xBF if 0x4800 <= (a & 0xFFFF) < 0x4C00 else orig(self, a)
    b = Board()
    if patch: b.m.mark_addrs(0x4800, 0x400, Z80Machine.READ_MARK)
    res = {}
    for t in (700, 1500, 4500):
        run(b, t - b.frame); res[t] = (hashlib.sha1(render(b.mem, b.gfx).tobytes()).hexdigest(), hashlib.sha1(bytes(b.mem[0x4000:0x5000])).hexdigest())
    return res
a = go(False); b = go(True)
line = "E14 return 0xBF instead of 0x00 for 4800-4BFF reads: identical picture and RAM at frames 700/1500/4500: %s" % (a == b)
open(os.path.join(root, 'docs', 'evidence', 'S07_prototype_experiments.txt'), 'a').write(line + "\n"); print(line)
