#!/bin/sh
# X46: Surround Ambient. (1) unit test of the averaging grid, (2) real windows at 21:9 (2560x1080) and 32:9 (3840x1080) and 16:9: the sides show a dark blurred glow of the picture, Black stays black,
# Purist stays black, the picture's place and size are the same in every mode, (3) frame-time comparison Black / Ambient, (4) menu cycle Black > Cabinet > Ambient > Black and settings.cfg, (5) replay hashes identical.
cd "$(dirname "$0")/.." || exit 2
D=build/x46; mkdir -p $D; rm -rf $D/w; mkdir -p $D/w; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
g++ -std=c++17 -O1 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I src -I src/third_party/miniz -o $D/t_amb tests/test_ultrawide.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o || { echo "FAIL: build"; exit 1; }
$D/t_amb || { echo "FAIL: unit test"; fail=1; }
python3 tools/analysis/oracle_replay.py --make $D/s.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
for res in 2560x1080 3840x1080 1920x1080; do
  W0=${res%x*}; H0=${res#*x}; echo "window $res:"
  for m in ambient black purist; do
    case $m in purist) OPT="--purist --surround ambient";; *) OPT="--surround $m";; esac
    rm -f $D/w/settings.cfg
    for xp in $(pgrep -x Xvfb); do tr '\0' ' ' < /proc/$xp/cmdline | grep -q ':95' && kill $xp; done; rm -f /tmp/.X95-lock /tmp/.X11-unix/X95; sleep 1
    ( Xvfb :95 -screen 0 ${res}x24 >/dev/null 2>&1 & echo $! > $D/w/xp ); sleep 2
    DISPLAY=:95 python3 tests/cpu_run.py $D/w/${res}_$m.time build/app/pacman-port --no-audio --scale 2 $OPT --replay $D/s.replay --config $D/w/c.cfg --no-hiscore --report $D/w/${res}_$m.txt --measure 20 &
    P=$!; WID=; for i in $(seq 1 20); do sleep 0.5; WID=$(DISPLAY=:95 xdotool search --onlyvisible --name "." 2>/dev/null | head -1); [ -n "$WID" ] && break; done
    DISPLAY=:95 xdotool windowmove $WID 0 0; DISPLAY=:95 xdotool windowsize $WID $W0 $H0; sleep 13
    DISPLAY=:95 import -window root $D/w/${res}_$m.png; wait $P; kill $(cat $D/w/xp) 2>/dev/null; sleep 1
  done
  LAY=$(grep -a "^Layout: window ${W0}x${H0}," $D/w/${res}_ambient.txt | tail -1); echo "  $LAY"
  chk "layout line found" "$(test -n "$LAY" && echo yes || echo no)" yes
  chk "picture place and size identical in Black" "$(grep -a "^Layout: window ${W0}x${H0}," $D/w/${res}_black.txt | tail -1)" "$LAY"
  chk "picture place and size identical in Purist" "$(grep -a "^Layout: window ${W0}x${H0}," $D/w/${res}_purist.txt | tail -1)" "$LAY"
  R=$(echo "$LAY" | sed 's/.*picture \([0-9]*\),\([0-9]*\) \([0-9]*\)x\([0-9]*\).*/\1 \2 \3 \4/')
  for m in ambient black purist; do MM=$m; [ $m = purist ] && MM=black; python3 tests/ultra_check.py $D/w/${res}_$m.png $W0 $H0 $R $MM | sed "s/^/  $m: /" | grep -a "samples\|RESULT\|FAIL\|step"; python3 tests/ultra_check.py $D/w/${res}_$m.png $W0 $H0 $R $MM >/dev/null || fail=1; done
  echo "  CPU seconds (user system) over 20 s: black $(cat $D/w/${res}_black.time | tail -1), ambient $(cat $D/w/${res}_ambient.time | tail -1)"
  echo "  $(grep -a 'Measured tick rate' $D/w/${res}_ambient.txt) (ambient); $(grep -a 'Measured tick rate' $D/w/${res}_black.txt) (black)"
done
# menu cycle (window on the test display :99)
export DISPLAY=:99; xdpyinfo >/dev/null 2>&1 || { rm -f /tmp/.X99-lock /tmp/.X11-unix/X99; ( Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & ); sleep 2; }
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; sleep 0.25; }
build/app/pacman-port --no-audio --scale 2 --config $D/w/m.cfg --no-hiscore --report $D/w/rm.txt --measure 24 >/dev/null 2>&1 &
P=$!; sleep 3; WID=$(xdotool search --pid $P | head -1); xdotool windowfocus $WID; sleep 1
k Escape; for i in 1 2 3 4 5; do k Down; done; k Return; for i in 1 2 3 4 5 6 7 8 9 10 11 12; do k Down; done; k Right; k Right; k Right
wait $P
chk "menu: three steps = cabinet, ambient, black" "$(grep -a 'Display: surround' $D/w/rm.txt | sed 's/Display: surround //' | tr '\n' ' ')" "cabinet ambient black "
chk "settings.cfg holds surround black after the cycle" "$(grep -a -c '^surround black' $D/w/settings.cfg)" 1
printf 'surround ambient\n' > $D/w/a.dir.tmp; mkdir -p $D/w/a; printf 'surround ambient\n' > $D/w/a/settings.cfg
build/app/pacman-port --no-audio --scale 2 --config $D/w/a/c.cfg --no-hiscore --report $D/w/rm2.txt --measure 4 >/dev/null 2>&1
chk "a saved 'surround ambient' is read back" "$(grep -a -c '^Surround: ambient' $D/w/rm2.txt)" 1
H="build/app/pacman-port --replay $D/s.replay --fast --config $D/h.cfg --no-hiscore"
$H --hashlog $D/h_off.log >/dev/null 2>&1; $H --surround ambient --hashlog $D/h_amb.log >/dev/null 2>&1
chk "hash log lines" "$(wc -l < $D/h_off.log)" 7273
chk "replay hash log identical with Ambient on" "$(cmp -s $D/h_off.log $D/h_amb.log && echo same || echo different)" same
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
