#!/usr/bin/env python3
"""Step S05 regression guard for the Python prototype (tools/analysis).
What it proves: the prototype still boots the ROM, enables VBLANK, uses IM2 vector 0xFA, and is deterministic
(two runs give identical frames; frame hashes equal the recorded golden values).
What it does NOT prove: that the picture matches a real arcade pixel for pixel. The golden hashes were produced by
the prototype itself; visual agreement with real Pac-Man screens is recorded in docs/evidence/prototype/."""
import sys, os, hashlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'analysis'))
from harness import Board, run
from render import render
GOLDEN = {700: 'b63cb26056e76c05f084894d7cb3f86c78f2e813', 1500: '3d0eb894d703684b468bfe8013ebdf3dfe5c46de'}
def frames():
    b = Board(); out = {}
    for t in sorted(GOLDEN):
        run(b, t - b.frame); out[t] = hashlib.sha1(render(b.mem, b.gfx).tobytes()).hexdigest()
    return b, out
def main():
    fails = []
    b, a = frames(); _, c = frames()
    if a != c: fails.append('not deterministic between two runs')
    for t, h in GOLDEN.items():
        if a[t] != h: fails.append(f'frame {t} hash {a[t]} != golden {h}')
    e = Board(); run(e, 5)
    if e.vec != 0xFA: fails.append(f'IM2 vector byte at frame 5 is {e.vec:#x}, expected 0xFA')
    if not e.int_enable: fails.append('VBLANK interrupt not enabled by frame 5')
    if b.vec != 0xFC: fails.append(f'IM2 vector byte at frame 1500 is {b.vec:#x}, expected 0xFC (finding: the ROM changes the vector byte after boot)')
    print('\n'.join(['FAIL: ' + f for f in fails]) if fails else 'PASS prototype: deterministic, golden frames 700 and 1500 reproduced, IM2 vector 0xFA at boot then 0xFC at frame 1500, VBLANK enabled')
    return 1 if fails else 0
if __name__ == '__main__': sys.exit(main())
