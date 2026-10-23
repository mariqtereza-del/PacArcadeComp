#!/bin/sh
# N07: the cycle meter (tests/cycle_meter.h) measures known ROM routines correctly: 2A23 = 111 cycles and 13 instructions, the fill routine 0008 = 5 + 26 * B.
# Usage: sh tests/test_cycle_meter.sh [FRAMES_A FRAMES_B]   (default 100000 60000: about 2 minutes). Writes build/n07/report.txt (copy kept in docs/evidence/N07_cycle_meter.txt).
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n07; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_cycle_meter.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_cycle_meter || { echo "FAIL: build"; exit 1; }
$D/test_cycle_meter build/rom/prog.bin ${1:-100000} ${2:-60000} $D/report.txt | tee $D/run.txt | grep -v "^METER\|^  "
grep -q "^all checks passed" $D/run.txt || { echo "FAIL: a meter check failed"; exit 1; }
grep -q "FORMULA cycles = 111 (constant)" $D/report.txt && grep -q "FORMULA instructions = 13 (constant)" $D/report.txt && grep -q "FORMULA cycles = 5 + 26 \* B(0=256)" $D/report.txt && grep -q "FORMULA instructions = 1 + 3 \* B(0=256)" $D/report.txt || { echo "FAIL: the report does not show the expected formulas"; exit 1; }
echo "PASS test_cycle_meter: 2A23 = 111 cycles / 13 instructions; 0008 = 5 + 26 * B cycles; report in $D/report.txt"
