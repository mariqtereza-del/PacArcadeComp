#!/bin/sh
# N16: the native score, life, credit and fruit pictures (src/game/status.h; adapters in src/rules/rules.h, namespace status) against the ROM routines 2AE0, 2B33, 2B4A, 2B6A, 2B7E, 2B80, 2B8F, 2BA1, 2BCD, 2BEA.
# Usage: sh tests/test_native_status.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N16 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n16
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_status.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n16/test_native_status || { echo "FAIL: build"; exit 1; }
build/n16/test_native_status build/rom/prog.bin "$F" "$C" "$N" > build/n16/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n16/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_status"; exit 1; }
grep -q "^PASS test_native_status" build/n16/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_status.sh (report in build/n16/report.txt)"
