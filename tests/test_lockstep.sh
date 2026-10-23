#!/bin/sh
# N08: the shared lockstep harness (tests/lockstep.h). Runs the T02a test (tests/test_rules_random.cpp, now a user of the harness) and the coverage players A and B
# through the harness, and writes the report with the progress meter to build/n08/lockstep_report.txt.
# Usage: sh tests/test_lockstep.sh [FRAMES] [COVERAGE_FRAMES]   (default 100000 and 30000: about 3 minutes; the N08 evidence run used 1,000,000 frames)
cd "$(dirname "$0")/.." || exit 2
N=${1:-100000}; C=${2:-30000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n08; mkdir -p $D build/t02
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/test_rules_random.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/t02/test_rules_random || { echo "FAIL: build"; exit 1; }
build/t02/test_rules_random build/rom/prog.bin "$N" "$C" > $D/lockstep_report.txt
rc=$?
cat $D/lockstep_report.txt
[ $rc -eq 0 ] || { echo "FAIL: the lockstep test failed"; exit 1; }
grep -q "^PASS test_rules_random" $D/lockstep_report.txt || { echo "FAIL: no PASS line"; exit 1; }
grep -q "^part 3: a wrong adapter (seed \* 5 + 3): caught" $D/lockstep_report.txt || { echo "FAIL: the deliberately wrong routine was not caught"; exit 1; }
grep -q "coverage players in lockstep: identical on every frame" $D/lockstep_report.txt || { echo "FAIL: the coverage players are not identical in lockstep"; exit 1; }
[ "$(grep -c '^VARIANT .*IDENTICAL' $D/lockstep_report.txt)" -ge 3 ] || { echo "FAIL: fewer than three variants were identical"; exit 1; }
grep -q "^PROGRESS " $D/lockstep_report.txt || { echo "FAIL: no progress meter line"; exit 1; }
echo "PASS test_lockstep: the T02a result on the shared harness, a wrong routine caught, the coverage players in lockstep; report in $D/lockstep_report.txt"
