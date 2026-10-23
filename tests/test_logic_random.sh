#!/bin/sh
# T01a: the readable random number generator (src/logic/random_number.h) against the ROM routine 2A23, side by side. See tests/test_logic_random.cpp.
# Usage: sh tests/test_logic_random.sh [FRAMES]   (default 400000 frames, about 1.5 minutes; the T01a evidence run used 3,000,000)
cd "$(dirname "$0")/.." || exit 2
N=${1:-400000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || exit 2
mkdir -p build/t01
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_logic_random.cpp src/board/machine.cpp src/board/bus.cpp -o build/t01/test_logic_random || { echo "FAIL: build"; exit 1; }
build/t01/test_logic_random build/rom/prog.bin "$N"
