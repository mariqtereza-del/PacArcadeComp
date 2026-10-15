#!/bin/sh
# T01b: the readable scoring (src/logic/scoring.h) against the ROM routine 2A5A, side by side. See tests/test_logic_scoring.cpp.
# Usage: sh tests/test_logic_scoring.sh [FRAMES [RANDOM_CASES [SCORE_LIMIT]]]
# Default: 300,000 frames, 500,000 random cases and the scores 0-99,999 (about 1 minute). The T01b evidence run used 3,000,000 frames, 4,000,000 random cases and all 1,000,000 scores.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || exit 2
mkdir -p build/t01
g++ -O2 -std=c++17 -Wall -Wextra -Isrc/core tests/test_logic_scoring.cpp src/board/machine.cpp src/board/bus.cpp -o build/t01/test_logic_scoring || { echo "FAIL: build"; exit 1; }
build/t01/test_logic_scoring build/rom/prog.bin "${1:-300000}" "${2:-500000}" "${3:-100000}"
