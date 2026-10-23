#!/bin/sh
# N14: the native number helpers (src/game/numbers.h; adapters in src/rules/rules.h, namespace numbers) against the ROM routines 2ACE, 2ABE, 2AAF, 2B0B, 02DF.
# Usage: sh tests/test_native_numbers.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N14 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n14
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_numbers.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n14/test_native_numbers || { echo "FAIL: build"; exit 1; }
build/n14/test_native_numbers build/rom/prog.bin "$F" "$C" "$N" > build/n14/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n14/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_numbers"; exit 1; }
grep -q "^PASS test_native_numbers" build/n14/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_numbers.sh (report in build/n14/report.txt)"
