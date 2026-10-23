#!/bin/sh
# N21: the native sprite pictures (src/game/sprite_pics.h; adapters in src/rules/rules.h, namespace pics) against the ROM blocks 141F to 16F7.
# Usage: sh tests/test_native_pics.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N21 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n21
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_pics.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n21/test_native_pics || { echo "FAIL: build"; exit 1; }
build/n21/test_native_pics build/rom/prog.bin "$F" "$C" "$N" > build/n21/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n21/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_pics"; exit 1; }
grep -q "^PASS test_native_pics" build/n21/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_pics.sh (report in build/n21/report.txt)"
