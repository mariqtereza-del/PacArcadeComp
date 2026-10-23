#!/bin/sh
# N15: the native text printer (src/game/text.h; adapters in src/rules/rules.h, namespace text) against the ROM routines 2C5E.
# Usage: sh tests/test_native_text.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N15 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n15
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_text.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n15/test_native_text || { echo "FAIL: build"; exit 1; }
build/n15/test_native_text build/rom/prog.bin "$F" "$C" "$N" > build/n15/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n15/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_text"; exit 1; }
grep -q "^PASS test_native_text" build/n15/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_text.sh (report in build/n15/report.txt)"
