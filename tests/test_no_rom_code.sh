#!/bin/sh
# N48: the machine with all readable rules and all native code executes no ROM instruction (central claim of Option A, measured).
# Usage: sh tests/test_no_rom_code.sh [SCRIPTED_FRAMES] [COVERAGE_FRAMES] [PREFIX_CASES_PER_ADDRESS]
#   default 100000 scripted frames, 20000 frames of each coverage player, 300 random cases per indexed-instruction address (about 2 minutes);
#   the N48 evidence run used 3,000,000 scripted frames and 60,000 coverage frames (about 20 minutes).
# Runs: the S27 replay (7,273 frames), the scripted player of T01a, the coverage players A, B, C and D; every ROM instruction fetched by the Z80 core is counted and listed.
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}; P=${3:-300}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n48
python3 tools/analysis/oracle_replay.py --make build/n48/script.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc tests/test_no_rom_code.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n48/test_no_rom_code || { echo "FAIL: build"; exit 1; }
build/n48/test_no_rom_code build/rom/prog.bin "$F" "$C" build/n48/script.replay "$P" > build/n48/report.txt
rc=$?
cat build/n48/report.txt
[ $rc -eq 0 ] || { echo "FAIL: test_no_rom_code"; exit 1; }
grep -q "^PASS test_no_rom_code" build/n48/report.txt || { echo "FAIL: no PASS line"; exit 1; }
echo "PASS test_no_rom_code.sh (report in build/n48/report.txt)"
