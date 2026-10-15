#!/bin/sh
# X40: CRT shadow mask. (1) unit test of the pattern and brightness average, (2) real-window screenshots at three sizes, light and strong, compared with the unmasked picture of the
# same replay tick, (3) Purist mode shows no mask, (4) the replay hashes are identical with the mask on and off, (5) speed at full screen 1920x1080 and 2560x1440 (measured, mask off vs strong).
cd "$(dirname "$0")/.." || exit 2
D=build/x40; mkdir -p $D; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
g++ -std=c++17 -O1 -o $D/t_crt tests/test_crt.cpp && $D/t_crt || { echo "FAIL: unit test"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
pgrep -f "Xvfb :98" >/dev/null || { Xvfb :98 -screen 0 2600x1500x24 >/dev/null 2>&1 & sleep 1; }
A="build/app/pacman-port --no-audio --replay $D/s27.replay --config $D/c.cfg --no-hiscore --ticks 900"
for s in 2 3 5; do
  DISPLAY=:98 timeout 60 $A --scale $s --crt-mask off --shot $D/s${s}_off.png >/dev/null 2>&1
  for l in light strong; do
    DISPLAY=:98 timeout 60 $A --scale $s --crt-mask $l --shot $D/s${s}_$l.png >/dev/null 2>&1
    echo "scale $s, $l:"; python3 tests/crt_shot_check.py $D/s${s}_off.png $D/s${s}_$l.png $l | sed 's/^/  /' | grep -a "size\|whole\|RESULT"
    python3 tests/crt_shot_check.py $D/s${s}_off.png $D/s${s}_$l.png $l >/dev/null || fail=1
  done
done
DISPLAY=:98 timeout 60 $A --scale 3 --crt-mask strong --purist --shot $D/purist.png >/dev/null 2>&1
chk "Purist picture identical to the unmasked picture even with --crt-mask strong" "$(cmp -s $D/purist.png $D/s3_off.png && echo same || echo different)" same
chk "mask changes the picture when not Purist" "$(cmp -s $D/s3_strong.png $D/s3_off.png && echo same || echo different)" different
H="build/app/pacman-port --replay $D/s27.replay --fast --config $D/c.cfg --no-hiscore"
DISPLAY=:98 timeout 200 $H --hashlog $D/h_off.log >/dev/null 2>&1; DISPLAY=:98 timeout 200 $H --crt-mask strong --hashlog $D/h_strong.log >/dev/null 2>&1
chk "hash log lines (mask off)" "$(wc -l < $D/h_off.log)" 7273
chk "replay hash log identical with the strong mask on" "$(cmp -s $D/h_off.log $D/h_strong.log && echo same || echo different)" same
# speed
for res in 1920x1080 2560x1440; do
  rm -f /tmp/.X97-lock /tmp/.X11-unix/X97; Xvfb :97 -screen 0 ${res}x24 >/dev/null 2>&1 & XP=$!; sleep 1
  for m in off strong; do
    DISPLAY=:97 build/app/pacman-port --no-audio --crt-mask $m --measure 12 --report $D/sp_${res}_$m.txt --config $D/c.cfg --no-hiscore >/dev/null 2>&1 &
    P=$!; sleep 2; W=$(DISPLAY=:97 xdotool search --pid $P | head -1); DISPLAY=:97 xdotool windowfocus $W; DISPLAY=:97 xdotool keydown F11; sleep 0.2; DISPLAY=:97 xdotool keyup F11
    wait $P
    chk "$res mask $m: ran full screen" "$(grep -a -c 'full screen on' $D/sp_${res}_$m.txt)" 1
    echo "  $res mask $m: $(grep -a 'time between presents' $D/sp_${res}_$m.txt)"
  done
  kill $XP 2>/dev/null; wait $XP 2>/dev/null; sleep 2
done
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
