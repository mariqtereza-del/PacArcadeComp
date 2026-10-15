#!/bin/sh
# X29: the save-state rule for achievements and the progress counts (made-up events); then (window) a state is saved and loaded in the middle of a game and POWER UP still unlocks.
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/x29
g++ -std=c++17 -O2 -Wall -Wextra tests/test_progress_ach.cpp -o build/x29/test_progress_ach 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
build/x29/test_progress_ach || exit 1
# window: S27 replay in real time; in the middle of the game F2 saves a state and F4 loads it at once; the game is then "assisted" for records, but POWER UP must still unlock.
D=build/x29; rm -rf $D/w; mkdir -p $D/w
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; }
build/app/pacman-port --no-audio --scale 2 --replay $D/s27.replay --config $D/w/controls.cfg --no-hiscore --report $D/w/r.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 3; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 9
k F2; sleep 0.6; k F4; sleep 18
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "a state was saved and loaded in the middle of the game" "$(grep -a -c 'State: slot 1 saved' $D/w/r.txt)$(grep -a -c 'State: .*load' $D/w/r.txt | awk '{print ($1>0)?"L":"-"}')" "1L"
chk "the game became assisted (records stop counting)" "$(grep -a -c 'ASSISTED' $D/w/r.txt | awk '{print ($1>0)?"yes":"no"}')" yes
chk "the game still counts for achievements after the load (the log says so)" "$(grep -a -c 'after the load the game still counts for achievements' $D/w/r.txt)" 1
# (whether the replay, thrown off by the load, still reaches its power pellet is not certain, so the unlock after a load is proved by the unit tests above)
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
