#!/bin/sh
# X06: auto-pause when the window loses focus, in the real program (Xvfb, no window manager). A second copy of the program is the "other window": xdotool gives it the focus.
# Needs the built app, roms/pacman.zip, xdotool, ImageMagick (import).
cd "$(dirname "$0")/.." || exit 2
D=build/x06; mkdir -p $D $D/on $D/off $D/pur $D/other
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -f $D/*/*.txt $D/*/*.cfg $D/*/quicksave.sav; printf 'pause_on_focus_loss off\n' > $D/off/settings.cfg
run() { n=$1; shift; build/app/pacman-port --no-audio --scale 2 --config $D/$n/c.cfg --report $D/$n/r.txt --hiscore $D/$n/hs.txt --ticks 10000000 "$@" >/dev/null 2>&1 & echo $!; }
win() { xdotool search --pid $1 | head -1; }
title() { xdotool getwindowname $1; }
# the other window
PO=$(run other); sleep 3; WO=$(win $PO)
# 1. default (on): lose focus -> paused; coming back stays paused; a key goes on
PA=$(run on); sleep 4; WA=$(win $PA)
xdotool windowfocus $WA; sleep 1; T0=$(title $WA)
xdotool windowfocus $WO; sleep 1; T1=$(title $WA)
import -window $WA $D/paused.png 2>/dev/null
sleep 3; xdotool windowfocus $WA; sleep 1; T2=$(title $WA)
xdotool key --window $WA space; sleep 1; T3=$(title $WA)
kill $PA 2>/dev/null; wait $PA 2>/dev/null; sleep 1
# 2. option off: focus loss does nothing
PB=$(run off); sleep 4; WB=$(win $PB); xdotool windowfocus $WB; sleep 1; xdotool windowfocus $WO; sleep 1; T4=$(title $WB)
# 3. Purist: nothing either
PP=$(run pur --purist); sleep 4; WP=$(win $PP); xdotool windowfocus $WP; sleep 1; xdotool windowfocus $WO; sleep 1; T5=$(title $WP)
# 4. the menu item (Esc, Controls, wheel down to the item) -- keyboard only
kill $PB $PP $PO 2>/dev/null; wait 2>/dev/null; sleep 1
chk "title while in focus" "$T0" "Maze Chase"
chk "focus lost: the game pauses by itself" "$T1" "Maze Chase - PAUSED (PRESS ANY KEY)"
chk "focus back: still paused" "$T2" "Maze Chase - PAUSED (PRESS ANY KEY)"
chk "any key goes on" "$T3" "Maze Chase"
chk "option off: focus loss does not pause" "$T4" "Maze Chase"
chk "Purist: focus loss does not pause" "$T5" "Maze Chase"
chk "log: paused once, went on once" "$(grep -c 'Focus: the window lost focus' $D/on/r.txt)/$(grep -c 'Focus: going on' $D/on/r.txt)" "1/1"
chk "log: no focus pause with the option off" "$(grep -c 'Focus:' $D/off/r.txt)" 0
# the machine really stood still while paused: far fewer ticks than the 60.6 per second of running time
secs=$(sed -n 's/^Ran \([0-9.]*\) s.*/\1/p' $D/on/r.txt | tail -1); ticks=$(sed -n 's/^Ran [0-9.]* s: \([0-9]*\) machine ticks.*/\1/p' $D/on/r.txt | tail -1)
echo "  (run: $secs s, $ticks machine ticks)"
chk "the paused window ran fewer ticks than it would have unpaused" "$(awk -v s="$secs" -v t="$ticks" 'BEGIN{ print (t < s*60.6*0.8) ? "yes" : "no" }')" yes
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
