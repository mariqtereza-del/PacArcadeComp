"""S09: run the prototype and report when each labelled routine (docs/disasm/labels.txt) is first reached. Read-only: does not change harness.py.
Writes docs/evidence/S09_prototype_trace.txt (use --check to compare against the stored file instead)."""
import sys, os, collections, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from z80 import Z80Machine
from harness import Board
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
FRAMES = int(os.environ.get('S09_FRAMES', '1500'))
def load_labels():
    out = []
    for l in open(os.path.join(root, 'docs', 'disasm', 'labels.txt')):
        if l.startswith('# ==== S11'): break          # S11 labels are checked by tests/check_gameplay.py
        if l.startswith('#') or not l.strip(): continue
        p = [x.strip() for x in l.split('|')]
        out.append((int(p[0], 16), p[1], p[2], p[3]))
    return out
def run():
    labels = load_labels(); addr = {a: n for a, n, c, k in labels}
    b = Board(); m = b.m
    m.mark_addrs(0, 0x4000, Z80Machine.READ_MARK)
    first = {}; count = collections.Counter(); events = []; frame_hits = collections.defaultdict(list)
    orig_rd = b._rd; orig_wr = b._wr
    def rd(a):
        a &= 0xFFFF
        if a < 0x4000 and a == m.pc and a in addr and not m.halted:   # while HALTed the core keeps re-reading the next byte; that is not an execution
            count[a] += 1
            if a not in first: first[a] = (b.frame, a)
            frame_hits[b.frame].append(a)
        return orig_rd(a)
    def wr(a, v):
        a &= 0xFFFF
        if 0x5000 <= a < 0x5100 and (a < 0x5040 or a >= 0x50C0 or True): events.append((b.frame, a, v, m.pc))
        return orig_wr(a, v)
    m.set_read_callback(rd); m.set_write_callback(wr)
    vec = {}; st = {}; draw = {}
    for f in range(FRAMES):
        in0 = 0xFF; in1 = 0xFF
        if f in range(400, 405): in0 &= 0xDF
        if f in range(410, 414): in1 &= 0xDF
        b.ioread[0x5000] = in0; b.ioread[0x5040] = in1
        v0 = b.vec; b.run_frame()
        if b.vec != v0: vec[f] = b.vec
        nb = sum(1 for i in range(0x40, 0x3C0) if b.mem[0x4000 + i] not in (0x40, 0x00)); draw[f] = nb
        if nb and f >= 298 and 'first_draw' not in vec: vec['first_draw'] = (f, nb)
        st[f] = (b.mem[0x4E00], b.mem[0x4E01], b.mem[0x4E02], b.mem[0x4E03], m.pc, m.halted, b.int_enable)
    return labels, first, count, events, frame_hits, vec, st, b, draw
