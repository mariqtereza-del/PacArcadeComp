#!/bin/sh
# X44: surround (background layer around the picture). (1) unit test of the layout rectangles at several window shapes, (2) real windows stretched to the full screen of at 4:3, 16:9 and 21:9 (Xvfb of those sizes):
# the cabinet look fills the area around the picture exactly as computed, black mode stays black, Purist stays black, the picture's place and size are the same in both modes, (3) a window that is exactly
# the picture's size shows the same picture in both modes, (4) Esc > DISPLAY > SURROUND with the keyboard and settings.cfg, (5) replay hashes identical with the cabinet on.
cd "$(dirname "$0")/.." || exit 2
D=build/x44; mkdir -p $D; rm -rf $D/w; mkdir -p $D/w; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
g++ -std=c++17 -O1 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I src -o $D/t_surr tests/test_surround.cpp src/board/bus.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o || { echo "FAIL: build"; exit 1; }
$D/t_surr || { echo "FAIL: unit test"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
for res in 1024x768 1920x1080 2560x1080; do
  pkill -f "Xvfb :96" 2>/dev/null; sleep 1; rm -f /tmp/.X96-lock /tmp/.X11-unix/X96; Xvfb :96 -screen 0 ${res}x24 >/dev/null 2>&1 & XP=$!; sleep 2
  W0=${res%x*}; H0=${res#*x}; echo "window $res:"
  for m in cabinet black purist; do
    case $m in purist) OPT="--purist --surround cabinet";; *) OPT="--surround $m";; esac
    DISPLAY=:96 build/app/pacman-port --no-audio --scale 2 $OPT --config $D/w/c.cfg --no-hiscore --report $D/w/${res}_$m.txt --measure 9 >/dev/null 2>&1 &
    P=$!; sleep 3; WID=$(DISPLAY=:96 xdotool search --pid $P | head -1); DISPLAY=:96 xdotool windowmove $WID 0 0; DISPLAY=:96 xdotool windowsize $WID $W0 $H0; sleep 2
    DISPLAY=:96 import -window root $D/w/${res}_$m.png; wait $P
  done
  LAY=$(grep -a "^Layout: window ${W0}x${H0}," $D/w/${res}_cabinet.txt | tail -1); echo "  $LAY"
  chk "layout line found" "$(test -n "$LAY" && echo yes || echo no)" yes
  chk "picture place and size identical with surround black" "$(grep -a "^Layout: window ${W0}x${H0}," $D/w/${res}_black.txt | tail -1)" "$LAY"
  chk "picture place and size identical in Purist" "$(grep -a "^Layout: window ${W0}x${H0}," $D/w/${res}_purist.txt | tail -1)" "$LAY"
  R=$(echo "$LAY" | sed 's/.*picture \([0-9]*\),\([0-9]*\) \([0-9]*\)x\([0-9]*\).*/\1 \2 \3 \4/')
  python3 tests/surround_shot_check.py $D/w/${res}_cabinet.png $W0 $H0 $R cabinet | sed 's/^/  cabinet: /' | grep -a "sample\|RESULT\|FAIL"; python3 tests/surround_shot_check.py $D/w/${res}_cabinet.png $W0 $H0 $R cabinet >/dev/null || fail=1
  python3 tests/surround_shot_check.py $D/w/${res}_black.png $W0 $H0 $R black | sed 's/^/  black: /' | grep -a "RESULT\|FAIL"; python3 tests/surround_shot_check.py $D/w/${res}_black.png $W0 $H0 $R black >/dev/null || fail=1
  python3 tests/surround_shot_check.py $D/w/${res}_purist.png $W0 $H0 $R black | sed 's/^/  purist: /' | grep -a "RESULT\|FAIL"; python3 tests/surround_shot_check.py $D/w/${res}_purist.png $W0 $H0 $R black >/dev/null || fail=1
  kill $XP 2>/dev/null; wait $XP 2>/dev/null; sleep 1
done
export DISPLAY=${DISPLAY:-:99}; pgrep -f "Xvfb $DISPLAY" >/dev/null || { Xvfb $DISPLAY -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
A="build/app/pacman-port --no-audio --replay $D/s27.replay --config $D/c.cfg --no-hiscore --ticks 900 --scale 3"
timeout 60 $A --surround black --shot $D/e_black.png >/dev/null 2>&1; timeout 60 $A --surround cabinet --shot $D/e_cabinet.png >/dev/null 2>&1
chk "window exactly the picture's size: same picture with the cabinet on" "$(cmp -s $D/e_black.png $D/e_cabinet.png && echo same || echo different)" same
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; sleep 0.25; }
build/app/pacman-port --no-audio --scale 2 --config $D/w/m.cfg --no-hiscore --report $D/w/rm.txt --measure 24 >/dev/null 2>&1 &
P=$!; sleep 3; WID=$(xdotool search --pid $P | head -1); xdotool windowfocus $WID; sleep 1
k Escape; for i in 1 2 3 4 5; do k Down; done; k Return; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do k Down; done; k Right; k Right; k Right; k Right
wait $P
chk "menu: four steps = cabinet, ambient, black, cabinet" "$(grep -a 'Display: surround' $D/w/rm.txt | sed 's/Display: surround //' | tr '\n' ' ')" "cabinet ambient black cabinet "
chk "settings.cfg holds surround cabinet" "$(grep -a -c '^surround cabinet' $D/w/settings.cfg)" 1
build/app/pacman-port --no-audio --scale 2 --config $D/w/m.cfg --no-hiscore --report $D/w/rm2.txt --measure 4 >/dev/null 2>&1
chk "restart: the saved surround is back" "$(grep -a -c '^Surround: cabinet' $D/w/rm2.txt)" 1
H="build/app/pacman-port --replay $D/s27.replay --fast --config $D/h.cfg --no-hiscore"
$H --hashlog $D/h_off.log >/dev/null 2>&1; $H --surround cabinet --hashlog $D/h_cab.log >/dev/null 2>&1
chk "hash log lines" "$(wc -l < $D/h_off.log)" 7273
chk "replay hash log identical with the cabinet surround on" "$(cmp -s $D/h_off.log $D/h_cab.log && echo same || echo different)" same
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
