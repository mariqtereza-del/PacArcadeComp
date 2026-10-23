#!/bin/sh
# S34D + S34E: the two RESET TO DEFAULTS items (DISPLAY page: look only, no restart; GAME OPTIONS page: game settings only, restarts) and the live DIP change, in the real program (no window needed beyond the Xvfb the other S34 tests use).
#  1. the S27 replay (7,273 frames) with settings set to non-default by command-line options (rules READABLE, overlay, filters, ...) and the reset pressed twice at tick 3000: the frame hash log equals a plain run
#     (the reset does not change the game, switching READABLE -> ORIGINAL in the middle is invisible), the log says the settings are back and that the game was NOT restarted
#  2. the settings file after the reset is byte-identical to one written from the standard settings; the saved high score and the controls file are untouched
#  3. lives changed at the attract screen (tick 1000) are put into the running game: from the coin on, the hashes equal a run that started with that setting
#  4. lives changed in the middle of a game (tick 5000): the game goes on unchanged for its whole length (hash log equal until the game is over)
#  5. one Enter only (not twice) resets nothing
# Needs roms/pacman.zip, g++, SDL2 and a display (Xvfb :99). About 2 minutes.
cd "$(dirname "$0")/.." || exit 2
T=build/s34d; rm -rf $T/a $T/a2 $T/a3 $T/b $T/c $T/ref $T/e $T/f $T/g $T/h $T/one; mkdir -p $T/a $T/b $T/c $T/ref $T/e $T/f $T/g $T/h $T/one
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM"; exit 2; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
python3 tools/analysis/oracle_replay.py --make $T/script.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
HSTXT='# test\nhigh_score = 5000\n'
for d in a b c e ref f g h one; do printf "$HSTXT" > $T/$d/hs.txt; done
run() { d=$1; shift; timeout 600 build/app/pacman-port --replay $T/script.replay --hashlog $T/$d/hash.log --fast --no-audio --config $T/$d/controls.cfg --hiscore $T/$d/hs.txt "$@" > $T/$d/out.txt 2>&1 || { echo "FAIL: run $d"; fail=1; }; }
cat > $T/a/settings.cfg <<'S'
# Pac-Man port settings (the menu writes this file; you may also edit it with Notepad)
overlay on
scanlines on
fill on
smooth on
window_frame off
help_panel on
lives 3
bonus 10000
difficulty normal
ghost_names normal
vibration off
pad_symbols xbox
pause_on_focus_loss off
hints keyboard
lightbar off
run_summary off
surround ambient
curve strong
glow strong
crt_mask strong
rules readable
S
printf 'purist on\n' >> $T/a/settings.cfg    # an old file with the removed key: reported and ignored
printf 'KEEP-THIS-CONTROLS-FILE\n' > $T/a/controls.cfg; cp $T/a/controls.cfg $T/a/controls.before
cp $T/a/hs.txt $T/a/hs.before
run ref
run a --rules readable --overlay --scanlines --surround ambient --curve strong --glow strong --crt-mask strong --cmd-at 3000:resetdisplay,3000:resetdisplay
cmp -s $T/a/hash.log $T/ref/hash.log && r=same || r=different
chk "1. frame hashes with the DISPLAY reset in the middle equal the plain run ($(wc -l < $T/ref/hash.log) frames)" $r same
chk "1. the log says the display settings are back to standard" "$(grep -c 'display settings are back to their standard values' $T/a/out.txt)" 1
chk "1. the log says the game was not restarted" "$(grep -c 'Game: restarted' $T/a/out.txt)" 0
chk "1. the first Enter only asked (the log has the question once)" "$(grep -c 'press Enter again on DISPLAY' $T/a/out.txt)" 1
chk "1. the DISPLAY reset left the rules alone (READABLE stays; no other rules line)" "$(grep -c 'Game rules: READABLE' $T/a/out.txt)" 1
run b --rules readable --cmd-at 3000:resetgame,3000:resetgame
chk "1b. the GAME OPTIONS reset restarts the game (log)" "$(grep -c 'Game: restarted (game settings reset)' $T/b/out.txt)" 1
chk "1b. the log says what was reset" "$(grep -c 'game settings are back to their standard values' $T/b/out.txt)" 1
chk "1b. and that the display was not touched" "$(grep -c 'display, sound, keys, buttons, high score' $T/b/out.txt)" 1
run c --rules readable --cmd-at 3000:resetgame
chk "1b. one Enter alone restarts nothing" "$(grep -c 'Game: restarted' $T/c/out.txt)" 0
# 2. the settings file after each reset (a replay run neither reads nor writes the settings file, so these runs are plain runs that stop after 400 machine frames)
run2() { d=$1; shift; timeout 300 build/app/pacman-port --fast --no-audio --ticks 400 --config $T/$d/controls.cfg --hiscore $T/$d/hs.txt "$@" > $T/$d/out.txt 2>&1 || { echo "FAIL: run2 $d"; fail=1; }; }
for d in a2 a3; do mkdir -p $T/$d; cp $T/a/settings.cfg $T/$d/settings.cfg; cp $T/a/controls.cfg $T/$d/controls.cfg; cp $T/a/hs.txt $T/$d/hs.txt; cp $T/a/controls.cfg $T/$d/controls.before; cp $T/a/hs.txt $T/$d/hs.before; done
sed -i 's/^lives 3/lives 5/' $T/a3/settings.cfg
run2 a2 --cmd-at 100:resetdisplay,100:resetdisplay
run2 a3 --cmd-at 100:resetgame,100:resetgame
chk "2. the old 'purist on' line is reported and ignored" "$(grep -c 'ignored: purist on' $T/a2/out.txt)" 1
chk "2. DISPLAY reset: display keys are standard in the file" "$(grep -c '^overlay off\|^scanlines off\|^fill off\|^smooth off\|^window_frame on\|^help_panel off\|^run_summary on\|^surround black\|^curve off\|^glow off\|^crt_mask off' $T/a2/settings.cfg)" 11
chk "2. DISPLAY reset: game keys and the other keys are kept" "$(grep -c '^rules readable\|^vibration off\|^pad_symbols xbox\|^pause_on_focus_loss off\|^hints keyboard\|^lightbar off' $T/a2/settings.cfg)" 6
chk "2. GAME reset: rules and lives are standard in the file" "$(grep -c '^rules original\|^lives 3\|^bonus 10000\|^difficulty normal\|^ghost_names normal' $T/a3/settings.cfg)" 5
chk "2. GAME reset: display and the other keys are kept" "$(grep -c '^overlay on\|^scanlines on\|^surround ambient\|^vibration off\|^lightbar off' $T/a3/settings.cfg)" 5
chk "2. GAME reset restarted the machine" "$(grep -c 'Game: restarted (game settings reset)' $T/a3/out.txt)" 1
for d in a2 a3; do
  cmp -s $T/$d/controls.cfg $T/$d/controls.before && r=same || r=different
  chk "2. $d: the controls file is untouched" $r same
  cmp -s $T/$d/hs.txt $T/$d/hs.before && r=same || r=different
  chk "2. $d: the saved high score is untouched" $r same
