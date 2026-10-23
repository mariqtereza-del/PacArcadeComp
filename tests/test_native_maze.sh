#!/bin/sh
# N18: the native maze drawing, dot placing and colours (src/game/maze.h; adapters in src/rules/rules.h, namespace maze) against the ROM routines 2419, 241F, 2448, 245C, 2487, 2498, 24C9, 24D7, 24E7.
# Usage: sh tests/test_native_maze.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N18 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n18
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_maze.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n18/test_native_maze || { echo "FAIL: build"; exit 1; }
build/n18/test_native_maze build/rom/prog.bin "$F" "$C" "$N" > build/n18/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n18/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_maze"; exit 1; }
grep -q "^PASS test_native_maze" build/n18/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_maze.sh (report in build/n18/report.txt)"
