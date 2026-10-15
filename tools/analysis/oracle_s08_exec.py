"""S08: record which ROM addresses the prototype really executes (instruction starts), while 'playing' with scripted inputs.
Writes docs/evidence/S08_runtime_pcs.txt (hex addresses, one per line). Does not change harness.py."""
import sys, os, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from z80 import Z80Machine
from harness import Board
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
pcs = set()
def run(dip, frames, cocktail, seed, rack):
    b = Board(dip=dip); m = b.m; rnd = random.Random(seed)
    m.mark_addrs(0, 0x4000, Z80Machine.READ_MARK)
    orig = b._rd
    def rd(a):
        a &= 0xFFFF
        if a < 0x4000 and a == m.pc: pcs.add(a)
        return orig(a)
    m.set_read_callback(rd)
    d = 0xFF; hold = 0
    for f in range(frames):
        in0 = 0xFF; in1 = 0xFF if not cocktail else 0x7F
        if f % 900 in (400, 401, 402, 403, 404): in0 &= 0xDF            # coin 1
        if f % 900 in (410, 411, 412, 413): in1 &= 0xDF                 # start 1
        if cocktail and f % 900 in (420, 421, 422, 423): in1 &= 0xBF    # start 2
        if f % 900 in (405, 406): in0 &= 0xBF                           # coin 2
        if f % 900 in (700, 701): in0 &= 0x7F                           # service coin
        if hold <= 0: d = rnd.choice([0xFE, 0xFD, 0xFB, 0xF7, 0xFF, 0xFE, 0xFD, 0xFB, 0xF7]); hold = rnd.randint(4, 40)
        hold -= 1; in0 &= d
        if rack and f % 1500 in range(900, 910): in0 &= 0xEF           # rack test (cheat) key
        b.ioread[0x5000] = in0; b.ioread[0x5040] = in1
        b.run_frame()
cfgs = [(0xC9, 5000, False, 1, False), (0xC9, 5000, True, 2, False), (0xFD, 6000, False, 3, True), (0xBC, 5000, True, 4, False), (0x3D, 5000, False, 5, False), (0xC8, 3000, False, 6, True), (0xF9, 3000, False, 7, False), (0xCD, 3000, True, 8, False)]
for c in cfgs:
    run(*c); print(c, len(pcs), flush=True)
open(os.path.join(root, 'docs', 'evidence', 'S08_runtime_pcs.txt'), 'w').write("\n".join(f"{a:04X}" for a in sorted(pcs)) + "\n")
