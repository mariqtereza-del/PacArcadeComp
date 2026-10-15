#!/bin/sh
# X01: game event reader. Unit test on made-up frames, then the real S27 replay (counts must equal the game's own counters), then the program's --events log.
cd "$(dirname "$0")/.." || exit 2
D=build/x01; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_events.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_events 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_events $D/s27.replay || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
run() { n=$1; shift; timeout 250 build/app/pacman-port --no-audio --scale 2 --replay $D/s27.replay --fast --config $D/$n.cfg --no-hiscore --report $D/$n.txt "$@" >/dev/null 2>&1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got $2, wanted $3)"; fail=1; fi; }
run plain --hashlog $D/plain.hash; run ev --hashlog $D/ev.hash --events $D/events.txt; run pur --purist --hashlog $D/pur.hash; run purev --purist --hashlog $D/purev.hash --events $D/purev_events.txt
chk "program log: dots (DOT lines)" "$(grep -c ' DOT$' $D/events.txt)" 119
chk "program log: energizers" "$(grep -c ' ENERGIZER$' $D/events.txt)" 2
chk "program log: deaths" "$(grep -c ' DEATH$' $D/events.txt)" 5
chk "program log: game starts" "$(grep -c ' GAME START' $D/events.txt)" 2
chk "program log: game overs" "$(grep -c ' GAME OVER$' $D/events.txt)" "$(grep -c ' GAME OVER$' $D/events.txt)"
chk "hashes with --events = without" "$(cmp -s $D/plain.hash $D/ev.hash && echo same)" same
chk "Purist hashes with --events = without" "$(cmp -s $D/pur.hash $D/purev.hash && echo same)" same
chk "Purist writes no events" "$([ ! -s $D/purev_events.txt ] && echo none)" none
chk "frames hashed" "$(wc -l < $D/ev.hash | tr -d ' ')" 7273
head -12 $D/events.txt; grep "Events summary" $D/ev.txt
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
