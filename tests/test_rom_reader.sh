#!/bin/sh
# N11: the ROM data reader (src/data/) returns exactly the bytes of the ROM tables found by step N10. Needs the N10 report (made here with a short run if missing).
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
[ -f build/n10/data_map.txt ] || sh tests/test_rom_data_map.sh 20000 20000 20000 20000 >/dev/null || { echo "FAIL: could not make the N10 report"; exit 1; }
mkdir -p build/n11
g++ -O2 -std=c++17 -Wall -Wextra tests/test_rom_reader.cpp src/data/rom_data.cpp -o build/n11/test_rom_reader || { echo "FAIL: build"; exit 1; }
build/n11/test_rom_reader build/rom/prog.bin docs/notes/rom_data_tables.md build/n10/data_map.txt | tee build/n11/result.txt
grep -q "^PASS test_rom_reader" build/n11/result.txt || { echo "FAIL: test_rom_reader"; exit 1; }
echo "PASS test_rom_reader.sh"
