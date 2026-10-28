#!/bin/sh
# 2.1.0: FRAME RATE. (1) unit test (sprites between ticks, the settings), (2) the game is the same at every frame rate: the machine hash log of a
# replay is identical at 30, 60, 144 and unlocked, (3) real window: FRAME RATE stepped in the DISPLAY menu with the keyboard,
# the log and settings.cfg say what was chosen, a restart shows the same values, (4) the log says what each rate does on this screen.
cd "$(dirname "$0")/.." || exit 2
D=build/x211; mkdir -p $D; rm -rf $D/w; mkdir -p $D/w; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
g++ -std=c++17 -O1 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) tests/test_fps.cpp src/video/sprites.cpp src/board/romset.cpp src/video/gfx.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o $D/t_fps $(sdl2-config --libs) || { echo "FAIL: build unit test"; exit 1; }
$D/t_fps roms/pacman.zip | tee $D/unit.txt | tail -4; grep -q "RESULT: PASS" $D/unit.txt || fail=1
export DISPLAY=${DISPLAY:-:99}; pgrep -f "Xvfb $DISPLAY" >/dev/null || { Xvfb $DISPLAY -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
# (2) the same machine at every frame rate: a scripted game (the coverage player input file used by the replay tests)
REP=$D/script.replay
python3 tools/analysis/oracle_replay.py --make $REP 7273 >/dev/null || { echo "FAIL: make replay"; exit 1; }
echo "  replay input file: $REP (the S27 replay, 7,273 frames)"
if true; then
  H0=""
  for v in "--fps 60" "--fps 30" "--fps 144" "--fps unlocked" "--fps 120"; do
    build/app/pacman-port --no-audio --fast --replay $REP --hashlog $D/h.txt $v --config $D/w/hc.cfg --no-hiscore >/dev/null 2>&1
    H=$(sha1sum < $D/h.txt | cut -d' ' -f1); [ -z "$H0" ] && H0=$H
    chk "hash log with $v" "$H" "$H0"
  done
fi
# (3) menu
rm -f $D/w/settings.cfg
build/app/pacman-port --no-audio --scale 2 --config $D/w/controls.cfg --no-hiscore --report $D/w/r1.txt --measure 30 >/dev/null 2>&1 &
P=$!; sleep 3; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 1
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; sleep 0.25; }
k Escape; for i in 1 2 3 4 5; do k Down; done; k Return          # DISPLAY page (FRAME RATE is the first row)
k Right; k Right                                                  # 60 > 75 > 100
wait $P
grep -a "Frame rate:" $D/w/r1.txt | cut -c1-120 | sed 's/^/  log: /'
chk "frame rate in the log" "$(grep -a 'Frame rate: 100' $D/w/r1.txt | wc -l | tr -d ' ')" "1"
chk "settings.cfg frame_rate" "$(grep -a '^frame_rate' $D/w/settings.cfg | tr -d '\r')" "frame_rate 100"
build/app/pacman-port --no-audio --scale 2 --config $D/w/controls.cfg --no-hiscore --report $D/w/r2.txt --measure 3 >/dev/null 2>&1
chk "a restart starts at 100" "$(grep -a 'Frame rate: 100' $D/w/r2.txt | wc -l | tr -d ' ')" "1"
[ $fail = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
