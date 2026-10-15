#!/bin/sh
# X02: test scenarios on the real ROM (needs build/rom/prog.bin made by tools/ and the S27 replay)
cd "$(dirname "$0")/.." || exit 2
D=build/x02; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_scenarios.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_scenarios 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_scenarios $D/s27.replay
