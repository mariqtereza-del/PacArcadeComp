#!/bin/sh
# X42: curve and vignette. (1) unit test (centre untouched, table symmetric, inverse, vignette), (2) real-window screenshots at three sizes compared with the curve formula, (3) Purist shows none,
# (4) replay hashes identical with the curve on, (5) frame time at full screen 1920x1080 and 2560x1440 (measured), (6) the curve is flat while the menu is open (tests/test_curve_menu.sh).
cd "$(dirname "$0")/.." || exit 2
D=build/x42; mkdir -p $D; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
g++ -std=c++17 -O1 -Wall -Wextra -o $D/t_curve tests/test_curve.cpp && $D/t_curve || { echo "FAIL: unit test"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
pgrep -f "Xvfb :98" >/dev/null || { Xvfb :98 -screen 0 2600x1500x24 >/dev/null 2>&1 & sleep 1; }
A="build/app/pacman-port --no-audio --replay $D/s27.replay --config $D/c.cfg --no-hiscore --ticks 900"
for s in 2 3 5; do
  DISPLAY=:98 timeout 60 $A --scale $s --curve off --shot $D/s${s}_off.png >/dev/null 2>&1
  for l in light strong; do DISPLAY=:98 timeout 60 $A --scale $s --curve $l --shot $D/s${s}_$l.png >/dev/null 2>&1; done
  echo "scale $s:"; python3 tests/curve_shot_check.py $D/s${s}_off.png $D/s${s}_light.png $D/s${s}_strong.png | sed 's/^/  /' | grep -a "size\|RESULT"
  python3 tests/curve_shot_check.py $D/s${s}_off.png $D/s${s}_light.png $D/s${s}_strong.png >/dev/null || fail=1
done
DISPLAY=:98 timeout 60 $A --scale 3 --curve strong --purist --shot $D/purist.png >/dev/null 2>&1
chk "Purist picture identical to the plain picture even with --curve strong" "$(cmp -s $D/purist.png $D/s3_off.png && echo same || echo different)" same
DISPLAY=:98 timeout 60 $A --scale 3 --curve strong --glow strong --crt-mask strong --shot $D/all.png >/dev/null 2>&1
chk "curve + glow + mask together make a picture (file written)" "$(test -s $D/all.png && echo yes || echo no)" yes
H="build/app/pacman-port --replay $D/s27.replay --fast --config $D/c.cfg --no-hiscore"
DISPLAY=:98 timeout 200 $H --hashlog $D/h_off.log >/dev/null 2>&1; DISPLAY=:98 timeout 200 $H --curve strong --hashlog $D/h_strong.log >/dev/null 2>&1
chk "hash log lines" "$(wc -l < $D/h_off.log)" 7273
chk "replay hash log identical with the strong curve on" "$(cmp -s $D/h_off.log $D/h_strong.log && echo same || echo different)" same
for res in 1920x1080 2560x1440; do
  rm -f /tmp/.X97-lock /tmp/.X11-unix/X97; Xvfb :97 -screen 0 ${res}x24 >/dev/null 2>&1 & XP=$!; sleep 2
  for m in off light strong; do
    DISPLAY=:97 build/app/pacman-port --no-audio --curve $m --measure 12 --report $D/sp_${res}_$m.txt --config $D/c.cfg --no-hiscore >/dev/null 2>&1 &
    P=$!; sleep 2; W=$(DISPLAY=:97 xdotool search --pid $P | head -1); DISPLAY=:97 xdotool windowfocus $W; DISPLAY=:97 xdotool keydown F11; sleep 0.2; DISPLAY=:97 xdotool keyup F11
    wait $P
    chk "$res curve $m: ran full screen" "$(grep -a -c 'full screen on' $D/sp_${res}_$m.txt)" 1
    echo "  $res curve $m: $(grep -a 'time between presents' $D/sp_${res}_$m.txt)"
  done
  kill $XP 2>/dev/null; wait $XP 2>/dev/null; sleep 2
done
DISPLAY=:99 sh tests/test_curve_menu.sh 2>&1 | tail -9; DISPLAY=:99 sh tests/test_curve_menu.sh >/dev/null 2>&1 || { echo "  FAIL: curve menu test"; fail=1; }
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
