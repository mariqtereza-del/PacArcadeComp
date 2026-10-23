#!/bin/sh
# N03: coverage player B (tests/coverage_player.h, run_b): ghosts eaten, fruit, power pills, extra lives, two players, other DIP settings. Deterministic;
# the report names what players A and B together still miss. Usage: sh tests/test_coverage_b.sh [FRAMES]   (default 300000 frames, about 3 minutes)
cd "$(dirname "$0")/.." || exit 2
N=${1:-300000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n03; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_coverage_b.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_coverage_b || { echo "FAIL: build"; exit 1; }
$D/test_coverage_b build/rom/prog.bin $N $D/cov_b.txt | tee $D/run.txt || { echo "FAIL: the player is not deterministic"; exit 1; }
[ -f build/n01/cov_replay.txt ] || sh tests/test_rom_map.sh >/dev/null || { echo "FAIL: N01 map"; exit 1; }
[ -f build/n02/cov_a.txt ] || sh tests/test_coverage_a.sh >/dev/null || { echo "FAIL: player A"; exit 1; }
python3 tests/rom_map.py build/n01/cov_replay.txt build/n01/cov_player.txt $D/map_check.md --cover A=build/n02/cov_a.txt --cover B=$D/cov_b.txt --report $D/report_ab.txt | tail -5 || { echo "FAIL: report"; exit 1; }
cmp -s $D/map_check.md docs/notes/rom_routines.md || { echo "FAIL: the routine map changed"; exit 1; }
grep -q "^run 1: .* [1-9][0-9]* games started, [1-9][0-9]* lives lost, [1-9][0-9]* game overs" $D/run.txt || { echo "FAIL: no complete game was played"; exit 1; }
grep -q "^  [1-9][0-9]* ghosts eaten in the scripted meetings, [1-9][0-9]* fruits eaten, [1-9][0-9]* extra lives, [1-9][0-9]* player switches" $D/run.txt || { echo "FAIL: a situation is missing (ghosts, fruit, extra life or player switch)"; exit 1; }
echo "PASS test_coverage_b: deterministic; report in $D/report_ab.txt (copy kept in docs/evidence/N03_coverage_b.txt)"
