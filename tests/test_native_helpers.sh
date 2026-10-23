#!/bin/sh
# N12: the native helper routines (src/game/helpers.h; adapters in src/rules/rules.h, namespace helpers) against the ROM routines 0008, 0010, 0018, 0020, 0028, 0030, 0042.
# Usage: sh tests/test_native_helpers.sh [FRAMES] [COVERAGE_FRAMES] [ISOLATED_CASES]
#   default 100000 frames of the scripted player, 20000 frames of each coverage player, 100000 isolated cases per routine (a few minutes);
#   the N12 evidence run used 1,000,000 scripted frames (about 7 minutes).
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; N=${3:-100000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n12
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_native_helpers.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n12/test_native_helpers || { echo "FAIL: build"; exit 1; }
build/n12/test_native_helpers build/rom/prog.bin "$F" "$C" "$N" > build/n12/report.txt
rc=$?
grep -v "ISOLATED MISMATCH" build/n12/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_native_helpers"; exit 1; }
grep -q "^PASS test_native_helpers" build/n12/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_native_helpers.sh (report in build/n12/report.txt)"
