#!/bin/sh
# X09: the light bar. Part 1: unit test of src/app/lightbar.h. Part 2: the real program with pretend pads (SDL virtual joystick; only the pretend PS4 and PS5 have a SetLED callback):
# they must be told yellow while playing, dim blue in the menu, yellow again, and the neutral blue on exit; with the option off the bar is put out; an Xbox pretend pad and Purist get nothing.
cd "$(dirname "$0")/.." || exit 2
D=build/x09; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_lightbar.cpp -o $D/test_lightbar || { echo "FAIL: build"; exit 1; }
$D/test_lightbar | tail -2 || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
seq_of() { grep -o '^Light bar: [0-9,]*' $1 | sed 's/Light bar: //' | tr '\n' ' '; }
rm -rf $D/run; mkdir -p $D/run
run() {   # name, extra args...
  n=$1; shift; mkdir -p $D/run/$n; [ -n "$SETTINGS" ] && printf '%s\n' "$SETTINGS" > $D/run/$n/settings.cfg
  build/app/pacman-port --no-audio --scale 2 --config $D/run/$n/controls.cfg --report $D/run/$n.txt --hiscore $D/hs.txt "$@" >/dev/null 2>&1
}
MENU='--pad-buttons 2000:rightstick,4000:rightstick'      # menu opens at 2 s and closes at 4 s
for t in ps4 ps5; do SETTINGS="" run $t --virtual-type $t $MENU --ticks 420
  chk "$t: dark (no game yet), dim blue, dark, neutral blue on exit" "$(seq_of $D/run/$t.txt)" "0,0,0 0,0,48 0,0,0 0,32,255 "
  chk "$t: the pad itself was told the neutral blue last" "$(grep -o 'light bar received.*' $D/run/$t.txt | tr -d '\r')" "light bar received: 4 colour commands, last 0,32,255"; done
SETTINGS="lightbar off" run off --virtual-type ps4 $MENU --ticks 420
chk "option off: the bar is put out (black), neutral blue on exit" "$(seq_of $D/run/off.txt)" "0,0,0 0,32,255 "
SETTINGS="" run xbox --virtual-type xbox360 $MENU --ticks 420
chk "Xbox pretend pad: no light bar command" "$(seq_of $D/run/xbox.txt)" ""
chk "Xbox pretend pad: the pad received none" "$(grep -o 'light bar received: [0-9]* ' $D/run/xbox.txt)" "light bar received: 0 "
SETTINGS="" run pur --purist --virtual-type ps4 $MENU --ticks 420
chk "Purist: the bar is never touched" "$(seq_of $D/run/pur.txt)" ""
chk "Purist: the pad received none" "$(grep -o 'light bar received: [0-9]* ' $D/run/pur.txt)" "light bar received: 0 "
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
