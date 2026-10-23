#!/bin/sh
# N42: the native attract-mode inter3 of Pac-Man (src/game/attract.h and attract_prog.h; adapter rules::attract in src/rules/rules.h) against the ROM.
# Usage: sh tests/test_native_inter3.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each cinter3age player, 100000 isolated cases per routine (a few minutes);
#   the N42 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n42
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc tests/test_native_inter3.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n42/test_native_inter3 || { echo "FAIL: build"; exit 1; }
build/n42/test_native_inter3 build/rom/prog.bin "$F" "$C" "$N" > build/n42/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n42/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_inter3"; exit 1; }
grep -q "^PASS test_native_inter3" build/n42/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_inter3.sh (report in build/n42/report.txt)"
