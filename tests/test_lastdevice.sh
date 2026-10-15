#!/bin/sh
# X07: which device was used last. Part 1: unit test of src/app/lastdevice.h (made-up events). Part 2: the real program: a pretend keyboard and mouse (xdotool) and a pretend
# PS4 pad (SDL virtual joystick pushing a stick) must give the log lines "Input: the last device used is now ..." in the right order; Purist mode gives none.
cd "$(dirname "$0")/.." || exit 2
D=build/x07; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_lastdevice.cpp -o $D/test_lastdevice || { echo "FAIL: build"; exit 1; }
$D/test_lastdevice || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -f $D/*.txt $D/*.cfg
# keyboard, mouse, keyboard, mouse click
build/app/pacman-port --no-audio --scale 2 --config $D/k.cfg --report $D/k.txt --hiscore $D/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 4; W=$(xdotool search --pid $P | head -1); eval $(xdotool getwindowgeometry --shell $W)
xdotool mousemove $((X+100)) $((Y+100)); sleep 0.5; xdotool mousemove $((X+101)) $((Y+101)); sleep 0.5      # one big move, then jitter of 2 pixels (ignored)
xdotool key --window $W Left; sleep 0.5; xdotool mousemove $((X+150)) $((Y+150)); sleep 0.5; xdotool key --window $W Right; sleep 0.5
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
echo "  log: $(grep 'Input: ' $D/k.txt | sed 's/Input: the last device used is now //' | tr '\n' ',' )"
chk "keyboard / mouse sequence (jitter ignored)" "$(grep 'Input: ' $D/k.txt | sed 's/Input: the last device used is now //' | tr '\n' ' ')" "MOUSE KEYBOARD MOUSE KEYBOARD "
# pretend PS4 pad: a stick pushed beyond the dead zone, then back to rest, then a small push (drift)
build/app/pacman-port --no-audio --scale 2 --config $D/p.cfg --report $D/p.txt --hiscore $D/hs.txt --virtual-type ps4 --pad-virtual "1500:a0=300,2500:a0=20000,3500:a0=0,4500:a0=9000,5500:a0=0" --ticks 700 >/dev/null 2>&1
echo "  log: $(grep 'Input: ' $D/p.txt | sed 's/Input: the last device used is now //' | tr '\n' ',' )"
chk "pad stick pushed: PAD (PS4), once (rest and drift ignored)" "$(grep -c 'Input: the last device used is now PAD (PS4)' $D/p.txt)" 1
# Purist: nothing is tracked
build/app/pacman-port --no-audio --scale 2 --purist --config $D/q.cfg --report $D/q.txt --virtual-type ps4 --pad-virtual "1500:a0=20000" --ticks 400 >/dev/null 2>&1
chk "Purist: no device lines" "$(grep -c 'Input: ' $D/q.txt)" 0
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
