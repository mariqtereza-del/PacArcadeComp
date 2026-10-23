#!/bin/sh
# N27: the native game-mode dispatchers 03C8 03D4 05E5 06BE (src/game/handler.h; adapters in src/rules/rules.h, namespace evt) against the ROM.
# Usage: sh tests/test_native_mode.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N27 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n27
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_mode.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n27/test_native_mode || { echo "FAIL: build"; exit 1; }
build/n27/test_native_mode build/rom/prog.bin "$F" "$C" "$N" > build/n27/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n27/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_mode"; exit 1; }
grep -q "^PASS test_native_mode" build/n27/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_mode.sh (report in build/n27/report.txt)"
