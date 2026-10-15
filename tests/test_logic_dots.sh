#!/bin/sh
# T01c: the readable dots and fruit (src/logic/dots_and_fruit.h) against the ROM, side by side. See tests/test_logic_dots.cpp.
# Usage: sh tests/test_logic_dots.sh [FRAMES [RANDOM_CASES]]
# Default: 300,000 frames and 100,000 random cases (about 1 minute). The T01c evidence run used 3,000,000 frames and 1,000,000 random cases per piece.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || exit 2
mkdir -p build/t01
g++ -O2 -std=c++17 -Wall -Wextra -Isrc/core tests/test_logic_dots.cpp src/board/machine.cpp src/board/bus.cpp -o build/t01/test_logic_dots || { echo "FAIL: build"; exit 1; }
build/t01/test_logic_dots build/rom/prog.bin "${1:-300000}" "${2:-100000}"
