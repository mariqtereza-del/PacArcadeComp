#!/bin/sh
# X04: the mouse in the menus. Part 1: unit test (pixel -> item mapping at several window sizes, hover, click, right click). Part 2: the real program in a window
# (Xvfb): xdotool moves and clicks the pretend mouse: hover selects, a click opens Controls, a click on BACK returns, a right click closes the menu.
cd "$(dirname "$0")/.." || exit 2
D=build/x04; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra $(sdl2-config --cflags) tests/test_mouse.cpp src/app/display.cpp src/video/gfx.cpp -o $D/test_mouse $(sdl2-config --libs) || { echo "FAIL: build"; exit 1; }
$D/test_mouse || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -f $D/a.txt $D/quicksave.sav
build/app/pacman-port --no-audio --scale 2 --config $D/a.cfg --report $D/a.txt --hiscore $D/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 4; W=$(xdotool search --name "Maze Chase" | head -1)
xdotool key --window $W Escape; sleep 1.5
eval $(xdotool getwindowgeometry --shell $W)       # X, Y, WIDTH, HEIGHT (the window is 352 wide while the menu is open)
S=$(( WIDTH * 1000 / 352 ))                        # window pixels per picture pixel, times 1000
at() { xdotool mousemove $(( X + $1 * S / 1000 )) $(( Y + $2 * S / 1000 )); sleep 0.4; }      # picture coordinates
row() { echo $(( 46 + 14 * $1 + 6 )); }
at 100 $(row 5); sleep 0.1
at 100 $(row 2); at 100 $(row 4)
import -window $W $D/hover_controls.png 2>/dev/null
xdotool click 1; sleep 0.6
import -window $W $D/controls_page.png 2>/dev/null
at 100 $(row 10); xdotool click 1; sleep 0.6
at 100 $(row 5); xdotool click 1; sleep 0.6; xdotool click 3; sleep 0.6
chk "right click on a page goes back (no menu close yet)" "$(xdotool getwindowname $W)" "Maze Chase - MENU"
at 100 $(row 0); xdotool click 3; sleep 1; chk "right click on the main page closes the menu" "$(xdotool getwindowname $W)" "Maze Chase"
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "hover over row 5 (DISPLAY) selected it (at least once)" "$([ $(grep -c 'mouse on item 5 of page 0' $D/a.txt) -ge 1 ] && echo yes)" yes
chk "hover follows the pointer (row 4)" "$(grep -c 'mouse on item 4 of page 0' $D/a.txt)" 1
chk "click opens CONTROLS (page 2)" "$(grep -c 'mouse click on item 0 (page 2)' $D/a.txt)" 1
chk "click on BACK returns to the main page (its cursor is back on CONTROLS, row 4)" "$(grep -c 'mouse click on item 4 (page 0)' $D/a.txt)" 1
chk "pointer shown while the menu was open" "$(grep -c 'mouse pointer shown' $D/a.txt)" 1
chk "pointer hidden again when it closed (and at the start)" "$(grep -c 'mouse pointer hidden' $D/a.txt)" 2
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
