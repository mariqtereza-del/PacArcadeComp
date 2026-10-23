#!/bin/sh
# N13: the native screen address helpers (src/game/screen.h; adapters in src/rules/rules.h, namespace screen) against the ROM routines 2000, 2018, 0065, 2052, 200F, 205A.
# Usage: sh tests/test_native_screen.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N13 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n13
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_screen.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n13/test_native_screen || { echo "FAIL: build"; exit 1; }
build/n13/test_native_screen build/rom/prog.bin "$F" "$C" "$N" > build/n13/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n13/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_screen"; exit 1; }
grep -q "^PASS test_native_screen" build/n13/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_screen.sh (report in build/n13/report.txt)"
