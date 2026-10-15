#!/bin/sh
# X28: the cleared-round achievements and PEACEFUL RUN: made-up events and the real ROM (rounds 2, 5, 9, 13, 17 cleared by the game; three clean rounds; a ghost breaks the streak).
cd "$(dirname "$0")/.." || exit 2
D=build/x28; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_round_ach.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_round_ach 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_round_ach $D/s27.replay
