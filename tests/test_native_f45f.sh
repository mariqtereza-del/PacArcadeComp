#!/bin/sh
# N45f: the native attract-mode f45f of Pac-Man (src/game/attract.h and attract_prog.h; adapter rules::attract in src/rules/rules.h) against the ROM.
# Usage: sh tests/test_native_f45f.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each cf45fage player, 100000 isolated cases per routine (a few minutes);
#   the N45f evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n45f
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc tests/test_native_f45f.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n45f/test_native_f45f || { echo "FAIL: build"; exit 1; }
build/n45f/test_native_f45f build/rom/prog.bin "$F" "$C" "$N" > build/n45f/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n45f/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_f45f"; exit 1; }
grep -q "^PASS test_native_f45f" build/n45f/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_f45f.sh (report in build/n45f/report.txt)"
