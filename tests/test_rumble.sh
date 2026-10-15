#!/bin/sh
# S34R: vibration in the real program. The S27 replay (7,273 frames: a one-player game with 3 deaths, 2 energizers, then a two-player game) is played with --fast
# on a pretend pad (SDL virtual joystick) of each type; the pretend pad records every motor command the program sends it.
# Needs a display (Xvfb :99), the built app and a replay made from the ROM (tools/analysis/oracle_replay.py --make).
cd "$(dirname "$0")/.." || exit 2
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
D=build/s34r; mkdir -p $D
python3 tools/analysis/oracle_replay.py --make $D/script.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
run() { n=$1; shift; timeout 250 build/app/pacman-port --no-audio --scale 2 --replay $D/script.replay --fast --config $D/$n.cfg --no-hiscore --report $D/$n.txt "$@" >/dev/null 2>&1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got $2, wanted $3)"; fail=1; fi; }
num() { grep -o "$2 [0-9]*" $D/$1.txt | tail -1 | cut -d' ' -f$3; }
got() { grep "Test pad received" $D/$1.txt | sed 's/.*received: \([0-9]*\) motor commands (strongest big \([0-9]*\), small \([0-9]*\) of.*, \([0-9]*\) trigger-motor commands (strongest \([0-9]*\)).*/\1 \2 \3 \4 \5/'; }
evn() { grep "Vibration summary" $D/$1.txt | sed -n "s/.*[ :,]$2 \([0-9]*\).*/\1/p"; }
for t in xbox360 xboxone ps4 ps5; do run $t --virtual-type $t; done
chk "Xbox 360 recognised" "$(grep -c 'Vibration: Xbox pad' $D/xbox360.txt)" 1
chk "Xbox One recognised" "$(grep -c 'Vibration: Xbox pad' $D/xboxone.txt)" 1
chk "DualShock 4 recognised" "$(grep -c 'Vibration: PlayStation 4 pad' $D/ps4.txt)" 1
chk "DualSense recognised" "$(grep -c 'Vibration: PlayStation 5 pad' $D/ps5.txt)" 1
for t in xbox360 xboxone ps4 ps5; do
  chk "$t: coins / dots / energizers / deaths / fright ends" "$(evn $t COIN)/$(evn $t DOT)/$(evn $t ENERGIZER)/$(evn $t DEATH)/$(evn $t "FRIGHT END")" "3/119/2/5/2"
  set -- $(got $t); chk "$t: the pad itself received motor commands" "$([ "$1" -gt 500 ] && echo yes || echo no)" yes
done
set -- $(got xbox360); X1=$1; XL=$2; XH=$3; XT=$4; XTM=$5
set -- $(got ps4);     P4L=$2; P4H=$3; P4T=$4
set -- $(got ps5);     P5L=$2; P5H=$3; P5T=$4
chk "Xbox: trigger motors driven" "$([ "$XT" -gt 100 ] && echo yes || echo no)" yes
chk "DualShock 4: no trigger commands" "$P4T" 0
chk "DualSense: no trigger commands" "$P5T" 0
chk "DualSense big motor softer than Xbox (80% vs 100%)" "$([ "$P5L" -lt "$XL" ] && echo yes || echo no)" yes
chk "DualShock 4 small motor stronger than Xbox (boost)" "$([ "$P4H" -gt "$XH" ] && echo yes || echo no)" yes
run light --virtual-type xbox360 --vibration light
chk "light: no dot purr and no heartbeat, energizers still felt" "$(evn light DOT)/$(evn light "GHOST NEAR")/$(evn light ENERGIZER)" "0/0/2"
run off --virtual-type xbox360 --vibration off
set -- $(got off); chk "off: the pad received no motor command" "$1" 0
run purist --virtual-type xbox360 --purist
set -- $(got purist); chk "Purist: the pad received no motor command" "$1" 0
run nopad
run hashA --virtual-type ps5 --hashlog $D/hashA.log
run hashB --hashlog $D/hashB.log
chk "vibration cannot change the game: 7,273 frame hashes identical with and without a shaking pad" "$(cmp -s $D/hashA.log $D/hashB.log && wc -l < $D/hashA.log)" 7273
chk "no pad: nothing happens and no crash" "$(grep -c 'Vibration' $D/nopad.txt)" 0
[ $fail = 0 ] && echo "PASS: vibration reaches the pad for Xbox, DualShock 4 and DualSense; Light, Off and Purist behave" || { echo "FAIL"; exit 1; }
