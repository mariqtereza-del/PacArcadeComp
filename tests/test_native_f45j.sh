#!/bin/sh
# N45j: the native attract-mode f45j of Pac-Man (src/game/attract.h and attract_prog.h; adapter rules::attract in src/rules/rules.h) against the ROM.
# Usage: sh tests/test_native_f45j.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each cf45jage player, 100000 isolated cases per routine (a few minutes);
#   the N45j evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n45j
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc tests/test_native_f45j.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n45j/test_native_f45j || { echo "FAIL: build"; exit 1; }
build/n45j/test_native_f45j build/rom/prog.bin "$F" "$C" "$N" > build/n45j/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n45j/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_f45j"; exit 1; }
grep -q "^PASS test_native_f45j" build/n45j/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_f45j.sh (report in build/n45j/report.txt)"
