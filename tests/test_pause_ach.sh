#!/bin/sh
# X27 fix: pausing the game (F1) in the middle of a game must NOT take away the achievements of that game (they were lost before). Real window, real time, S27 replay:
# the game starts, the window is paused for a second in the middle of the game, then the power pellet of the replay is eaten: POWER UP must still unlock.
cd "$(dirname "$0")/.." || exit 2
D=build/x27p; mkdir -p $D; rm -rf $D/w; mkdir -p $D/w
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
build/app/pacman-port --no-audio --scale 2 --replay $D/s27.replay --config $D/w/controls.cfg --no-hiscore --report $D/w/r.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 3; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 9
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; }; k F1; sleep 1.5; k F1; sleep 14
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "the game was paused in the middle" "$(grep -a -c -i 'pause' $D/w/r.txt | awk '{print ($1>0)?"yes":"no"}')" yes
chk "POWER UP still unlocked after the pause" "$(grep -a -c 'Achievement unlocked: POWER UP' $D/w/r.txt)" 1
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
