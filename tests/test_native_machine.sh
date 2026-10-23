#!/bin/sh
# N49a: the machine without the Z80 core (register file only, src/board/cpuregs.h, build macro PAC_NO_Z80) against the Z80 machine running the original ROM.
# Both play the S27-style replay, the scripted player of T01a, and the coverage players A, B, C, D; one fingerprint line per frame (memory, I/O writes, cycles,
# registers); the two files must be identical. A deliberately wrong native machine must be caught. The native build must not contain the Z80 core.
# Usage: sh tests/test_native_machine.sh [SCRIPTED_FRAMES] [COVERAGE_FRAMES]   (default 100000 20000; the N49a evidence run used 1,000,000 and 60,000)
cd "$(dirname "$0")/.." || exit 2
F=${1:-100000}; C=${2:-20000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/n49a
python3 tools/analysis/oracle_replay.py --make build/n49a/script.replay 7273 >/dev/null 2>&1 || { echo "FAIL: no replay file"; exit 1; }
SRC="tests/test_native_machine.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp"
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc $SRC -o build/n49a/native_z80 || { echo "FAIL: build (Z80 machine)"; exit 1; }
g++ -O2 -std=c++17 -DPAC_NO_Z80 -Wall -Wextra -Itests/oracle -Isrc $SRC -o build/n49a/native_nz80 || { echo "FAIL: build (machine without the Z80)"; exit 1; }
# the native build must not contain the Z80 core: neither its source after the preprocessor, nor its symbols
n_src=$(g++ -E -std=c++17 -DPAC_NO_Z80 -Itests/oracle -Isrc tests/test_native_machine.cpp 2>/dev/null | grep -c "z80_cpu\|z80_executor\|z80_decoder\|z80_state")
n_sym=$(nm -C build/n49a/native_nz80 | grep -c "z80_cpu\|z80_executor\|z80_decoder\|z80_state")
n_ref=$(nm -C build/n49a/native_z80 | grep -c "z80_cpu\|z80_executor\|z80_decoder\|z80_state")
echo "Z80 core in the native build: $n_src lines after the preprocessor, $n_sym symbols (the Z80 build has $n_ref symbols)"
[ "$n_src" -eq 0 ] && [ "$n_sym" -eq 0 ] && [ "$n_ref" -gt 0 ] || { echo "FAIL: the native build contains the Z80 core"; exit 1; }
build/n49a/native_z80 build/rom/prog.bin build/n49a/z80.txt "$F" "$C" build/n49a/script.replay || { echo "FAIL: run (Z80 machine)"; exit 1; }
build/n49a/native_nz80 build/rom/prog.bin build/n49a/nz80.txt "$F" "$C" build/n49a/script.replay || { echo "FAIL: run (machine without the Z80)"; exit 1; }
build/n49a/native_nz80 build/rom/prog.bin build/n49a/wrong.txt "$F" "$C" build/n49a/script.replay WRONG >/dev/null || { echo "FAIL: run (wrong version)"; exit 1; }
n=$(wc -l < build/n49a/z80.txt)
if cmp -s build/n49a/z80.txt build/n49a/nz80.txt; then echo "frame fingerprints (memory, I/O writes, cycles, registers) identical on all $n frames"; else
  echo "FAIL: the machines differ; first difference:"; diff build/n49a/z80.txt build/n49a/nz80.txt | head -4; exit 1; fi
if cmp -s build/n49a/z80.txt build/n49a/wrong.txt; then echo "FAIL: the deliberately wrong machine (one cycle too slow in scripted frame 1000) was NOT caught"; exit 1; else
  echo "wrong version caught: $(diff build/n49a/z80.txt build/n49a/wrong.txt | grep -c '^>') frame lines differ (first: $(diff build/n49a/z80.txt build/n49a/wrong.txt | grep '^>' | head -1 | cut -c1-30))"; fi
echo "PASS test_native_machine.sh ($n frames)"
