#!/bin/sh
# S34: Purist mode proof. The window program plays the S27 replay (7,273 frames: a one-player game, then a two-player game) with --fast, and writes the per-frame hash log.
#  1. --purist run == the Python prototype's log, line for line (the S27 prototype hashes)
#  2. a normal run with no saved high score also equals the prototype (the extras change nothing on their own)
#  3. a normal run with a saved high score of 5000 must DIFFER (the proof can see an addition) and the Purist run must not touch that file
# Needs roms/pacman.zip, g++, SDL2 and a display (Xvfb :99). About 1 minute.
cd "$(dirname "$0")/.." || exit 2
T=build/s34; mkdir -p $T/hs
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM"; exit 2; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
python3 tools/analysis/oracle_replay.py --make $T/script.replay 7273 >/dev/null && python3 tools/analysis/oracle_replay.py --hashes $T/script.replay $T/py.log >/dev/null || { echo "FAIL: prototype"; exit 1; }
HS=$T/hs/highscore.txt; printf '# test\nhigh_score = 5000\n' > $HS; cp $HS $T/hs/before.txt
run() { n=$1; shift; timeout 300 build/app/pacman-port --replay $T/script.replay --hashlog $T/$n.log --fast --config $T/hs/controls.cfg --hiscore $HS "$@" > $T/$n.out 2>&1 || { echo "FAIL: run $n"; exit 1; }; }
run purist --purist
cmp -s $HS $T/hs/before.txt || { echo "FAIL: the Purist run changed the high score file"; exit 1; }
cmp -s $T/purist.log $T/py.log || { echo "FAIL: Purist run differs from the prototype at line $(cmp $T/purist.log $T/py.log | sed -n 's/.*line \([0-9]*\).*/\1/p')"; exit 1; }
run plain_nohs --no-hiscore
cmp -s $T/plain_nohs.log $T/py.log || { echo "FAIL: normal run without a saved score differs from the prototype"; exit 1; }
run plain_hs
if cmp -s $T/plain_hs.log $T/py.log; then echo "FAIL: a saved high score changed nothing, so this proof could not see an addition"; exit 1; fi
L=$(cmp $T/plain_hs.log $T/py.log | sed -n 's/.*line \([0-9]*\).*/\1/p')
printf '# test\nhigh_score = 5000\n' > $HS
echo "PASS: Purist run: $(wc -l < $T/purist.log) frame hashes identical to the Python prototype and the high score file untouched; normal run without a saved score identical; normal run with a saved score of 5000 differs from frame $L (so the extras are real and Purist really removes them)"
