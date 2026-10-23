#!/bin/sh
# N20: the native sprite staging in the interrupt handler (src/game/sprites.h; adapter in src/rules/rules.h, namespace sprites) against the ROM block 009D to 018B.
# Usage: sh tests/test_native_sprites.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N20 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n20
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_sprites.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n20/test_native_sprites || { echo "FAIL: build"; exit 1; }
build/n20/test_native_sprites build/rom/prog.bin "$F" "$C" "$N" > build/n20/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n20/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_sprites"; exit 1; }
grep -q "^PASS test_native_sprites" build/n20/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_sprites.sh (report in build/n20/report.txt)"
