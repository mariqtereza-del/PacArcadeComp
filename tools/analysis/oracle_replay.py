#!/usr/bin/env python3
"""S27 oracle: the Python prototype's side of the replay test (tools/analysis/harness.py + oracle_dump.MirrorBoard, exact 50,688-cycle frames).
  python3 tools/analysis/oracle_replay.py --make REPLAY [FRAMES=7273]   write the scripted two-minute play (seeded, so always the same file)
  python3 tools/analysis/oracle_replay.py --hashes REPLAY LOG           run the replay on the prototype and write 'frame sha1' per frame
The replay file format is described in src/app/replay.h. The hash covers memory 4000-4FFF followed by the 48 bytes at 5040-506F
(sound registers and sprite coordinates), exactly what src/app/replay.cpp state_hash() covers.

The script: player 1 game started with a coin at frame 400 and START 1 at 420, random joystick direction (held 10-40 frames) from frame 430 on;
a second game, two coins at 4500 and 4510 and START 2 at 4530, with the same kind of random joystick play from 4600 on. 7,273 frames = 120 s."""
import sys, os, random, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def make(path, frames=7273, seed=7):
    rnd = random.Random(seed)
    in0 = [0xFF] * frames; in1 = [0xFF] * frames
    for f in range(400, 405): in0[f] &= 0xDF                 # coin 1
    for f in range(420, 424): in1[f] &= 0xDF                 # start 1
    for f in range(4500, 4505): in0[f] &= 0xDF               # coin 1
    for f in range(4510, 4515): in0[f] &= 0xDF               # coin 1 again (two coins = two credits)
    for f in range(4530, 4534): in1[f] &= 0xBF               # start 2
    for start in (430, 4600):
        f = start; end = 4400 if start == 430 else frames
        while f < end:
            d = rnd.choice([0xFE, 0xFD, 0xFB, 0xF7]); hold = rnd.randint(10, 40)
            for g in range(f, min(f + hold, end)): in0[g] &= d
            f += hold
    with open(path, 'w') as o:
        o.write('PACMAN-REPLAY 1\ndip C9\nframes %d\n# frame in0 in1 (hex, active low; a value holds until the next line)\n' % frames)
        prev = None
        for f in range(frames):
            cur = (in0[f], in1[f])
            if cur != prev: o.write('%d %02X %02X\n' % (f, cur[0], cur[1])); prev = cur

def load(path):
    dip, frames, ent = 0xC9, 0, []
    for line in open(path):
        line = line.strip()
        if not line or line.startswith('#') or line.startswith('PACMAN-REPLAY'): continue
        p = line.split()
        if p[0] == 'dip': dip = int(p[1], 16)
        elif p[0] == 'frames': frames = int(p[1])
        else: ent.append((int(p[0]), int(p[1], 16), int(p[2], 16)))
    return dip, frames, ent

def hashes(replay, logpath):
    from oracle_dump import MirrorBoard, run_frame_exact
    dip, frames, ent = load(replay)
    b = MirrorBoard(dip=dip); i = 0; cur = (0xFF, 0xFF)
    with open(logpath, 'w') as out:
        for f in range(frames):
            while i < len(ent) and ent[i][0] <= f: cur = (ent[i][1], ent[i][2]); i += 1
            b.ioread[0x5000] = cur[0]; b.ioread[0x5040] = cur[1]
            run_frame_exact(b)
            out.write('%d %s\n' % (f + 1, hashlib.sha1(bytes(b.mem[0x4000:0x5000]) + bytes(b.mem[0x5040:0x5070])).hexdigest()))

if __name__ == '__main__':
    a = sys.argv[1:]
    if a[:1] == ['--make'] and len(a) >= 2: make(a[1], int(a[2]) if len(a) > 2 else 7273)
    elif a[:1] == ['--hashes'] and len(a) >= 3: hashes(a[1], a[2])
    else: print(__doc__); sys.exit(2)
