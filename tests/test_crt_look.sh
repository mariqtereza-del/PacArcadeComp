#!/bin/sh
# X43: CRT presets. (1) unit test (table values, matching, stepping, menu value, Purist lock), (2) real window: Esc > DISPLAY > CRT LOOK stepped with the keyboard, the log says which values each preset
# set and settings.cfg holds them, a restart shows them again, (3) the replay hashes are identical with the Old TV look on (the full proof is tests/test_purist.sh and tests/test_replay.sh).
cd "$(dirname "$0")/.." || exit 2
D=build/x43; mkdir -p $D; rm -rf $D/w; mkdir -p $D/w; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
mkdir -p build/s34
g++ -std=c++17 -O1 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) tests/test_crt_look.cpp src/board/romset.cpp src/video/gfx.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o $D/t_look $(sdl2-config --libs) || { echo "FAIL: build"; exit 1; }
$D/t_look || { echo "FAIL: unit test"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep -f "Xvfb $DISPLAY" >/dev/null || { Xvfb $DISPLAY -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; sleep 0.25; }
build/app/pacman-port --no-audio --scale 2 --config $D/w/controls.cfg --no-hiscore --report $D/w/r1.txt --measure 30 >/dev/null 2>&1 &
P=$!; sleep 3; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 1
k Escape; for i in 1 2 3 4 5; do k Down; done; k Return          # Display page
for i in 1 2 3 4 5 6 7 8; do k Down; done                           # to CRT LOOK
k Right; k Right; k Right; k Right; k Left                          # Subtle, Arcade monitor, Old TV, Off, then back to Old TV
wait $P
L=$(grep -a "CRT look" $D/w/r1.txt)
echo "$L" | sed 's/^/  log: /'
chk "first step: SUBTLE (mask light, glow light, curve off, scanlines off)" "$(echo "$L" | grep -a -c 'CRT look SUBTLE (mask light, glow light, curve off, scanlines off)')" 1
chk "second step: ARCADE MONITOR (mask light, glow light, curve off, scanlines on)" "$(echo "$L" | grep -a -c 'CRT look ARCADE MONITOR (mask light, glow light, curve off, scanlines on)')" 1
chk "third step: OLD TV (mask strong, glow strong, curve strong, scanlines off)" "$(echo "$L" | grep -a -c 'CRT look OLD TV (mask strong, glow strong, curve strong, scanlines off)')" 2
chk "fourth step: OFF" "$(echo "$L" | grep -a -c 'CRT look OFF (mask off, glow off, curve off, scanlines off)')" 1
chk "five steps in all" "$(echo "$L" | wc -l)" 5
S=$D/w/settings.cfg
chk "settings.cfg exists" "$(test -s $S && echo yes || echo no)" yes
echo "  settings.cfg: $(grep -a 'crt_mask\|glow\|curve\|scanlines' $S | tr '\n' ' ')"
chk "settings.cfg holds the Old TV look" "$(grep -a -c '^crt_mask strong\|^glow strong\|^curve strong' $S)" 3
build/app/pacman-port --no-audio --scale 2 --config $D/w/controls.cfg --no-hiscore --report $D/w/r2.txt --measure 4 >/dev/null 2>&1
chk "restart: the saved look is back (mask strong, glow strong, curve strong)" "$(grep -a -c 'CRT mask: strong\|Glow: strong\|Curve: strong' $D/w/r2.txt)" 3
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
H="build/app/pacman-port --replay $D/s27.replay --fast --no-hiscore --config $D/h.cfg"
$H --hashlog $D/h_off.log >/dev/null 2>&1; $H --crt-mask strong --glow strong --curve strong --hashlog $D/h_tv.log >/dev/null 2>&1
chk "hash log lines" "$(wc -l < $D/h_off.log)" 7273
chk "replay hash log identical with the Old TV look on" "$(cmp -s $D/h_off.log $D/h_tv.log && echo same || echo different)" same
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
