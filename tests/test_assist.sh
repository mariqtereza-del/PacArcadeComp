#!/bin/sh
# X03: the shared assisted-game rule. Part 1: unit test of src/app/assist.h. Part 2: the real program in a window (Xvfb): F2 (save) then F4 (load) makes the
# window title say ASSISTED with the reason and the log say why; Purist mode never shows it. Needs the built app, roms/pacman.zip, xdotool.
cd "$(dirname "$0")/.." || exit 2
D=build/x03; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_assist.cpp -o $D/test_assist || { echo "FAIL: build"; exit 1; }
$D/test_assist || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -f $D/quicksave.sav $D/hs.txt $D/a.txt
build/app/pacman-port --no-audio --scale 2 --config $D/a.cfg --report $D/a.txt --hiscore $D/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!
sleep 5; W=$(xdotool search --name "Maze Chase" | head -1)
T0=$(xdotool getwindowname $W); xdotool key --window $W F2; sleep 1; xdotool key --window $W F4; sleep 1
T1=$(xdotool getwindowname $W); sleep 4; T2=$(xdotool getwindowname $W)
xdotool key --window $W Escape; sleep 1; T3=$(xdotool getwindowname $W); xdotool key --window $W Escape; sleep 1; T4=$(xdotool getwindowname $W)
kill $P 2>/dev/null; wait $P 2>/dev/null
chk "title before any help" "$T0" "Maze Chase"
chk "title right after the load shows the loaded notice" "$(echo "$T1" | grep -c 'loaded')" 1
chk "title afterwards says ASSISTED with the reason" "$T2" "Maze Chase - ASSISTED: STATE LOADED"
chk "menu open: title is MENU" "$T3" "Maze Chase - MENU"
chk "menu closed: ASSISTED title back" "$T4" "Maze Chase - ASSISTED: STATE LOADED"
chk "log says why, once" "$(grep -c 'ASSISTED: this game loaded a save state, so it does not count for records.' $D/a.txt)" 1
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
