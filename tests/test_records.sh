#!/bin/sh
# X24: personal bests. Unit rules + real-ROM scenarios (tests/test_records.cpp), then the real program on the S27 replay: hashes unchanged, Purist untouched, records.cfg only written by counted clears.
cd "$(dirname "$0")/.." || exit 2
D=build/x24; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_records.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_records 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_records $D/s27.replay | grep -v "^  ok" || exit 1
