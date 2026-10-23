#!/bin/sh
# S34E: achievements follow the GAME settings, not the look.
#  1. the S27 replay (7,273 frames, standard settings) unlocks achievements in a fresh settings folder
#  2. the same replay with a 5-lives machine (replay header dip CD) unlocks nothing and the log says why
#  3. the same replay with RULES: READABLE unlocks nothing
#  4. the same replay with every display setting changed unlocks exactly what 1 unlocks (display settings have no effect), achievements.cfg is byte-identical
#  5. lives set to 5 and back to 3 at the attract screen before the first game: the game counts again (late script, first coin at frame 4500)
#  6. lives set to 5 at the attract screen: nothing unlocks
# Needs roms/pacman.zip, g++, SDL2 and a display (Xvfb :99). About 2 minutes.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM"; exit 2; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
T=build/s34e; rm -rf $T; mkdir -p $T
python3 tools/analysis/oracle_replay.py --make $T/script.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sed 's/^dip C9/dip CD/' $T/script.replay > $T/script5.replay
python3 - $T/script.replay $T/late3.replay <<'PY'
import sys
src, o3 = sys.argv[1:3]
out = []
for ln in open(src).read().split("\n"):
    t = ln.split()
    if len(t) == 3 and t[0].isdigit() and int(t[0]) >= 400: ln = "%d %s %s" % (int(t[0]) + 4100, t[1], t[2])
    elif ln.startswith("frames "): ln = "frames %d" % (int(ln.split()[1]) + 4100)
    out.append(ln)
open(o3, "w").write("\n".join(out))
PY
fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
run() { d=$1; r=$2; shift; shift; mkdir -p $T/$d; printf '# test\nhigh_score = 5000\n' > $T/$d/hs.txt; timeout 600 build/app/pacman-port --replay $r --fast --no-audio --config $T/$d/controls.cfg --hiscore $T/$d/hs.txt "$@" > $T/$d/out.txt 2>&1 || { echo "FAIL: run $d"; fail=1; }; }
n() { grep -c "Achievement unlocked" $T/$1/out.txt; }
run s1 $T/script.replay
run s2 $T/script5.replay
run s3 $T/script.replay --rules readable
run s4 $T/script.replay --overlay --scanlines --surround ambient --curve strong --glow strong --crt-mask strong
run s5 $T/late3.replay --cmd-at 1000:lives,1010:lives,1020:lives,1030:lives
run s6 $T/late3.replay --cmd-at 1000:lives
N1=$(n s1)
[ "$N1" -ge 1 ] && r=yes || r=no
chk "1. the standard game unlocks achievements ($N1)" $r yes
chk "2. 5 lives: nothing unlocks" "$(n s2)" 0
chk "2. the log says a game setting is not at its standard value" "$(grep -c 'a game setting is not at its standard value' $T/s2/out.txt)" 1
chk "3. RULES: READABLE: nothing unlocks" "$(n s3)" 0
chk "4. display settings changed: the same achievements unlock" "$(n s4)" "$N1"
sed 's/ [0-9]*$//' $T/s1/achievements.cfg > $T/s1.cfg; sed 's/ [0-9]*$//' $T/s4/achievements.cfg > $T/s4.cfg
cmp -s $T/s1.cfg $T/s4.cfg && r=same || r=different
chk "4. and achievements.cfg is the same (apart from the wall-clock time of the unlock)" $r same
chk "5. lives 5 and back to 3 before the game: it counts again" "$([ "$(n s5)" -ge 1 ] && echo yes || echo no)" yes
chk "5. the log says the settings left the standard and came back" "$(grep -c 'every game setting is at its standard value again' $T/s5/out.txt)" 1
chk "6. lives left at 5: nothing unlocks" "$(n s6)" 0
[ $fail = 0 ] && echo "PASS test_achievement_settings.sh" || { echo "FAIL test_achievement_settings.sh"; exit 1; }
