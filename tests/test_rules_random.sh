#!/bin/sh
# T02a: the readable random numbers IN the game (src/rules/rules.h) against the original ROM routine 2A23. See tests/test_rules_random.cpp.
# Also: the replay test with the readable rules on (frame hashes equal to the original run and to the Python prototype), and with them off.
# Usage: sh tests/test_rules_random.sh [FRAMES]   (default 200000 frames, a few minutes; the T02a evidence run used 3,000,000)
cd "$(dirname "$0")/.." || exit 2
N=${1:-200000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/t02; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Isrc/core tests/test_rules_random.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_rules_random || { echo "FAIL: build"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -DREPLAY_MAIN src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/replay_tool || { echo "FAIL: build replay tool"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/script.replay 7273 >/dev/null || { echo "FAIL: replay script"; exit 1; }
$D/replay_tool build/rom/prog.bin $D/script.replay $D/original.log > /dev/null || { echo "FAIL: replay (original)"; exit 1; }
$D/replay_tool build/rom/prog.bin $D/script.replay $D/readable.log --rules readable > /dev/null || { echo "FAIL: replay (readable)"; exit 1; }
[ -f $D/python.log ] || python3 tools/analysis/oracle_replay.py --hashes $D/script.replay $D/python.log || { echo "FAIL: Python run"; exit 1; }
cmp -s $D/original.log $D/readable.log || { echo "FAIL: replay with the readable rules differs from the original"; exit 1; }
cmp -s $D/python.log $D/readable.log || { echo "FAIL: replay with the readable rules differs from the Python prototype"; exit 1; }
echo "replay: $(wc -l < $D/readable.log) frame hashes with the readable rules = original rules = Python prototype"
# the window program (the real game loop) with --rules readable, and Purist mode ignoring it. Needs SDL2 and a display (Xvfb :99).
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
app() { n=$1; shift; timeout 300 build/app/pacman-port --replay $D/script.replay --hashlog $D/$n.log --fast --no-hiscore --config $D/controls.cfg "$@" > $D/$n.out 2>&1 || { echo "FAIL: window program run $n"; exit 1; }; }
app win_readable --rules readable
grep -q "Game rules: READABLE" $D/win_readable.out || { echo "FAIL: the window program did not switch the readable rules on"; exit 1; }
cmp -s $D/win_readable.log $D/python.log || { echo "FAIL: window program with the readable rules differs from the Python prototype"; exit 1; }
app win_purist --rules readable --purist
grep -q "Game rules: original ROM routines" $D/win_purist.out || { echo "FAIL: Purist mode did not keep the original ROM routines"; exit 1; }
cmp -s $D/win_purist.log $D/python.log || { echo "FAIL: Purist run differs from the Python prototype"; exit 1; }
echo "window program: --rules readable plays the replay with $(wc -l < $D/win_readable.log) frame hashes equal to the prototype; with --purist the log says the original ROM routines run"
$D/test_rules_random build/rom/prog.bin "$N"
