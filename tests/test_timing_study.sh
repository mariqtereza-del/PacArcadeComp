#!/bin/sh
# N06: timing study (tests/timing_study.cpp): where the interrupt lands, the real length of a frame, the cost of the interrupt handler, the queue depth.
# Usage: sh tests/test_timing_study.sh [FRAMES_A FRAMES_B FRAMES_C FRAMES_D]   (default 300000 300000 300000 54000: about 4 minutes)
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n06; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/timing_study.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/timing_study || { echo "FAIL: build"; exit 1; }
$D/timing_study build/rom/prog.bin docs/notes/rom_routines.md $D/timing.txt ${1:-300000} ${2:-300000} ${3:-300000} ${4:-54000} || { echo "FAIL: the study did not run"; exit 1; }
python3 - <<'P' || exit 1
import re, sys
t = open('build/n06/timing.txt').read()
def g(pat):
    m = re.search(pat, t, re.M)
    if not m: print('FAIL: missing in the output:', pat); sys.exit(1)
    return m.groups()
frames, = g(r'^all: (\d+) frames'); frames = int(frames)
deliv, notdel = map(int, g(r'interrupt delivered in (\d+), not delivered .* in (\d+)'))
poll, = g(r'polling loop 238D-2392 : (\d+)'); glue, = g(r'2395-23A7 : (\d+)'); mid, = g(r'INSIDE A RUNNING TASK \(any other ROM address\) : (\d+)')
halted, = g(r'halted : (\d+)')
if frames != deliv + notdel: print('FAIL: delivered + not delivered != frames'); sys.exit(1)
if int(poll) + int(glue) + int(mid) + int(halted) != deliv: print('FAIL: the landing classes do not add up to the delivered interrupts'); sys.exit(1)
fmin, fmax = map(int, g(r'shortest (\d+), longest (\d+)'))
if fmin < 50688 or fmax > 50688 + 23: print('FAIL: a frame has a length outside 50688 to 50711 cycles'); sys.exit(1)
over = sum(int(x) for x in re.findall(r'^  \+\d+ : (\d+) frames', t, re.M))
if over != frames: print('FAIL: the overshoot table does not cover every frame'); sys.exit(1)
rows = [l.split(' | ') for l in t.split('\n') if re.match(r'^[0-9A-F]{4} \| [0-9A-F]{4} \| ', l)]
places, = g(r'^places: (\d+);'); 
if len(rows) != int(places) or not rows: print('FAIL: the place table is incomplete'); sys.exit(1)
if sum(int(r[3].split()[0]) for r in rows) != int(mid): print('FAIL: the place table does not add up to the mid-task frames'); sys.exit(1)
for r in rows:
    if r[1] == '0000' or r[2] == '?' or len(r) < 8: print('FAIL: a place without a routine or verdict:', r[0]); sys.exit(1)
hn, hmin, hmax = map(int, g(r'frames measured (\d+); cheapest (\d+), dearest (\d+)'))
if hmin < 1000 or hmax > 50688: print('FAIL: handler cost out of range'); sys.exit(1)
print('study output consistent: %d frames, %d inside a task, %d places in %d routines' % (frames, int(mid), len(rows), len(set(r[1] for r in rows))))
# the notes file quotes the numbers of this run
n = open('docs/notes/native_timing.md').read()
for need in (str(frames), mid, places, str(hmin), str(hmax)):
    if need not in n.replace(',', ''): print('FAIL: docs/notes/native_timing.md does not quote', need); sys.exit(1)
print('notes file quotes the numbers of this run')
P
# no ROM bytes in the notes file (16-byte window rule of the export)
python3 - <<'P' || { echo "FAIL: ROM bytes in the notes file"; exit 1; }
import zipfile, sys
b = open('docs/notes/native_timing.md', 'rb').read()
with zipfile.ZipFile('roms/pacman.zip') as z:
    for n in z.namelist():
        d = z.read(n)
        for i in range(0, len(d) - 15):
            w = d[i:i+16]
            if len(set(w)) >= 8 and (w in b or w.hex().encode() in b.replace(b' ', b'')): print('FOUND', n, i); sys.exit(1)
print('notes file: no run of 16 ROM bytes')
P
echo "PASS test_timing_study: the interrupt places, the frame length, the handler cost and the queue depth are measured; numbers recorded in docs/notes/native_timing.md"
