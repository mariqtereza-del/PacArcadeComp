#!/bin/sh
# N05: service player D (tests/coverage_player.h, run_d): the service switch, the input test screens, a restart in the middle of a game. Deterministic.
# Usage: sh tests/test_coverage_d.sh [FRAMES]   (default 54000 frames = six power-ons, about 40 seconds)
cd "$(dirname "$0")/.." || exit 2
N=${1:-54000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n04; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_coverage_d.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_coverage_d || { echo "FAIL: build"; exit 1; }
$D/test_coverage_d build/rom/prog.bin $N $D/cov_d.txt | tee $D/run_d.txt || { echo "FAIL: the player is not deterministic"; exit 1; }
# the input test screens (ROM 3298 to 32EA) and the screen drawing after them (3AF4) must have run
python3 - $D/cov_d.txt <<'P' || { echo "FAIL: the service test did not run"; exit 1; }
import sys
s = set()
for ln in open(sys.argv[1]):
    if ln[0] == '#': continue
    a, b = ln.split(); s.update(range(int(a, 16), int(b, 16) + 1))
missing = [hex(x) for x in (0x3298, 0x32A7, 0x32C6, 0x32DC, 0x32EA, 0x3AF4) if x not in s]
print('service test addresses reached' if not missing else 'missing ' + str(missing)); sys.exit(1 if missing else 0)
P
echo "PASS test_coverage_d: deterministic; the service test screens ran"