def report():
    labels, first, count, events, frame_hits, vec, st, b, draw = run()
    fd = vec.pop('first_draw', None)
    L = ['S09 prototype trace: frames 0-%d, inputs: coin 1 at frames 400-404, start 1 at 410-413, no joystick.' % (FRAMES - 1),
         'Frame numbering: frame N is the N-th 50,688-cycle slice; the interrupt is delivered at its end (prototype model, see S07 U1).', '',
         'Label                              address  confidence  first frame  times reached   expectation', ]
    exp = {0x0000: 'frame 0 (reset)', 0x230B: 'frame 0 (jumped to from 0005)', 0x3000: 'frame 1: the first interrupt is delivered at the end of frame 0 and vectors here (vector byte FA)',
           0x234B: 'frame 296: after the self-test and its 8 delay loops (jump at 3295/32A4/32EA)', 0x238D: 'frame 298: end of main_init',
           0x008D: 'frame 299: first interrupt after main_init sets 5000=1 at 238C (vector byte FC set at frame 253)', 0x0038: 'never reached (error trap)'}
    EXPECT = {0x0000: 0, 0x230B: 0, 0x3000: 1, 0x234B: 296, 0x238D: 298, 0x008D: 299, 0x0038: None, 0x23A8: None}
    for a, n, c, k in labels:
        if a in first: fr = '%d' % first[a][0]; ct = count[a]
        else: fr = '-'; ct = 0
        if k == 'data': fr = 'data'
        if a in EXPECT and (first[a][0] if a in first else None) != EXPECT[a] and not k == 'data': L.append('MISMATCH for %s: expected %s got %s' % (n, EXPECT[a], first.get(a)))
        L.append('%-34s %04X     %-10s %-12s %-15d %s' % (n, a, c, fr, ct, exp.get(a, '')))
    L.append('')
    L.append('Where the CPU is at the end of selected frames (frame: pc, interrupts enabled by 5000 bit 0, game-mode bytes 4E00/4E01):')
    L.append('  ' + '; '.join('%d: %04X en=%d 4E00=%02X/%02X' % (f, st[f][4], st[f][6], st[f][0], st[f][1]) for f in (0, 1, 100, 252, 253, 254, 295, 296, 297, 298, 299, 300)))
    L.append('')
    # first non-blank tile in the main play area (video RAM 4040-43BF), after the boot
    L.append('First frame at or after 298 (end of main_init) with a non-blank tile in the main screen area 4040-43BF (before that the self-test fills video RAM with test patterns): ' + ('frame %d (%d tiles)' % fd if fd else 'none'))
    for f in (fd[0] if fd else 0, (fd[0] + 3) if fd else 0):
        L.append('  tiles drawn at the end of frame %d: %d' % (f, draw.get(f, 0)))
    L.append('Vector byte (OUT (0),A) changes: ' + ', '.join('frame %d -> %02X' % (f, v) for f, v in sorted(vec.items())))
    wr0 = [e for e in events if e[1] == 0x5000]
    L.append('First writes to 5000 (frame, value, pc): ' + str([(f, v, '%04X' % pc) for f, a, v, pc in wr0[:8]]))
    # timeline of 4E00/4E01 changes
    prev = None; tl = []
    for f in range(FRAMES):
        s = st[f][:2]
        if s != prev: tl.append('frame %d: 4E00=%02X 4E01=%02X' % (f, s[0], s[1])); prev = s
    L.append('Changes of the game-mode bytes 4E00/4E01: ' + '; '.join(tl[:14]))
    # one full interrupt cycle: pick frame 600
    cyc = 600
    L.append('')
    L.append('One full interrupt cycle (frame %d): labelled routines in the order they are first entered (with repeats collapsed):' % cyc)
    seq = []
    for a in frame_hits.get(cyc, []):
        if not seq or seq[-1] != a: seq.append(a)
    names = {a: n for a, n, c, k in labels}
    L.append('  ' + ' > '.join('%s(%04X)' % (names[a], a) for a in seq))
    L.append('I/O writes in frame %d (address=value at pc): ' % cyc + ', '.join('%04X=%02X@%04X' % (a, v, pc) for f, a, v, pc in events if f == cyc and not (0x5040 <= a < 0x50C0)))
    L.append('Sound/sprite register writes in frame %d: %d writes to 5040-50BF, from pcs %s' % (cyc, sum(1 for f, a, v, pc in events if f == cyc and 0x5040 <= a < 0x50C0), sorted({'%04X' % pc for f, a, v, pc in events if f == cyc and 0x5040 <= a < 0x50C0})))
    return '\n'.join(L) + '\n'
if __name__ == '__main__':
    out = report()
    p = os.path.join(root, 'docs', 'evidence', 'S09_prototype_trace.txt')
    if '--check' in sys.argv:
        ok = open(p).read() == out
        print('PASS S09 trace identical' if ok else 'FAIL S09 trace differs'); sys.exit(0 if ok else 1)
    open(p, 'w').write(out); print(out)
