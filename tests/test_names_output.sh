#!/bin/sh
# N53: the named output state (src/game/output.h) equals the machine's screen, sprite, sound and latch arrays in every frame of the S27 replay; decoding from the names matches
# the renderer's formula and the sound chip; a shifted view is caught; the lockstep harness compares through the names; the app passes the names to the renderer and the chip.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM"; exit 2; }
mkdir -p build/n53
python3 tools/analysis/oracle_replay.py --make build/n53/s27.replay 7273 >/dev/null 2>&1 || { echo "FAIL: replay"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -Isrc/oracle -Itests/oracle -Isrc tests/test_names_output.cpp src/board/machine.cpp src/board/bus.cpp -o build/n53/test_names_output || { echo "FAIL: build"; exit 1; }
build/n53/test_names_output build/rom/prog.bin build/n53/s27.replay || { echo "FAIL test_names_output.sh"; exit 1; }
grep -q "game::output_view" src/app/main.cpp || { echo "FAIL: the app does not use the named output"; exit 1; }
grep -q "output_to_old_layout" tests/lockstep.h || { echo "FAIL: lockstep does not map the named output"; exit 1; }
echo "PASS test_names_output.sh"
