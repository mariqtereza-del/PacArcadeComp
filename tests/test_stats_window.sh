#!/bin/sh
# X22, part 2 (real window, Xvfb + xdotool): play with 1 life and do nothing, so the game ends by itself; the summary page must appear (a picture is taken),
# the log must say so, the numbers on the page must be the ones in the --stats file, any key closes it, the setting RUN SUMMARY off hides it, Purist never shows it.
cd "$(dirname "$0")/.." || exit 2
D=build/x22w; mkdir -p $D
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
hold() { xdotool keydown $1; sleep 0.15; xdotool keyup $1; sleep ${2:-0.35}; }
game() {      # name, extra args: one-life game, nobody moves; picture of the page when the box colour shows at (56,164)
  n=$1; shift; rm -rf $D/$n; mkdir -p $D/$n; printf 'lives 1\n%s\n' "$SETTINGS" > $D/$n/settings.cfg
  build/app/pacman-port --no-audio --scale 2 --config $D/$n/controls.cfg --report $D/$n/r.txt --hiscore $D/$n/hs.txt --stats $D/$n/stats.txt --ticks 10000000 "$@" >/dev/null 2>&1 &
  P=$!; sleep 6; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5
  hold 5 1.5; hold 1 0.5; FOUND=no; t=0
  while [ $t -lt 70 ]; do sleep 1; t=$((t+1)); import -window $W $D/$n/now.png 2>/dev/null
    c=$(convert $D/$n/now.png -format '%[hex:p{56,164}]' info: 2>/dev/null); if [ "$c" = "000028" ]; then FOUND=yes; cp $D/$n/now.png $D/$n/page.png; break; fi; done
  if [ $FOUND = yes ] && [ "$KEYTEST" = 1 ]; then sleep 1; hold Left 0.8; import -window $W $D/$n/after_key.png 2>/dev/null; AK=$(convert $D/$n/after_key.png -format '%[hex:p{56,164}]' info: 2>/dev/null); fi
  kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
}
SETTINGS="" KEYTEST=1 game on
chk "the summary page appeared by itself after the game ended" "$FOUND" yes
chk "the log says the page was shown" "$(grep -a -c 'Stats: game over, summary page shown (counted)' $D/on/r.txt)" 1
chk "a key closed the page" "$(grep -a -c 'Stats: summary page closed' $D/on/r.txt)" 1
chk "the picture no longer shows the page after the key" "$([ "$AK" != "000028" ] && echo gone)" gone
S=$D/on/stats.txt; echo "  stats file: $(grep GAMEOVER $S)"
chk "the stats file has the finished game, counted" "$(grep -c '^GAMEOVER P1 .*counted=1' $S)" 1
chk "its score equals the game's score" "$(grep GAMEOVER $S | awk '{split($5,a,"=");split($6,b,"="); print (a[2]==b[2])?"same":"differs"}')" same
SETTINGS="run_summary off" KEYTEST=0 game off
chk "setting off: no page" "$FOUND" no
chk "setting off: the stats file still has the game (the setting only hides the page)" "$(grep -c '^GAMEOVER' $D/off/stats.txt)" 1
SETTINGS="" KEYTEST=0 game pur --purist
chk "Purist: no page" "$FOUND" no
chk "Purist: no statistics" "$([ ! -s $D/pur/stats.txt ] && echo none)" none
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
