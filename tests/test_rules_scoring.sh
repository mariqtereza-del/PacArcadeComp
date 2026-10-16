#!/bin/sh
# T02b: the readable scoring IN the game (src/rules/rules.h) against the original ROM routine 2A5A. See tests/test_rules_scoring.cpp.
# Also: the replay test with all readable rules on (frame hashes equal to the original run and to the Python prototype).
# Usage: sh tests/test_rules_scoring.sh [FRAMES] [ISOLATED_CASES]   (default 200000 frames, 300000 isolated calls; the T02b evidence run used 3,000,000 frames)
cd "$(dirname "$0")/.." || exit 2
N=${1:-200000}; ISO=${2:-300000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/t02; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Isrc/core tests/test_rules_scoring.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_rules_scoring || { echo "FAIL: build"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -DREPLAY_MAIN src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/replay_tool || { echo "FAIL: build replay tool"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/script.replay 7273 >/dev/null || { echo "FAIL: replay script"; exit 1; }
$D/replay_tool build/rom/prog.bin $D/script.replay $D/original.log > /dev/null || { echo "FAIL: replay (original)"; exit 1; }
$D/replay_tool build/rom/prog.bin $D/script.replay $D/readable.log --rules readable > /dev/null || { echo "FAIL: replay (readable)"; exit 1; }
[ -f $D/python.log ] || python3 tools/analysis/oracle_replay.py --hashes $D/script.replay $D/python.log || { echo "FAIL: Python run"; exit 1; }
cmp -s $D/original.log $D/readable.log || { echo "FAIL: replay with the readable rules differs from the original"; exit 1; }
cmp -s $D/python.log $D/readable.log || { echo "FAIL: replay with the readable rules differs from the Python prototype"; exit 1; }
echo "replay: $(wc -l < $D/readable.log) frame hashes with the readable rules = original rules = Python prototype"
$D/test_rules_scoring build/rom/prog.bin "$N" "$ISO"
