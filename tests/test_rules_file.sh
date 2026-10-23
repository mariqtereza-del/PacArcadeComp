#!/bin/sh
# T02i: the rules file and the RULES setting.
#  1. tests/test_rules_file.cpp: the reader (bad lines reported and ignored), the numbers put into the level, the points and the extra life, and a changed game on the machine
#  2. the window program plays the S27 replay (7,273 frames) with --fast and writes the frame-hash log:
#     a. RULES ORIGINAL (nothing set) = the Python prototype            b. --rules readable (no file) = the prototype
#     c. the standard rules file (written by --write-rules-file) = the prototype, and the log says the values are standard, not MODDED
#     d. a test mod (ghosts at 60 percent) DIFFERS from the prototype, the log says MODDED, and the modded game saves no high score (the same replay unmodded does)
#     e. Purist mode ignores the mod file: the log equals the prototype      f. a file of only wrong lines: every line is reported and the hashes equal the prototype
#  3. the settings file keeps `rules` (round trip), and the menu has the RULES row (tests/test_menu.sh and tests/test_integration.sh check the rest)
# Usage: sh tests/test_rules_file.sh [FRAMES]   (default 60000 frames for part 1; needs roms/pacman.zip, g++, SDL2 and Xvfb)
cd "$(dirname "$0")/.." || exit 2
N=${1:-60000}
T=build/t02i; mkdir -p $T/c
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM"; exit 2; }
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Itests tests/test_rules_file.cpp src/board/machine.cpp src/board/bus.cpp -o $T/test_rules_file || { echo "FAIL: build test"; exit 1; }
$T/test_rules_file build/rom/prog.bin "$N" || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
python3 tools/analysis/oracle_replay.py --make $T/script.replay 7273 >/dev/null && python3 tools/analysis/oracle_replay.py --hashes $T/script.replay $T/py.log >/dev/null || { echo "FAIL: prototype"; exit 1; }
run() { n=$1; shift; rm -f $T/hs_$n.txt; timeout 300 build/app/pacman-port --replay $T/script.replay --hashlog $T/$n.log --fast --config $T/c/controls.cfg --hiscore $T/hs_$n.txt "$@" > $T/$n.out 2>&1 || { echo "FAIL: run $n"; exit 1; }; }
equal() { cmp -s $T/$1.log $T/py.log || { echo "FAIL: $2 (differs from the prototype at line $(cmp $T/$1.log $T/py.log | sed -n 's/.*line \([0-9]*\).*/\1/p'))"; exit 1; }; }
run orig
equal orig "RULES ORIGINAL"
grep -q "Game rules: original ROM routines" $T/orig.out || { echo "FAIL: the original run did not say original ROM routines"; exit 1; }
run readable --rules readable
equal readable "RULES READABLE without a file"
build/app/pacman-port --write-rules-file $T/standard.cfg >/dev/null || { echo "FAIL: --write-rules-file"; exit 1; }
[ "$(grep -c '^[a-z]' $T/standard.cfg)" -eq 24 ] || { echo "FAIL: the standard file should have 24 value lines"; exit 1; }
run standard --rules-file $T/standard.cfg
equal standard "the standard rules file"
grep -q "all values standard" $T/standard.out || { echo "FAIL: the standard file was not recognised as standard"; exit 1; }
grep -q "MODDED" $T/standard.out && { echo "FAIL: the standard file was called MODDED"; exit 1; }
# the test mod: slower ghosts (the example for the guide) and a different dot value
printf '# test mod: slower ghosts\nghost_speed_percent 60\n' > $T/slow_ghosts.cfg
run slow --rules-file $T/slow_ghosts.cfg
if cmp -s $T/slow.log $T/py.log; then echo "FAIL: the slow-ghost mod did not change the game"; exit 1; fi
L=$(cmp $T/slow.log $T/py.log | sed -n 's/.*line \([0-9]*\).*/\1/p')
grep -q "MODDED" $T/slow.out || { echo "FAIL: the mod run did not say MODDED"; exit 1; }
[ -f $T/hs_orig.txt ] && grep -q "high_score = [1-9]" $T/hs_orig.txt || { echo "FAIL: the unmodded run should have saved a high score (the proof needs it)"; exit 1; }
[ ! -f $T/hs_slow.txt ] || ! grep -q "high_score = [1-9]" $T/hs_slow.txt || { echo "FAIL: the modded game saved a high score"; exit 1; }
grep -q "ASSISTED: this game played with modified rules" $T/slow.out || { echo "FAIL: the modded game was not called ASSISTED / modified rules in the log"; exit 1; }
run purist --purist --rules-file $T/slow_ghosts.cfg
equal purist "Purist mode with a mod file (it must ignore it)"
printf 'nonsense\nghost_speed_percent 5000\ndot_points 15\nunknown 1\n' > $T/bad.cfg
run bad --rules-file $T/bad.cfg
equal bad "a file of only wrong lines"
[ "$(grep -c 'ignored)' $T/bad.out)" -eq 4 ] || { echo "FAIL: the four wrong lines were not all reported"; grep ignored $T/bad.out; exit 1; }
# the settings file keeps the choice
echo "PASS test_rules_file: reader and numbers checked; original, readable and the standard file give the prototype's $(wc -l < $T/orig.log) frame hashes; the slow-ghost mod differs from frame $L and is marked MODDED (no high score saved); Purist ignores the file; wrong lines are reported"
