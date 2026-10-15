#!/bin/sh
# X24, part 2 (the real program in a window, Xvfb + xdotool): the test aid --test-clear pretends levels were cleared (the machine is not touched). Checks: first clear = RECORD SET, better = NEW BEST,
# worse = nothing, records.cfg content, the banner on the picture, the Records page (Esc > RECORDS) after a restart, Purist never writes records.
cd "$(dirname "$0")/.." || exit 2
D=build/x24w; mkdir -p $D
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
k() { xdotool key $1; sleep ${2:-0.35}; }
rm -rf $D/a $D/p; mkdir -p $D/a $D/p
build/app/pacman-port --no-audio --scale 2 --config $D/a/controls.cfg --report $D/a/r.txt --hiscore $D/a/hs.txt --test-clear "1,1200,3000;1,1500,2500;1,900,3200;4,500,800" --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 5; W=$(xdotool search --pid $P | head -1); FOUND=no; t=0
while [ $t -lt 60 ]; do sleep 0.4; t=$((t+1)); import -window $W $D/a/now.png 2>/dev/null; c=$(convert $D/a/now.png -format '%[hex:p{52,244}]' info: 2>/dev/null)
  if [ "$c" = "000028" ]; then FOUND=yes; cp $D/a/now.png $D/a/banner.png; break; fi; done
sleep 9; kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "the banner appeared on the picture" "$FOUND" yes
R=$D/a/r.txt
chk "clear 1: first record" "$(grep -a -c 'Records: level 1 cleared, score 1200.*first record' $R)" 1
chk "clear 2: NEW BEST score and time" "$(grep -a -c 'Records: level 1 cleared, score 1500.*NEW BEST score and time' $R)" 1
chk "clear 3: no record" "$(grep -a -c 'Records: level 1 cleared, score 900.*no record' $R)" 1
chk "clear 4: level 4 first record" "$(grep -a -c 'Records: level 4 cleared, score 500.*first record' $R)" 1
chk "records.cfg holds the best of level 1" "$(grep -c '^level 1 score 1500 frames 3000$\|^level 1 score 1500 frames 2500$' $D/a/records.cfg)" 1
chk "records.cfg level 1 line: best score 1500, fastest 2500" "$(grep '^level 1 ' $D/a/records.cfg)" "level 1 score 1500 frames 2500"
chk "records.cfg level 4 line" "$(grep '^level 4 ' $D/a/records.cfg)" "level 4 score 500 frames 800"
chk "no other lines" "$(grep -c '^level' $D/a/records.cfg)" 2
# Records page after a restart (the file is read again): Esc, down x6 to RECORDS, Enter
build/app/pacman-port --no-audio --scale 2 --config $D/a/controls.cfg --report $D/a/r2.txt --hiscore $D/a/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 6; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5
k Escape 0.8; for i in 1 2 3 4 5 6; do k Down 0.25; done; k Return 0.6; import -window $W $D/a/records_page.png 2>/dev/null
k Return 0.6; import -window $W $D/a/after_back.png 2>/dev/null
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "the second start loaded records.cfg" "$(grep -a -c 'Records: loaded' $D/a/r2.txt)" 1
# Purist: nothing is written
build/app/pacman-port --no-audio --scale 2 --purist --config $D/p/controls.cfg --report $D/p/r.txt --hiscore $D/p/hs.txt --test-clear "1,1200,3000" --ticks 1500 >/dev/null 2>&1
chk "Purist: no records.cfg" "$([ ! -e $D/p/records.cfg ] && echo none)" none
chk "Purist: no Records line in the log" "$(grep -a -c 'Records: level' $D/p/r.txt)" 0
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
