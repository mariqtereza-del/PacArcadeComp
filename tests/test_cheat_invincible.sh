#!/bin/sh
# The INVINCIBLE test cheat on the real ROM, in the original and in the readable rules (no ghost kills Pac-Man unless it is blue; blue ghosts are eaten).
cd "$(dirname "$0")/.." || exit 2
D=build/cheat; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra -Itests/oracle -Isrc tests/test_cheat_invincible.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_cheat_invincible 2>&1 | grep -E "error" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_cheat_invincible $D/s27.replay > $D/report.txt; rc=$?
cat $D/report.txt | tail -4
# N49c: the same checks in the program without the Z80 (the readable rules only; every routine runs as native code, a missing one aborts)
g++ -std=c++17 -O2 -DPAC_NO_Z80 -Itests/oracle -Isrc tests/test_cheat_invincible.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_cheat_invincible_nz 2>&1 | grep -E "error" && { echo "FAIL: build (no Z80)"; exit 1; }
$D/test_cheat_invincible_nz $D/s27.replay > $D/report_nz.txt; rc2=$?
tail -3 $D/report_nz.txt
[ $rc2 -eq 0 ] || { echo "FAIL test_cheat_invincible.sh (no Z80)"; exit 1; }
[ $rc -eq 0 ] && echo "PASS test_cheat_invincible.sh" || { echo "FAIL test_cheat_invincible.sh"; exit 1; }
