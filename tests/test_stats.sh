#!/bin/sh
# X22: per-run statistics. Part 1: unit test on made-up frames. Part 2: the real S27 replay: the program's statistics (--stats) must add up to the game's own counters
# (the independent --events log: dots, energizers, deaths; the score equals the score in RAM). Part 3: hashes with statistics = without; Purist writes nothing and hashes are identical.
cd "$(dirname "$0")/.." || exit 2
D=build/x22; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_stats.cpp -o $D/test_stats || { echo "FAIL: build"; exit 1; }
$D/test_stats | tail -1 || exit 1
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
run() { n=$1; shift; timeout 250 build/app/pacman-port --no-audio --scale 2 --replay $D/s27.replay --fast --config $D/$n.cfg --no-hiscore --report $D/$n.txt "$@" >/dev/null 2>&1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got $2, wanted $3)"; fail=1; fi; }
rm -f $D/stats.txt $D/pur_stats.txt
run st --hashlog $D/st.hash --stats $D/stats.txt --events $D/ev.txt; run plain --hashlog $D/plain.hash; run pur --purist --hashlog $D/pur.hash; run purst --purist --hashlog $D/purst.hash --stats $D/pur_stats.txt
sum() { grep -o "$1=[0-9]*" $D/stats.txt | cut -d= -f2 | awk '{s+=$1} END {print s+0}'; }
chk "dots over all runs = DOT events" "$(sum dots)" "$(grep -c ' DOT$' $D/ev.txt)"
chk "energizers over all runs = ENERGIZER events" "$(sum energizers)" "$(grep -c ' ENERGIZER$' $D/ev.txt)"
chk "deaths over all runs = DEATH events" "$(sum deaths)" "$(grep -c ' DEATH$' $D/ev.txt)"
chk "ghosts over all runs = GHOST EATEN events" "$(sum ghosts)" "$(grep -c 'GHOST EATEN' $D/ev.txt)"
chk "score = the game's score in RAM in every line" "$(awk '{split($5,a,"=");split($6,b,"="); if (a[2]!=b[2]) bad++} END {print bad+0}' $D/stats.txt)" 0
chk "one finished game and two still running at the end" "$(grep -c '^GAMEOVER' $D/stats.txt) $(grep -c '^ACTIVE' $D/stats.txt)" "1 2"
chk "every run counted (nothing was helped)" "$(grep -c 'counted=1' $D/stats.txt)" 3
chk "hashes with statistics = without" "$(cmp -s $D/st.hash $D/plain.hash && echo same)" same
chk "Purist hashes = without" "$(cmp -s $D/pur.hash $D/purst.hash && echo same)" same
chk "Purist writes no statistics" "$([ ! -s $D/pur_stats.txt ] && echo none)" none
chk "frames hashed" "$(wc -l < $D/st.hash | tr -d ' ')" 7273
cat $D/stats.txt
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
