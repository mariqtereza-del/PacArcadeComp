#!/bin/sh
# X45: original cabinet look + own picture. (1) unit test (PNG reader limits, bad files, cabinet geometry), (2) real windows: generated cabinet shows marquee, grille and side panels at 4:3, 16:9, 21:9;
# a made-up surround.png next to the program replaces it; bad files (damaged, too small, interlaced, text) give a log line and fall back to the generated cabinet; Purist and Surround Black ignore the file;
# (3) replay hashes identical with the cabinet and with an own picture.
cd "$(dirname "$0")/.." || exit 2
D=build/x45; mkdir -p $D; rm -rf $D/w $D/app; mkdir -p $D/w $D/app; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
python3 tests/make_cabinet_pngs.py $D/png || exit 1
g++ -std=c++17 -O1 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I src -I src/third_party/miniz -o $D/t_cab tests/test_cabinet.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o || { echo "FAIL: build"; exit 1; }
$D/t_cab $D/png || { echo "FAIL: unit test"; fail=1; }
# a private copy of the program: its own folder is where surround.png is looked for
cp build/app/pacman-port roms/pacman.zip $D/app/
px() { python3 -c "from PIL import Image; import sys; im=Image.open(sys.argv[1]).convert('RGB'); print(*im.getpixel((int(sys.argv[2]),int(sys.argv[3]))))" "$@"; }
pkill -f "Xvfb :96" 2>/dev/null; sleep 1; rm -f /tmp/.X96-lock /tmp/.X11-unix/X96; Xvfb :96 -screen 0 1920x1080x24 >/dev/null 2>&1 & XP=$!; sleep 2
run() {      # run NAME PNGFILE|none EXTRA-OPTIONS  -> $D/w/NAME.txt and NAME.png (window stretched to the whole 1920x1080 screen)
  rm -f $D/app/surround.png $D/w/settings.cfg; [ "$2" != none ] && cp $D/png/$2 $D/app/surround.png
  DISPLAY=:96 $D/app/pacman-port --no-audio --scale 2 $3 --config $D/w/c.cfg --no-hiscore --report $D/w/$1.txt --measure 14 >/dev/null 2>&1 &
  P=$!; WID=; for i in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20; do sleep 0.5; WID=$(DISPLAY=:96 xdotool search --pid $P | head -1); [ -n "$WID" ] && break; done; sleep 1; DISPLAY=:96 xdotool windowmove $WID 0 0; DISPLAY=:96 xdotool windowsize $WID 1920 1080; sleep 2
  DISPLAY=:96 import -window root $D/w/$1.png; wait $P
}
run gen none "--surround cabinet"
chk "no file: log says generated cabinet" "$(grep -a -c '^Surround: no surround.png next to the program' $D/w/gen.txt)" 1
python3 tests/surround_shot_check.py $D/w/gen.png 1920 1080 624 108 672 864 cabinet | grep -a "RESULT\|FAIL"; python3 tests/surround_shot_check.py $D/w/gen.png 1920 1080 624 108 672 864 cabinet >/dev/null || fail=1
run own rgb.png "--surround cabinet"
chk "good picture: log says it is used" "$(grep -a -c '^Surround: using your picture .*(200 x 100)' $D/w/own.txt)" 1
chk "good picture: top-left corner is the picture's red" "$(px $D/w/own.png 3 3)" "200 30 30"
chk "good picture: top-right corner is the picture's blue" "$(px $D/w/own.png 1916 3)" "30 30 200"
chk "good picture: window centre is the game (black), not the picture" "$(px $D/w/own.png 960 500)" "0 0 0"
for bad in corrupt.png small.png interlaced.png deep.png notpng.png trunc.png huge.png; do
  run bad "$bad" "--surround cabinet"
  chk "$bad: log says it could not be used" "$(grep -a -c '^Surround: could not use .*surround.png: .*using the generated cabinet' $D/w/bad.txt)" 1
  chk "$bad: the generated cabinet is shown instead (orange moulding at its place)" "$(px $D/w/bad.png 390 500 | awk '{print ($1>150 && $3<80) ? "orange" : "not orange: "$0}')" orange
done
run pur rgb.png "--purist --surround cabinet"
chk "Purist: the file is not even read" "$(grep -a -c '^Surround: using your picture' $D/w/pur.txt)" 0
chk "Purist: corner stays black" "$(px $D/w/pur.png 3 3)" "0 0 0"
run blk rgb.png "--surround black"
chk "Surround Black: corner stays black" "$(px $D/w/blk.png 3 3)" "0 0 0"
kill $XP 2>/dev/null; wait $XP 2>/dev/null
rm -f $D/app/surround.png
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
H="build/app/pacman-port --replay $D/s27.replay --fast --config $D/h.cfg --no-hiscore"
$H --hashlog $D/h_off.log >/dev/null 2>&1; $H --surround cabinet --hashlog $D/h_cab.log >/dev/null 2>&1
cp $D/png/rgb.png $D/app/surround.png; $D/app/pacman-port --replay $D/s27.replay --fast --config $D/h.cfg --no-hiscore --surround cabinet --hashlog $D/h_own.log >/dev/null 2>&1; rm -f $D/app/surround.png
chk "hash log lines" "$(wc -l < $D/h_off.log)" 7273
chk "replay hashes identical with the generated cabinet" "$(cmp -s $D/h_off.log $D/h_cab.log && echo same || echo different)" same
chk "replay hashes identical with an own picture" "$(cmp -s $D/h_off.log $D/h_own.log && echo same || echo different)" same
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
