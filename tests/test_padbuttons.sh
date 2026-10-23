#!/bin/sh
# S34B: the real program is fed simulated pad-button events (--pad-buttons) and must react exactly as the plan says. Needs a display (Xvfb :99), roms/pacman.zip and the built app.
#  normal run: R3 opens the menu, d-pad down + A = Save state (X12: opens the slot picker), A again = slot 1, R3 closes it; pad button LB = quick save; R3, down, down, A = Load state; X = pause and resume; RB = quick load; Y held = rewind;
#              B on the main menu page closes it. A quicksave.sav must exist.
#  Purist run: the same buttons: menu opens and closes, save, load and rewind are refused ("switched off in Purist mode"), no quicksave.sav is written. (A refused Load in the menu leaves the menu open,
#              so the X and RB presses that follow are ignored until R3 closes it.)
cd "$(dirname "$0")/.." || exit 2
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
T=build/s34b/t; rm -rf $T; mkdir -p $T/n $T/p
for d in n p; do printf 'pad_save = leftshoulder\npad_load = rightshoulder\npad_pause = x\npad_rewind = y\n' > $T/$d/controls.cfg; done
EV="3000:y,3600:-y,4000:rightstick,4300:dpdown,4600:a,4750:a,4900:rightstick,6000:leftshoulder,7000:rightstick,7300:dpdown,7600:dpdown,7900:a,8100:a,9000:x,9500:x,10000:rightshoulder,11000:rightstick,11300:b"
timeout 90 build/app/pacman-port --no-audio --scale 2 --config $T/n/controls.cfg --report $T/n/run.txt --pad-buttons "$EV" --ticks 1500 >/dev/null 2>&1
timeout 90 build/app/pacman-port --no-audio --scale 2 --purist --config $T/p/controls.cfg --report $T/p/run.txt --pad-buttons "$EV" --ticks 1500 >/dev/null 2>&1
c() { grep -c "$2" "$1"; }
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got $2, wanted $3)"; fail=1; fi; }
chk "normal: menu opened by the pad button (twice by R3 plus once more)" "$(c $T/n/run.txt 'Menu: opened')" 3
chk "normal: saves (menu item + LB)" "$(c $T/n/run.txt 'State: saved')" 2
chk "normal: loads (menu item + RB)" "$(c $T/n/run.txt 'State: loaded')" 2
chk "normal: pause and resume by X" "$(c $T/n/run.txt 'State: paused')$(c $T/n/run.txt 'State: resumed')" 11
chk "normal: rewind by held Y" "$(c $T/n/run.txt 'State: rewinding')" 1
chk "normal: quicksave.sav written" "$([ -s $T/n/quicksave.sav ] && echo yes || echo no)" yes
chk "purist: menu opened by the pad button (twice; the refused load leaves it open, so X and RB are ignored until R3 closes it)" "$(c $T/p/run.txt 'Menu: opened')" 2
chk "purist: menu closed by the pad button" "$(c $T/p/run.txt 'Menu: resumed')" 2
chk "purist: nothing saved or loaded" "$(c $T/p/run.txt 'State: saved')$(c $T/p/run.txt 'State: loaded')" 00
chk "purist: refusals shown (Y rewind, menu A twice on SAVE STATE, LB, menu A twice on LOAD STATE)" "$(c $T/p/run.txt 'switched off')" 6
chk "purist: no quicksave.sav" "$([ -e $T/p/quicksave.sav ] && echo yes || echo no)" no

# S34B fix: triggers (L2/R2 are axes), Share/Mic, paddle and touchpad can be bound. A virtual pad drives the triggers and extra buttons.
V=build/s34b/v; rm -rf $V; mkdir -p $V
printf 'pad_coin = lefttrigger\npad_save = righttrigger\npad_pause = touchpad\npad_rewind = misc1\npad_load = paddle1\n' > $V/controls.cfg
timeout 100 build/app/pacman-port --no-audio --scale 2 --config $V/controls.cfg --report $V/run.txt --virtual-pad --pad-virtual "8000:a4=32767,8300:a4=0,9500:a5=32767,9800:a5=0,11000:b15=1,11600:b15=0,12500:b16=1,12600:b16=0" --ticks 1500 >/dev/null 2>&1
chk "triggers: L2 coin gives a credit" "$(grep -c 'credits 1' $V/run.txt)" 1
chk "triggers: R2 saves" "$(c $V/run.txt 'State: saved')" 1
chk "extra buttons: Share/Mic rewinds" "$(c $V/run.txt 'State: rewinding')" 1
chk "extra buttons: paddle loads" "$(c $V/run.txt 'State: loaded')" 1
timeout 100 build/app/pacman-port --no-audio --scale 2 --config $V/controls.cfg --report $V/run2.txt --pad-buttons "4000:touchpad,5000:touchpad" --ticks 1500 >/dev/null 2>&1
chk "touchpad: pauses and resumes" "$(c $V/run2.txt 'State: paused')$(c $V/run2.txt 'State: resumed')" 11
[ $fail = 0 ] && echo "PASS: pad buttons drive the menu, pause, save, load and rewind; Purist locks save, load and rewind" || { echo "FAIL"; exit 1; }
