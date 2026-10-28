#!/bin/sh
# X24 (changed in 2.1.0), part 2 (the real program in a window, Xvfb + xdotool): the test aid --test-clear pretends levels were cleared (the machine is not touched).
# Since 2.1.0 the records are INTERNAL: nothing about them is shown (no NEW BEST banner, no Records page or menu item) or written to the log; records.cfg is still kept exactly as before.
# Checks: records.cfg content after first / better / worse clears, a restart reads it again without any message, the menu has no RECORDS item, the log has no record line, Purist never writes records.
cd "$(dirname "$0")/.." || exit 2
D=build/x24w; mkdir -p $D
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -rf $D/a $D/p; mkdir -p $D/a $D/p
build/app/pacman-port --no-audio --scale 2 --config $D/a/controls.cfg --report $D/a/r.txt --hiscore $D/a/hs.txt --test-clear "1,1200,3000;1,1500,2500;1,900,3200;4,500,800" --ticks 3000 --fast >/dev/null 2>&1
R=$D/a/r.txt
chk "no Records line in the log" "$(grep -a -c '^Records' $R)" 0
chk "no NEW BEST text in the log" "$(grep -a -c 'NEW BEST\|RECORD SET\|first record' $R)" 0
chk "records.cfg holds the best of level 1" "$(grep -c '^level 1 score 1500 frames 3000$\|^level 1 score 1500 frames 2500$' $D/a/records.cfg)" 1
chk "records.cfg level 1 line: best score 1500, fastest 2500" "$(grep '^level 1 ' $D/a/records.cfg)" "level 1 score 1500 frames 2500"
chk "records.cfg level 4 line" "$(grep '^level 4 ' $D/a/records.cfg)" "level 4 score 500 frames 800"
chk "no other lines" "$(grep -c '^level' $D/a/records.cfg)" 2
# a restart reads the file again (silently) and a worse clear does not change it (the menu has no RECORDS item: tests/test_menu.sh checks that)
build/app/pacman-port --no-audio --scale 2 --config $D/a/controls.cfg --report $D/a/r2.txt --hiscore $D/a/hs.txt --test-clear "1,100,9000" --ticks 1500 --fast >/dev/null 2>&1
chk "the second start printed no Records line" "$(grep -a -c 'Records' $D/a/r2.txt)" 0
chk "records.cfg level 1 line unchanged by the worse clear" "$(grep '^level 1 ' $D/a/records.cfg)" "level 1 score 1500 frames 2500"
# Purist: nothing is written
build/app/pacman-port --no-audio --scale 2 --purist --config $D/p/controls.cfg --report $D/p/r.txt --hiscore $D/p/hs.txt --test-clear "1,1200,3000" --ticks 1500 --fast >/dev/null 2>&1
chk "Purist: no records.cfg" "$([ ! -e $D/p/records.cfg ] && echo none)" none
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
