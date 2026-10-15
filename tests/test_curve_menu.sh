#!/bin/sh
# X42: (1) the curve is flat while the Esc menu is open (so the mouse hits the right menu line) and bends again when it closes; (2) with a real game on screen the maze's left wall bows with
# the curve on and is straight with it off. Real window (Xvfb), screenshots of the window with ImageMagick.
cd "$(dirname "$0")/.." || exit 2
D=build/x42m; mkdir -p $D; fail=0
python3 tools/analysis/oracle_replay.py --make $D/s27.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep -f "Xvfb $DISPLAY" >/dev/null || { Xvfb $DISPLAY -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
k() { xdotool keydown $1; sleep 0.2; xdotool keyup $1; }
shot() { eval $(xdotool getwindowgeometry --shell $1); import -window root -crop "${WIDTH}x${HEIGHT}+${X}+${Y}" +repage $2; }
# (1) menu: attract screen, curve strong
build/app/pacman-port --no-audio --scale 2 --curve strong --config $D/c1.cfg --no-hiscore --report $D/r1.txt --measure 20 >/dev/null 2>&1 &
P=$!; sleep 3; W=$(xdotool search --pid $P | head -1); xdotool windowfocus $W; sleep 1
k Escape; sleep 1.5; shot $W $D/menu.png; k Escape; sleep 1.5; wait $P
# (2) game: replay, curve off and strong
for c in off strong; do
  build/app/pacman-port --no-audio --scale 2 --curve $c --replay $D/s27.replay --config $D/c2_$c.cfg --no-hiscore --measure 20 >/dev/null 2>&1 &
  P=$!; sleep 12; W=$(xdotool search --pid $P | head -1); shot $W $D/game_$c.png; wait $P
done
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
chk "log: the curve went flat when the menu opened" "$(grep -a -c 'Curve: flat while the menu is open' $D/r1.txt)" 1
chk "log: the curve was on before and came back after the menu closed" "$(grep -a -c 'Curve: on (picture bent)' $D/r1.txt)" 2
python3 - <<'P' || fail=1
import numpy as np, sys
from PIL import Image
L = lambda n: np.asarray(Image.open(f"build/x42m/{n}.png").convert("RGB")).astype(int)
ok = True
def chk(n, g):
    global ok; ok &= bool(g); print(("  ok: " if g else "  FAIL: ") + n)
a = L("menu"); m = (a[:, :, 0] > 200) & (a[:, :, 1] > 200) & (a[:, :, 2] < 90); m[:, int(a.shape[1] * 0.6):] = False       # the yellow menu frame, left 60 % of the window
xs = [np.nonzero(r)[0].min() for r in m if r.sum() > 0]
print(f"  menu open with the curve on: yellow frame on {len(xs)} rows, left side moves by {max(xs) - min(xs)} pixels")
chk("menu open: the yellow menu frame is a straight line although the curve is on", len(xs) > 50 and max(xs) - min(xs) <= 1)
def wall(n):                          # the maze's outer left wall between 10 % and 30 % of the window height: leftmost blue pixel per row (10 to 90 percentile range)
    b = L(n); mm = (b[:, :, 2] > 120) & (b[:, :, 0] < 110) & (b[:, :, 1] < 110); H = b.shape[0]
    v = [np.nonzero(r)[0].min() for r in mm[int(H * .10):int(H * .30)] if r.sum() > 0]; return np.percentile(v, 90) - np.percentile(v, 10), len(v)
(r0, n0), (r1, n1) = wall("game_off"), wall("game_strong")
print(f"  maze left wall spread: curve off {r0:.0f} px ({n0} rows), curve strong {r1:.0f} px ({n1} rows)")
chk("game: with the curve off the wall is straight (spread <= 2)", n0 > 30 and r0 <= 2)
chk("game: with the curve strong the wall bows (spread >= 8)", n1 > 30 and r1 >= 8)
sys.exit(0 if ok else 1)
P
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
