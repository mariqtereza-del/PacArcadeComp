# X42: window screenshots of the same replay tick with curve off / light / strong. Usage: curve_shot_check.py OFF.png LIGHT.png STRONG.png
import sys, numpy as np
from PIL import Image
imgs = {n: np.asarray(Image.open(f).convert("RGB")).astype(int) for n, f in zip(("off", "light", "strong"), sys.argv[1:4])}
o = imgs["off"]; H, W, _ = o.shape; ok = True; K = {"light": 0.04, "strong": 0.09}; A = {"light": 0.25, "strong": 0.5}
def chk(name, good):
    global ok; ok &= bool(good); print(("  ok: " if good else "  FAIL: ") + name)
print(f"size {W}x{H}")
rng = np.random.default_rng(1)
ys, xs = np.nonzero(o[:, :, 2] > 200)                       # the blue maze walls of the unbent picture
pick = rng.choice(len(ys), 400, replace=False)
for name in ("light", "strong"):
    c = imgs[name]; k = K[name]; hit = 0; tot = 0
    bright = c.max(axis=2)
    for i in pick:
        y, x = ys[i], xs[i]; px, py = 2 * (x + .5) / W - 1, 2 * (y + .5) / H - 1
        f = (1 + k * (2 - px * px - py * py)) / (1 + 2 * k); sx = int((px * f + 1) / 2 * W); sy = int((py * f + 1) / 2 * H)
        r = 3; win = bright[max(0, sy - r):sy + r + 1, max(0, sx - r):sx + r + 1]
        tot += 1; hit += int(win.max() > 40)
    chk(f"{name}: {hit} of {tot} sampled wall pixels found where the curve formula puts them", hit >= 0.97 * tot)
    # the middle of the picture is untouched (vignette 1.0, scale 1): compare a central block
    cy, cx = H // 2, W // 2; b = 24
    d = np.abs(c[cy - b:cy + b, cx - b:cx + b] - o[cy - b:cy + b, cx - b:cx + b]).mean()
    print(f"  centre block mean difference {d:.2f}"); chk(f"{name}: the centre block is nearly untouched (< 12 of 255 on average)", d < 12)
    # corners: the picture corner region is now black or darker, and an edge wall is darker (vignette)
    edge = c[H // 2 - 20:H // 2 + 20, 0:int(W * (0.18 + k))].astype(float).sum(); eo = o[H // 2 - 20:H // 2 + 20, 0:int(W * (0.18 + k))].astype(float).sum()
    print(f"  left edge strip brightness vs off: {edge / eo:.2f}")
    chk(f"{name}: the picture is darker towards the edge", edge < eo)
    # shape: leftmost lit column in the middle row band is further right than in the off picture, by about the formula
    band = slice(H // 2 - 10, H // 2 + 10)
    def left(a): cols = np.nonzero((a[band].max(axis=2) > 60).any(axis=0))[0]; return cols.min()
    lo, lc = left(o), left(c); px, py = 2 * (lo + .5) / W - 1, 0.0; f = (1 + k * (2 - px * px - py * py)) / (1 + 2 * k); want = (px * f + 1) / 2 * W
    print(f"  left wall at the middle row: off x={lo}, {name} x={lc}, formula says about {want:.0f}"); chk(f"{name}: the left edge bows by the formula (within 6 pixels)", abs(lc - want) <= 6)
chk("strong is darker overall than light, and light than off (vignette)", imgs["strong"].sum() < imgs["light"].sum() < o.sum())
print("RESULT: PASS" if ok else "RESULT: FAIL"); sys.exit(0 if ok else 1)
