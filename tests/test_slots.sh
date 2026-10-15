#!/bin/sh
# X12: eight save slots. Part 1: unit test of the slot files (header, round trip, old quicksave.sav, picker lines). Part 2: the real program in a window (Xvfb, xdotool): play a bit,
# Esc > SAVE STATE > slot 3, play on, Esc > LOAD STATE > slot 3 (back at the saved moment: same score), an empty slot, F2 / F4 on slot 1, the old quicksave.sav read as slot 1,
# Purist locked. Needs roms/pacman.zip, the built app, xdotool, ImageMagick (import).
cd "$(dirname "$0")/.." || exit 2
D=build/x12; mkdir -p $D
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_slots.cpp src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp src/audio/wsg.cpp \
  build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o $D/test_slots || { echo "FAIL: build"; exit 1; }
$D/test_slots roms/pacman.zip | tail -2 || exit 1
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
k() { xdotool key $1; sleep ${2:-0.35}; }
hold() { xdotool keydown $1; sleep 0.15; xdotool keyup $1; sleep ${2:-0.35}; }      # the game looks at the keys once per frame: a very short tap can be missed
attempt() {      # one run of the window part: returns after the program was killed; the log is $D/w/r.txt
  rm -rf $D/w; mkdir -p $D/w
  build/app/pacman-port --no-audio --scale 2 --config $D/w/c.cfg --report $D/w/r.txt --hiscore $D/w/hs.txt --ticks 10000000 >/dev/null 2>&1 &
  P=$!; sleep 7; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5
  hold 5 1.5; hold 1 0.5; sleep 12; xdotool keydown Left; sleep 4; xdotool keyup Left; sleep 0.3
  # Esc > SAVE STATE (main page row 1) > Enter > slot 3 (two Down) > Enter
  k Escape 0.8; k Down; k Return 0.5; k Down; k Down; k Return 0.6
  import -window $W $D/w/picker_save.png 2>/dev/null
  k Escape; k Escape 0.8
  xdotool keydown Right; sleep 3; xdotool keyup Right; sleep 0.3
  # Esc > LOAD STATE (row 2) > slot 3
  k Escape 0.8; k Down; k Down; k Return 0.5
  import -window $W $D/w/picker_load.png 2>/dev/null
  k Down; k Down; k Return 1.0
  # the empty slot 5: Esc > LOAD STATE > Down x4 > Enter: stays open
  k Escape 0.8; k Down; k Down; k Return 0.5; k Down; k Down; k Down; k Down; k Return 0.6
  import -window $W $D/w/empty.png 2>/dev/null
  eval $(xdotool getwindowgeometry --shell $W); W_EMPTY=$WIDTH      # the window is 352 picture columns wide only while the menu is open (key panel)
  k Escape; k Escape 0.8
  # F2 saves slot 1, F4 loads it
  k F2 0.6; k F4 0.8
  kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
}
for try in 1 2 3; do attempt; if grep -a -q 'State: slot 3 saved (.* L[0-9]* [1-9][0-9]*, lives' $D/w/r.txt; then break; fi; echo "  (try $try: the game had no score yet when slot 3 was saved - the pretend player is not always quick enough; trying again)"; done
R=$D/w/r.txt
chk "slot 3 file written" "$([ -s $D/w/slot3.sav ] && echo yes)" yes
chk "slot 3 file has the header" "$(head -c 4 $D/w/slot3.sav)" PMSL
SAVED=$(grep -m1 'State: slot 3 saved' $R); LOADED=$(grep -m1 'State: slot 3 loaded' $R)
echo "  log: $SAVED"; echo "  log: $LOADED"
SS=$(echo "$SAVED" | sed 's/.* L[0-9]* \([0-9]*\),.*/\1/'); LS=$(echo "$LOADED" | sed 's/.*in the machine now \([0-9]*\);.*/\1/'); LB=$(echo "$LOADED" | sed 's/.*score before the load \([0-9]*\),.*/\1/')
chk "the saved score is above 0 (the game was really played)" "$([ "${SS:-0}" -gt 0 ] && echo yes)" yes
chk "after loading slot 3 the machine has the saved score" "$LS" "$SS"
LIV_S=$(echo "$SAVED" | sed 's/.*lives \([0-9]*\))/\1/'); LIV_B=$(echo "$LOADED" | sed 's/.*lives before the load \([0-9]*\),.*/\1/'); LIV_N=$(echo "$LOADED" | sed 's/.*lives before the load [0-9]*, in the machine now \([0-9]*\))/\1/')
chk "after loading slot 3 the machine has the saved number of lives" "$LIV_N" "$LIV_S"
chk "and the game had moved on before the load (more points or fewer lives: before $LB points / $LIV_B lives, saved $SS / $LIV_S)" "$([ "${LB:-0}" -gt "${SS:-0}" ] || [ "${LIV_B:-9}" -lt "${LIV_S:-0}" ] && echo yes)" yes
chk "the load was marked as assisted" "$(grep -ci 'assist' $R | awk '{print ($1>0)?"yes":"no"}')" yes
chk "loading the empty slot 5 says so in the log" "$([ $(grep -c 'State: slot 5 is empty' $R) -ge 1 ] && echo yes)" yes
chk "the menu stayed open after the empty slot (the window is wide: the key panel is showing)" "$([ $W_EMPTY -gt 600 ] && echo open)" open
chk "slot 5 was not created" "$([ ! -e $D/w/slot5.sav ] && echo none)" none
chk "F2 wrote slot 1 (quicksave.sav) with the header" "$(head -c 4 $D/w/quicksave.sav)" PMSL
chk "F4 loaded slot 1" "$(grep -c 'State: slot 1 loaded' $R)" 1
# the old quicksave.sav (no header) is read as slot 1
tail -c +21 $D/w/quicksave.sav > $D/w/old.bin; cp $D/w/old.bin $D/w/quicksave.sav; rm -f $D/w/slot3.sav
build/app/pacman-port --no-audio --scale 2 --config $D/w/c.cfg --report $D/w/r2.txt --hiscore $D/w/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 6; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5; k F4 1; k Escape 0.8; k Down; k Down; k Return 0.5; import -window $W $D/w/picker_old.png 2>/dev/null
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "the old headerless quicksave.sav loads with F4" "$(grep -c 'State: slot 1 loaded (OLD SAVE' $D/w/r2.txt)" 1
# Purist: the picker is locked
build/app/pacman-port --no-audio --scale 2 --purist --config $D/w/c.cfg --report $D/w/r3.txt --hiscore $D/w/hs.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 5; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5; k Escape 0.8; k Down; k Return 0.5; T_P=$(xdotool getwindowname $W); k F2 0.5; k F4 0.5
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "Purist: no slot activity at all" "$(grep -c 'State: slot' $D/w/r3.txt)" 0
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
