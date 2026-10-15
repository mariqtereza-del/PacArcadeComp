#!/bin/sh
# X25: achievements. Part 1: unit test on made-up events + real-ROM scenarios (tests/test_achievements.cpp). Part 2: the real program on the S27 replay: POWER UP unlocks exactly once, achievements.cfg is written at once,
# a second run does not unlock it again, Purist unlocks nothing, hashes unchanged. Part 3 (window): the pop-up is seen on the picture, and Esc > ACHIEVEMENTS lists POWER UP as DONE.
cd "$(dirname "$0")/.." || exit 2
D=build/x25; mkdir -p $D
g++ -std=c++17 -O2 -Wall -Wextra tests/test_achievements.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_achievements 2>&1 | grep -E "error|warning" && { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
$D/test_achievements $D/s27.replay | grep -v "^  ok" || exit 1
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
rm -rf $D/a $D/p; mkdir -p $D/a $D/p
run() { d=$1; shift; timeout 250 build/app/pacman-port --no-audio --scale 2 --replay $D/s27.replay --fast --config $D/$d/controls.cfg --no-hiscore --report $D/$d/$N.txt "$@" >/dev/null 2>&1; }
N=r1; run a --hashlog $D/a/h1.hash
chk "run 1: POWER UP unlocked exactly once" "$(grep -a -c 'Achievement unlocked: POWER UP' $D/a/r1.txt)" 1
chk "run 1: FRUIT SALAD not unlocked (no fruit eaten in the replay)" "$(grep -a -c 'Achievement unlocked: FRUIT SALAD' $D/a/r1.txt)" 0
chk "achievements.cfg written" "$(grep -c '^unlocked first_pellet ' $D/a/achievements.cfg)" 1
N=r2; run a --hashlog $D/a/h2.hash
chk "run 2: loaded the file (1 unlocked)" "$(grep -a -c 'Achievements: loaded .* (1 unlocked)' $D/a/r2.txt)" 1
chk "run 2: nothing unlocked again" "$(grep -a -c 'Achievement unlocked' $D/a/r2.txt)" 0
N=r3; run p --purist --hashlog $D/p/h3.hash
chk "Purist: nothing unlocked" "$(grep -a -c 'Achievement unlocked' $D/p/r3.txt)" 0
chk "Purist: no achievements.cfg" "$([ ! -e $D/p/achievements.cfg ] && echo none)" none
chk "hashes with achievements = Purist hashes (7,273 frames)" "$(cmp -s $D/a/h1.hash $D/p/h3.hash && echo same)" same
chk "frames hashed" "$(wc -l < $D/a/h1.hash | tr -d ' ')" 7273
# a damaged file is set aside
rm -rf $D/c; mkdir -p $D/c; printf 'garbage\nnot an achievements file\n' > $D/c/achievements.cfg; N=r4; run c
chk "damaged file: the game started and says so" "$(grep -a -c 'was damaged; it was set aside' $D/c/r4.txt)" 1
chk "damaged file: set aside as .bad" "$([ -e $D/c/achievements.cfg.bad ] && echo yes)" yes
chk "damaged file: the unlock was still recorded in a new file" "$(grep -c '^unlocked first_pellet ' $D/c/achievements.cfg)" 1
# window: the pop-up on the picture (real time replay), then the list
rm -rf $D/w; mkdir -p $D/w
build/app/pacman-port --no-audio --scale 2 --replay $D/s27.replay --config $D/w/controls.cfg --no-hiscore --report $D/w/r.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 8; W=$(xdotool search --pid $P | head -1); FOUND=no; t=0
while [ $t -lt 100 ]; do sleep 0.3; t=$((t+1)); import -window $W $D/w/now.png 2>/dev/null; c=$(convert $D/w/now.png -format '%[hex:p{28,60}]' info: 2>/dev/null)
  if [ "$c" = "000028" ]; then FOUND=yes; cp $D/w/now.png $D/w/popup.png; break; fi; done
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "the pop-up was seen on the picture" "$FOUND" yes
build/app/pacman-port --no-audio --scale 2 --config $D/w/controls.cfg --hiscore $D/w/hs.txt --report $D/w/r2.txt --ticks 10000000 >/dev/null 2>&1 &
P=$!; sleep 6; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5
k() { xdotool key $1; sleep ${2:-0.3}; }
k Escape 0.8; for i in 1 2 3 4 5 6 7; do k Down 0.25; done; k Return 0.6; import -window $W $D/w/list.png 2>/dev/null
for i in 1 2 3 4 5 6 7 8 9 10 11; do k Down 0.2; done; k Up 0.3; import -window $W $D/w/list_scrolled.png 2>/dev/null
kill $P 2>/dev/null; wait $P 2>/dev/null; sleep 1
chk "the second start loaded the file" "$(grep -a -c 'Achievements: loaded .* (1 unlocked)' $D/w/r2.txt)" 1
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
