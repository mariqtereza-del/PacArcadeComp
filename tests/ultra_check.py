# X46: checks a window screenshot in Ambient mode. Usage: ultra_check.py SHOT W H PICX PICY PICW PICH MODE(ambient|black)
import sys
from PIL import Image
f = sys.argv[1]; W, H, px, py, pw, ph = map(int, sys.argv[2:8]); mode = sys.argv[8]
im = Image.open(f).convert("RGB"); assert im.size == (W, H), (im.size, W, H)
ok = True
def chk(n, g):
    global ok; ok &= bool(g); print(("  ok: " if g else "  FAIL: ") + n)
side = [(x, y) for x in range(2, px - 2, 3) for y in range(2, H - 2, 7)] + [(x, y) for x in range(px + pw + 2, W - 2, 3) for y in range(2, H - 2, 7)]
lum = [sum(im.getpixel(p)) for p in side]
mx = max(lum); lit = sum(1 for v in lum if v > 30)
print(f"  {len(side)} side samples, brightest sum {mx}, lit {lit}")
if mode == "black": chk("sides are black", mx == 0)
else:
    chk("sides are not black (a glow is there)", lit > len(side) // 12)
    chk("sides are dark (darkened copy: no pixel brighter than 70 % of white)", mx < 3 * 255 * 0.7)
    # smooth: neighbouring samples in a row differ little (blurred, not sharp text)
    row = [sum(im.getpixel((x, H // 3))) for x in range(2, max(3, px - 2))]
    jump = max((abs(a - b) for a, b in zip(row, row[1:])), default=0)
    chk(f"blurred: no sharp edges between neighbouring pixels in the side area (biggest step {jump})", jump < 40)
print("RESULT: PASS" if ok else "RESULT: FAIL"); sys.exit(0 if ok else 1)
