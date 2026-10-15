#!/bin/sh
# X27: the ghost-chain achievements: made-up events (per round, reset, fruit order, helped game, player 2, test mode) and the real ROM (four chains in one round, then two fruits).
cd "$(dirname "$0")/.." || exit 2
D=build/x27; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_chain_ach.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_chain_ach 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_chain_ach $D/s27.replay
