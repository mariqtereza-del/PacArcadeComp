#!/bin/sh
# N49c: every instruction of the native instruction list against the Z80 running the same ROM instruction, from random states (registers, flags, alternates, WZ, R, memory).
# Usage: sh tests/test_native_ops.sh [CASES_PER_INSTRUCTION]   (default 300; the N49c evidence run used 1000 = 5.4 million cases)
cd "$(dirname "$0")/.." || exit 2
N=${1:-300}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n49c
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc tests/test_native_ops.cpp src/board/machine.cpp src/board/bus.cpp -o build/n49c/test_native_ops || { echo "FAIL: build"; exit 1; }
build/n49c/test_native_ops build/rom/prog.bin "$N" > build/n49c/ops.txt; rc=$?
tail -12 build/n49c/ops.txt
[ $rc -eq 0 ] && echo "PASS test_native_ops.sh" || { echo "FAIL test_native_ops.sh"; exit 1; }