done
# 3. lives changed at the attract screen: put in at once, no restart; from the coin on equal to a run that started with 5 lives
# a script whose first coin comes late (frame 4500), so the attract screen lasts long enough for the change to be put in before the game
python3 - $T/script.replay $T/late3.replay $T/late5.replay <<'PY'
import sys
src, o3, o5 = sys.argv[1:4]
L = open(src).read().split("\n")
out = []
for ln in L:
    t = ln.split()
    if len(t) == 3 and t[0].isdigit() and int(t[0]) >= 400: ln = "%d %s %s" % (int(t[0]) + 4100, t[1], t[2])
    elif ln.startswith("frames "): ln = "frames %d" % (int(ln.split()[1]) + 4100)
    out.append(ln)
open(o3, "w").write("\n".join(out))
open(o5, "w").write("\n".join(out).replace("dip C9", "dip CD"))
PY
runl() { d=$1; r=$2; shift; shift; mkdir -p $T/$d; printf "$HSTXT" > $T/$d/hs.txt; timeout 600 build/app/pacman-port --replay $r --hashlog $T/$d/hash.log --fast --no-audio --config $T/$d/controls.cfg --hiscore $T/$d/hs.txt "$@" > $T/$d/out.txt 2>&1 || { echo "FAIL: run $d"; fail=1; }; }
runl f $T/late3.replay --cmd-at 1000:lives
runl g $T/late5.replay    # the reference: a machine powered on with 5 lives (the replay header carries the switches)
runl l3 $T/late3.replay   # the same script with 3 lives
chk "3. lives put into the running game at the attract screen (log)" "$(grep -c 'DIP settings put into the running game' $T/f/out.txt)" 1
chk "3. no restart" "$(grep -c 'Game: restarted' $T/f/out.txt)" 0
tail -n +1101 $T/f/hash.log > $T/f/tail.log; tail -n +1101 $T/g/hash.log > $T/g/tail.log
cmp -s $T/f/tail.log $T/g/tail.log && r=same || r=different
chk "3. from frame 1101 on (the whole game, coin at 4500) the hashes equal a run powered on with 5 lives" $r same
cmp -s $T/f/hash.log $T/l3/hash.log && r=same || r=different
chk "3. and the run really differs from the 3-lives run (the proof can see the setting)" $r different
# 4. lives changed in the middle of a game: the game goes on unchanged, the setting waits
run h --cmd-at 5000:lives
head -n 5200 $T/h/hash.log > $T/h/head.log; head -n 5200 $T/ref/hash.log > $T/ref/head.log
cmp -s $T/h/head.log $T/ref/head.log && r=same || r=different
chk "4. the game in progress is not changed (first 5200 frames equal the plain run)" $r same
chk "4. no restart" "$(grep -c 'Game: restarted' $T/h/out.txt)" 0
# 5. one Enter only
run one --cmd-at 3000:resetdisplay
cp $T/a/settings.cfg $T/one/settings.before 2>/dev/null
chk "5. one Enter alone asks and resets nothing" "$(grep -c 'display settings are back' $T/one/out.txt)" 0
[ $fail = 0 ] && echo "PASS test_reset_defaults.sh" || { echo "FAIL test_reset_defaults.sh"; exit 1; }
