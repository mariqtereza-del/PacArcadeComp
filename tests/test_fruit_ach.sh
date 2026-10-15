#!/bin/sh
# X26: the fruit achievements: made-up events (each kind, five different, same fruit repeated, helped game, player 2, names) and the real ROM (one fruit per round where it appears: rounds 1, 2, 3, 5, 7, 9, 11, 13).
cd "$(dirname "$0")/.." || exit 2
D=build/x26; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_fruit_ach.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_fruit_ach 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_fruit_ach $D/s27.replay | grep -v "^  ok"
