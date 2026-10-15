#!/bin/sh
# X41: glow. (1) unit test on known images, (2) real-window screenshots at three sizes: glow only adds light, stays black far from bright parts, light < strong, Purist shows none,
# (3) replay hashes identical with the glow on, (4) frame time at full screen 1920x1080 and 2560x1440 (measured), (5) glow together with the X40 mask.
cd "$(dirname "$0")/.." || exit 2
D=build/x41; mkdir -p $D; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
g++ -std=c++17 -O1 -Wall -Wextra -o $D/t_glow tests/test_glow.cpp && $D/t_glow || { echo "FAIL: unit test"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
pgrep -f "Xvfb :98" >/dev/null || { Xvfb :98 -screen 0 2600x1500x24 >/dev/null 2>&1 & sleep 1; }
A="build/app/pacman-port --no-audio --replay $D/s27.replay --config $D/c.cfg --no-hiscore --ticks 900"
for s in 2 3 5; do
  DISPLAY=:98 timeout 60 $A --scale $s --glow off --shot $D/s${s}_off.png >/dev/null 2>&1
  for l in light strong; do DISPLAY=:98 timeout 60 $A --scale $s --glow $l --shot $D/s${s}_$l.png >/dev/null 2>&1; done
  echo "scale $s:"; python3 tests/glow_shot_check.py $D/s${s}_off.png $D/s${s}_light.png $D/s${s}_strong.png | sed 's/^/  /' | grep -a "size\|RESULT"
  python3 tests/glow_shot_check.py $D/s${s}_off.png $D/s${s}_light.png $D/s${s}_strong.png >/dev/null || fail=1
done
DISPLAY=:98 timeout 60 $A --scale 3 --glow strong --purist --shot $D/purist.png >/dev/null 2>&1
chk "Purist picture identical to the plain picture even with --glow strong" "$(cmp -s $D/purist.png $D/s3_off.png && echo same || echo different)" same
DISPLAY=:98 timeout 60 $A --scale 3 --glow strong --crt-mask strong --shot $D/both.png >/dev/null 2>&1
python3 tests/crt_shot_check.py $D/s3_off.png $D/both.png strong >/dev/null 2>&1; echo "  (glow + mask together written: $D/both.png)"
chk "glow and mask together make a picture different from either alone" "$(cmp -s $D/both.png $D/s3_strong.png && echo same || echo different)" different
# the glow together with the curve (X42): it must sit where it sits without the curve, only bent with the picture (a stretched glow was reported by the user)
for lv in light:0.04 strong:0.09; do l=${lv%%:*}; k=${lv#*:}
  DISPLAY=:98 timeout 60 $A --scale 3 --glow $l --shot $D/f_$l.png >/dev/null 2>&1; DISPLAY=:98 timeout 60 $A --scale 3 --curve $l --shot $D/c_only_$l.png >/dev/null 2>&1; DISPLAY=:98 timeout 60 $A --scale 3 --curve $l --glow $l --shot $D/c_glow_$l.png >/dev/null 2>&1
  echo "glow with curve $l:"; python3 tests/glow_curve_check.py $D/s3_off.png $D/f_$l.png $D/c_only_$l.png $D/c_glow_$l.png $k | head -1; python3 tests/glow_curve_check.py $D/s3_off.png $D/f_$l.png $D/c_only_$l.png $D/c_glow_$l.png $k >/dev/null || fail=1
done
python3 tests/glow_curve_check.py $D/s3_off.png $D/f_strong.png $D/c_only_strong.png $D/c_glow_strong.png 0.09 scale >/dev/null && { echo "  FAIL: self test: a stretched glow was not detected"; fail=1; } || echo "  ok: self test: a glow stretched by 1.57 (the old bug) is detected"
python3 tests/glow_curve_check.py $D/s3_off.png $D/f_strong.png $D/c_only_strong.png $D/c_glow_strong.png 0.09 shift >/dev/null && { echo "  FAIL: self test: a shifted glow was not detected"; fail=1; } || echo "  ok: self test: a glow shifted by 45 pixels is detected"
H="build/app/pacman-port --replay $D/s27.replay --fast --config $D/c.cfg --no-hiscore"
DISPLAY=:98 timeout 200 $H --hashlog $D/h_off.log >/dev/null 2>&1; DISPLAY=:98 timeout 200 $H --glow strong --hashlog $D/h_strong.log >/dev/null 2>&1
chk "hash log lines" "$(wc -l < $D/h_off.log)" 7273
chk "replay hash log identical with the strong glow on" "$(cmp -s $D/h_off.log $D/h_strong.log && echo same || echo different)" same
for res in 1920x1080 2560x1440; do
  rm -f /tmp/.X97-lock /tmp/.X11-unix/X97; Xvfb :97 -screen 0 ${res}x24 >/dev/null 2>&1 & XP=$!; sleep 2
  for m in off light strong; do
    DISPLAY=:97 build/app/pacman-port --no-audio --glow $m --measure 12 --report $D/sp_${res}_$m.txt --config $D/c.cfg --no-hiscore >/dev/null 2>&1 &
    P=$!; sleep 2; W=$(DISPLAY=:97 xdotool search --pid $P | head -1); DISPLAY=:97 xdotool windowfocus $W; DISPLAY=:97 xdotool keydown F11; sleep 0.2; DISPLAY=:97 xdotool keyup F11
    wait $P
    chk "$res glow $m: ran full screen" "$(grep -a -c 'full screen on' $D/sp_${res}_$m.txt)" 1
    echo "  $res glow $m: $(grep -a 'time between presents' $D/sp_${res}_$m.txt)"
  done
  kill $XP 2>/dev/null; wait $XP 2>/dev/null; sleep 2
done
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
