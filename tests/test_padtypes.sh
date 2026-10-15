#!/bin/sh
# S34C: the controller icon and button pictures, for each pad type, in the real window. A pretend pad (SDL virtual joystick) of each type is plugged in; the program is driven by
# simulated pad buttons (R3 opens the menu, d-pad and A go to Controls > Bindings); screenshots are taken of the toast and of the Bindings screen and counted for the colours of the pictures.
# Needs a display (Xvfb :99), ImageMagick `import`, python3 with PIL, and the built app. No ROM needed beyond the program's own (roms/pacman.zip).
cd "$(dirname "$0")/.." || exit 2
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
D=build/s34c; rm -rf $D; mkdir -p $D
# face buttons on four pad commands (their pictures are the test), triggers on coin and start 2
printf 'pad_coin = lefttrigger\npad_start1 = start\npad_start2 = righttrigger\npad_menu = rightstick\npad_pause = a\npad_save = b\npad_load = x\npad_rewind = y\n' > $D/controls.cfg
EV="2500:rightstick,2800:dpdown,3000:dpdown,3200:dpdown,3400:dpdown,3700:a,4000:a"
for t in xbox360 xboxone ps4 ps5 other ps4asxbox; do
  vt="--virtual-type $t"; [ $t = other ] && vt="--virtual-pad"; [ $t = ps4asxbox ] && vt="--virtual-type ps4 --pad-symbols xbox"
  build/app/pacman-port --no-audio --scale 2 --config $D/controls.cfg --report $D/$t.txt $vt --pad-buttons "$EV" --ticks 100000 >/dev/null 2>&1 &
  P=$!
  sleep 1.6; import -window root $D/${t}_toast.png
  sleep 3.6; import -window root $D/${t}_bindings.png
  kill $P 2>/dev/null; wait $P 2>/dev/null
done
python3 - <<'PY'
import sys
from PIL import Image
D = "build/s34c"
def count(img, rgb, tol=10):
    px = img.load(); w, h = img.size; n = 0
    for y in range(h):
        for x in range(w):
            p = px[x, y][:3]
            if abs(p[0]-rgb[0]) <= tol and abs(p[1]-rgb[1]) <= tol and abs(p[2]-rgb[2]) <= tol: n += 1
    return n
GREEN, RED, BLUE, YELLOW = (0x30,0xC0,0x40), (0xE0,0x30,0x30), (0x30,0x70,0xF0), (0xF0,0xC0,0x20)
PSX, PSO, PSS, PST = (0x6C,0x9E,0xFF), (0xFF,0x50,0x50), (0xFF,0x8C,0xD2), (0x40,0xE0,0xA0)
fail = 0
def chk(what, ok):
    global fail
    print(("  ok: " if ok else "  FAIL: ") + what)
    if not ok: fail = 1
for t, word in (("xbox360","XBOX"),("xboxone","XBOX"),("ps4","PS4"),("ps5","PS5"),("other","PAD"),("ps4asxbox","XBOX")):
    b = Image.open(f"{D}/{t}_bindings.png").convert("RGB")
    xb = [count(b, c) for c in (GREEN, RED, BLUE, YELLOW)]; ps = [count(b, c) for c in (PSX, PSO, PSS, PST)]
    log = open(f"{D}/{t}.txt").read()
    chk(f"{t}: the log says the icon is {word}", f"Gamepad icon: {word} connected" in log)
    if t.startswith("xbox") or t == "ps4asxbox":
        chk(f"{t}: Bindings screen has the green, red, blue and yellow A B X Y discs (counts {xb})", all(n > 20 for n in xb) )
        chk(f"{t}: no PlayStation shapes (counts {ps})", all(n < 5 for n in ps))
    elif t.startswith("ps"):
        chk(f"{t}: Bindings screen has the cross, circle, square and triangle in their colours (counts {ps})", all(n > 8 for n in ps))
        chk(f"{t}: no Xbox discs (counts {xb})", all(n < 5 for n in xb))
    else:
        chk(f"{t}: plain names, no pictures (counts {xb} {ps})", all(n < 5 for n in xb + ps))
    tt = Image.open(f"{D}/{t}_toast.png").convert("RGB")
    chk(f"{t}: the 'connected' notice is on the screen (green frame, count {count(tt, (0,255,0), 6)})", count(tt, (0,255,0), 6) > 100)
print("PASS: the right icon (and the Xbox override on a PlayStation pad) and button pictures for Xbox, DualShock 4, DualSense and other pads" if not fail else "FAIL")
sys.exit(fail)
PY
