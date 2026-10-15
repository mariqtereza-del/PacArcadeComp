#!/bin/sh
# X08: button pictures switch between keyboard and pad. Real window, pretend pads (SDL virtual joystick) of each type, a pretend keyboard (xdotool).
# The menu is opened (so the key help panel is on screen) and the screen is photographed; key caps have a grey-violet rim (7A7A92), the Xbox / PlayStation button pictures have their own colours.
cd "$(dirname "$0")/.." || exit 2
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
D=build/x08; rm -rf $D; mkdir -p $D
printf 'pad_coin = a\npad_start1 = b\npad_start2 = x\npad_menu = rightstick\npad_pause = y\npad_save = leftshoulder\npad_load = rightshoulder\npad_rewind = back\n' > $D/controls.cfg
mkcfg() { mkdir -p $D/$1; cp $D/controls.cfg $D/$1/controls.cfg; [ -n "$2" ] && printf 'hints %s\n' "$2" > $D/$1/settings.cfg; }
# --- keyboard only: Esc opens the menu
mkcfg kb ""
build/app/pacman-port --no-audio --scale 2 --config $D/kb/controls.cfg --report $D/kb.txt --hiscore $D/hs.txt --ticks 100000 >/dev/null 2>&1 &
P=$!; sleep 3.5; W=$(xdotool search --pid $P | head -1); xdotool key --window $W Escape; sleep 1.0; import -window root $D/kb.png; kill $P 2>/dev/null; wait $P 2>/dev/null
# --- pad of each type: touch the stick, open the menu with the pad
for t in xbox360 xboxone ps4 ps5; do
  mkcfg $t ""
  build/app/pacman-port --no-audio --scale 2 --config $D/$t/controls.cfg --report $D/$t.txt --hiscore $D/hs.txt --virtual-type $t --pad-virtual "1500:a0=20000,2300:a0=0" --pad-buttons "2800:rightstick" --ticks 100000 >/dev/null 2>&1 &
  P=$!; sleep 4.5; import -window root $D/$t.png; kill $P 2>/dev/null; wait $P 2>/dev/null
done
# --- pad touched, then the keyboard (back to key caps)
mkcfg back ""
build/app/pacman-port --no-audio --scale 2 --config $D/back/controls.cfg --report $D/back.txt --hiscore $D/hs.txt --virtual-type ps4 --pad-virtual "1500:a0=20000,2300:a0=0" --pad-buttons "2800:rightstick" --ticks 100000 >/dev/null 2>&1 &
P=$!; sleep 4.5; import -window root $D/back_pad.png; W=$(xdotool search --pid $P | head -1); xdotool key --window $W Down; sleep 1.0; import -window root $D/back_key.png; kill $P 2>/dev/null; wait $P 2>/dev/null
# --- forced settings with a ps4 pad whose stick was touched / keyboard used
mkcfg forcekb keyboard
build/app/pacman-port --no-audio --scale 2 --config $D/forcekb/controls.cfg --report $D/forcekb.txt --hiscore $D/hs.txt --virtual-type ps4 --pad-virtual "1500:a0=20000,2300:a0=0" --pad-buttons "2800:rightstick" --ticks 100000 >/dev/null 2>&1 &
P=$!; sleep 4.5; import -window root $D/forcekb.png; kill $P 2>/dev/null; wait $P 2>/dev/null
mkcfg forcepad pad
build/app/pacman-port --no-audio --scale 2 --config $D/forcepad/controls.cfg --report $D/forcepad.txt --hiscore $D/hs.txt --virtual-type ps4 --ticks 100000 >/dev/null 2>&1 &
P=$!; sleep 3.5; W=$(xdotool search --pid $P | head -1); xdotool key --window $W Escape; sleep 1.0; import -window root $D/forcepad.png; kill $P 2>/dev/null; wait $P 2>/dev/null
# --- Purist with a pad: always key caps (greyed)
mkcfg purist ""
build/app/pacman-port --no-audio --scale 2 --purist --config $D/purist/controls.cfg --report $D/purist.txt --hiscore $D/hs.txt --virtual-type ps4 --pad-virtual "1500:a0=20000,2300:a0=0" --ticks 100000 >/dev/null 2>&1 &
P=$!; sleep 5; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 0.5; xdotool key Escape; sleep 1.2; import -window root $D/purist.png; kill $P 2>/dev/null; wait $P 2>/dev/null
python3 - <<'PY'
import sys
from PIL import Image
D = "build/x08"
def count(img, rgb, tol=10):
    px = img.load(); w, h = img.size; n = 0
    for y in range(h):
        for x in range(w):
            p = px[x, y][:3]
            if abs(p[0]-rgb[0]) <= tol and abs(p[1]-rgb[1]) <= tol and abs(p[2]-rgb[2]) <= tol: n += 1
    return n
CAP = (0x7A,0x7A,0x92); GREEN, RED, BLUE, YELLOW = (0x30,0xC0,0x40), (0xE0,0x30,0x30), (0x30,0x70,0xF0), (0xF0,0xC0,0x20)
PSX, PSO, PSS, PST = (0x6C,0x9E,0xFF), (0xFF,0x50,0x50), (0xFF,0x8C,0xD2), (0x40,0xE0,0xA0)
fail = 0
def chk(what, ok):
    global fail
    print(("  ok: " if ok else "  FAIL: ") + what)
    if not ok: fail = 1
def stats(n):
    im = Image.open(f"{D}/{n}.png").convert("RGB")
    return count(im, CAP, 4), [count(im, c) for c in (GREEN, RED, BLUE, YELLOW)], [count(im, c) for c in (PSX, PSO, PSS, PST)]
def keys(n):
    cap, xb, ps = stats(n); chk(f"{n}: key caps (rim pixels {cap}), no button pictures (Xbox {xb}, PlayStation {ps})", cap > 3000 and all(v < 5 for v in xb + ps)); 
def xbox(n):
    cap, xb, ps = stats(n); chk(f"{n}: Xbox pictures {xb}, no key caps (rim pixels {cap}), no PlayStation shapes {ps}", all(v > 20 for v in xb) and cap < 500 and all(v < 5 for v in ps))
def psn(n):
    cap, xb, ps = stats(n); chk(f"{n}: PlayStation pictures {ps}, no key caps (rim pixels {cap}), no Xbox discs {xb}", all(v > 8 for v in ps) and cap < 500 and all(v < 5 for v in xb))
keys("kb"); xbox("xbox360"); xbox("xboxone"); psn("ps4"); psn("ps5")
psn("back_pad"); keys("back_key")
keys("forcekb"); psn("forcepad"); keys("purist")
print("PASS: key caps for the keyboard, the pad's own pictures after touching the pad, and back; the setting forces either; Purist keeps key caps" if not fail else "FAIL")
sys.exit(fail)
PY
