#!/bin/sh
# N51: the named layout of the actors block (src/game/state.h) equals the machine's memory in every frame of the S27 replay, anchors from ram_map.md hold, a shifted view is caught;
# and no raw work-RAM address is left in the code of src/game and src/rules (comments excluded); state.h is exactly what tools/gen_names.py writes.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM"; exit 2; }
mkdir -p build/n51
python3 tools/analysis/oracle_replay.py --make build/n51/s27.replay 7273 >/dev/null 2>&1 || { echo "FAIL: replay"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -Isrc/oracle -Itests/oracle -Isrc tests/test_names.cpp src/board/machine.cpp src/board/bus.cpp -o build/n51/test_names || { echo "FAIL: build"; exit 1; }
build/n51/test_names build/rom/prog.bin build/n51/s27.replay || { echo "FAIL test_names.sh"; exit 1; }
n=$(grep -rnE "0x4[CDE][0-9A-Fa-f]{2}" src/game src/rules --include=*.h | grep -v "attract_prog.h\|state.h" | awk '{l=$0; sub(/^[^:]+:[0-9]+:/,"",l); i=index(l,"//"); c=(i? substr(l,1,i-1):l); if (c ~ /0x4[CDE][0-9A-Fa-f][0-9A-Fa-f]/) print}' | grep -v '0x4C00' | wc -l)
[ "$n" -eq 0 ] || { echo "FAIL: $n raw work-RAM addresses in code (use the names in src/game/state.h)"; exit 1; }
cp src/game/state.h build/n51/state.h.keep; python3 tools/gen_names.py >/dev/null; cmp -s src/game/state.h build/n51/state.h.keep || { echo "FAIL: state.h is not what tools/gen_names.py writes"; exit 1; }
echo "PASS test_names.sh (no raw actor addresses in the code; state.h is generated)"
