#!/bin/sh
# X05: mouse wheel, the Bindings screen and the key panel pin, in the real program in a window (Xvfb) with a pretend mouse and keyboard (xdotool).
# The hit areas themselves are unit tested in tests/test_mouse.sh (182 checks). Needs the built app, roms/pacman.zip, xdotool.
cd "$(dirname "$0")/.." || exit 2
D=build/x05; mkdir -p $D
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -f $D/a.txt $D/a.cfg $D/controls.cfg $D/settings.cfg $D/quicksave.sav
build/app/pacman-port --no-audio --scale 2 --config $D/a.cfg --report $D/a.txt --hiscore $D/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 4; W=$(xdotool search --name "PacArcadeComp" | head -1)
xdotool key --window $W Escape; sleep 1.5
eval $(xdotool getwindowgeometry --shell $W)
S=$(( WIDTH * 1000 / 352 ))
at() { xdotool mousemove $(( X + $1 * S / 1000 )) $(( Y + $2 * S / 1000 )); sleep 0.4; }
mrow() { echo $(( 46 + 14 * $1 + 6 )); }         # menu row, picture y
brow() { echo $(( 46 + 15 * $1 + 4 )); }         # Bindings row, picture y
at 100 $(mrow 0)
xdotool click 5; sleep 0.3; xdotool click 5; sleep 0.3; xdotool click 5; sleep 0.3; xdotool click 4; sleep 0.4        # wheel down x3, up x1
import -window $W $D/wheel_menu.png 2>/dev/null
at 100 $(mrow 4); xdotool click 1; sleep 0.6; at 100 $(mrow 0); xdotool click 1; sleep 0.8              # CONTROLS, then BINDINGS
T1=$(xdotool getwindowname $W)
at 100 $(brow 0); xdotool click 5; sleep 0.3; xdotool click 5; sleep 0.3; xdotool click 4; sleep 0.4        # wheel on the Bindings screen
at 100 $(brow 4); xdotool click 1; sleep 0.5                                                              # click COIN, KEY 1
import -window $W $D/bind_waiting.png 2>/dev/null
xdotool key --window $W 7; sleep 0.6                                                                      # the new key
import -window $W $D/bind_changed.png 2>/dev/null
at 200 $(brow 5); xdotool click 1; sleep 0.5; xdotool click 3; sleep 0.5                                  # START 1 KEY 2: waiting, right click cancels
xdotool click 3; sleep 0.8; T2=$(xdotool getwindowname $W)                                                # right click again: back to the menu
xdotool click 3; sleep 0.6                                                                                # back to the main page
at 300 10; xdotool click 1; sleep 0.6; xdotool click 1; sleep 0.6                                         # the key panel header: pin, unpin
import -window $W $D/after_pin.png 2>/dev/null
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "wheel down x3 then up x1 on the menu ends on item 2" "$(grep 'mouse wheel to item' $D/a.txt | tail -1 | sed 's/.*item \([0-9]*\) of.*/\1/')" 2
chk "wheel moved the menu 4 times" "$(grep -c 'mouse wheel to item' $D/a.txt)" 4
chk "BINDINGS opened by clicking" "$T1" "PacArcadeComp - BINDINGS"
chk "wheel on the Bindings screen (3 steps)" "$(grep -c 'Controls: mouse wheel to row' $D/a.txt)" 3
chk "click on COIN KEY 1 waits for a key" "$(grep -c 'mouse click on row 4, column 0 (waiting for a key)' $D/a.txt)" 1
chk "the pressed key 7 was bound to COIN (log)" "$(grep -c 'Controls: COIN = 7' $D/a.txt)" 1
chk "the controls file has coin = 7" "$(grep -c '^coin = 7' $D/a.cfg)" 1
chk "click on START 1 KEY 2 waits" "$(grep -c 'mouse click on row 5, column 1 (waiting for a key)' $D/a.txt)" 1
chk "right click cancels the wait" "$(grep -c 'mouse right click (cancelled)' $D/a.txt)" 1
chk "right click again goes back (menu)" "$T2" "PacArcadeComp - MENU"
chk "click on the key panel header pinned it" "$(grep -c 'panel pinned' $D/a.txt)" 1
chk "click again unpinned it" "$(grep -c 'panel unpinned' $D/a.txt)" 1
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
