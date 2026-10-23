#!/bin/sh
# N04: coverage player C (tests/coverage_player.h, run_c): clearing round after round so that the three intermission cartoons play, the level 21 plateau
# and level 256 are reached. Deterministic; the report names what players A, B and C together still miss.
# Usage: sh tests/test_coverage_c.sh [FRAMES]   (default 300000 frames, about 3 minutes)
cd "$(dirname "$0")/.." || exit 2
N=${1:-300000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n04; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_coverage_c.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_coverage_c || { echo "FAIL: build"; exit 1; }
$D/test_coverage_c build/rom/prog.bin $N $D/cov_c.txt | tee $D/run.txt || { echo "FAIL: the player is not deterministic"; exit 1; }
[ -f build/n01/cov_replay.txt ] || sh tests/test_rom_map.sh >/dev/null || { echo "FAIL: N01 map"; exit 1; }
[ -f build/n02/cov_a.txt ] || sh tests/test_coverage_a.sh >/dev/null || { echo "FAIL: player A"; exit 1; }
[ -f build/n03/cov_b.txt ] || sh tests/test_coverage_b.sh >/dev/null || { echo "FAIL: player B"; exit 1; }
python3 tests/rom_map.py build/n01/cov_replay.txt build/n01/cov_player.txt $D/map_check.md --cover A=build/n02/cov_a.txt --cover B=build/n03/cov_b.txt --cover C=$D/cov_c.txt --report $D/report_abc.txt | head -6 || { echo "FAIL: report"; exit 1; }
cmp -s $D/map_check.md docs/notes/rom_routines.md || { echo "FAIL: the routine map changed"; exit 1; }
grep -q "highest level 256" $D/run.txt || { echo "FAIL: level 256 was not reached"; exit 1; }
# the three intermission cartoons (routines 2108, 219E and 2297 in the jump table at 0A45) must each have run
for r in 2108 219E 2297; do
  awk -F' [|] ' -v r=$r '$1==r { if ($3+0 > 0) ok=1 } END { exit ok ? 0 : 1 }' $D/report_abc.txt || { echo "FAIL: intermission routine $r did not run"; exit 1; }
done
echo "PASS test_coverage_c: deterministic; levels up to 256; all three intermissions; report in $D/report_abc.txt (copy kept in docs/evidence/N04_coverage_c.txt)"
