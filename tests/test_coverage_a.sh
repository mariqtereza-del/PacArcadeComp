#!/bin/sh
# N02: coverage player A (tests/coverage_player.h): complete one-player games, deterministic, with a coverage report by routine.
# Usage: sh tests/test_coverage_a.sh [FRAMES]   (default 300000 frames, about 2 minutes: it plays everything twice to prove it is deterministic)
cd "$(dirname "$0")/.." || exit 2
N=${1:-300000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n02; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_coverage_a.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_coverage_a || { echo "FAIL: build"; exit 1; }
$D/test_coverage_a build/rom/prog.bin $N $D/cov_a.txt | tee $D/run.txt || { echo "FAIL: the player is not deterministic"; exit 1; }
[ -f build/n01/cov_replay.txt ] || sh tests/test_rom_map.sh >/dev/null || { echo "FAIL: N01 map"; exit 1; }
python3 tests/rom_map.py build/n01/cov_replay.txt build/n01/cov_player.txt $D/map_check.md --cover A=$D/cov_a.txt --report $D/report_a.txt | tail -4 || { echo "FAIL: report"; exit 1; }
cmp -s $D/map_check.md docs/notes/rom_routines.md || { echo "FAIL: the routine map changed"; exit 1; }
grep -q "^run 1: .* [1-9][0-9]* games started, [1-9][0-9]* lives lost, [1-9][0-9]* game overs" $D/run.txt || { echo "FAIL: no complete game was played"; exit 1; }
echo "PASS test_coverage_a: deterministic complete games; report in $D/report_a.txt (copy kept in docs/evidence/N02_coverage_a.txt)"
