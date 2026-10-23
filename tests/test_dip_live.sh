#!/bin/sh
# S34D: DIP settings put into a running machine (src/app/dipnow.h) equal a power-on with those settings (4 setting sets, 4000 frames each, all RAM + CPU + cycles every frame); a wrong version is caught.
# Needs build/rom/prog.bin (python3 tools/check_rom.py). About 10 seconds.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
mkdir -p build/s34d
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Isrc tests/test_dip_live.cpp src/board/machine.cpp src/board/bus.cpp src/board/inputs.cpp -o build/s34d/t_dip || { echo "FAIL: build"; exit 1; }
build/s34d/t_dip build/rom/prog.bin | tee build/s34d/dip_live.txt | tail -12
grep -q "^PASS test_dip_live" build/s34d/dip_live.txt || { echo "FAIL: test_dip_live"; exit 1; }
echo "PASS test_dip_live.sh"
