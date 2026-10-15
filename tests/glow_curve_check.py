# X41 fix (reported by the user with a screenshot): with the curve on, the glow must sit exactly where it sits without the curve, only bent with the picture.
# Usage: glow_curve_check.py FLAT_OFF.png FLAT_GLOW.png CURVE_OFF.png CURVE_GLOW.png K [shift|scale]   (K = curve strength 0.04 light / 0.09 strong; the last argument is a self test:
# the curved glow is deliberately misplaced and the check must FAIL).
# glow_flat = FLAT_GLOW - FLAT_OFF; glow_curve = CURVE_GLOW - CURVE_OFF; the flat glow is bent with the curve formula and compared with glow_curve (normalised correlation).
import sys, numpy as np
from PIL import Image
L = lambda f: np.asarray(Image.open(f).convert("RGB")).astype(float)
fo, fg, co, cg = L(sys.argv[1]), L(sys.argv[2]), L(sys.argv[3]), L(sys.argv[4]); k = float(sys.argv[5]); mode = sys.argv[6] if len(sys.argv) > 6 else ""
gf = (fg - fo).sum(axis=2); gc = (cg - co).sum(axis=2); H, W = gf.shape
if mode == "shift": gc = np.roll(gc, 45, axis=1)
if mode == "scale":
    ys, xs = np.mgrid[0:H, 0:W]; gc = gc[((ys - H / 2) / 1.57 + H / 2).astype(int).clip(0, H - 1), ((xs - W / 2) / 1.57 + W / 2).astype(int).clip(0, W - 1)]
ys, xs = np.mgrid[0:H, 0:W]; sx = 2 * (xs + .5) / W - 1; sy = 2 * (ys + .5) / H - 1; px, py = sx.copy(), sy.copy()
for _ in range(60):
    f = (1 + k * (2 - px * px - py * py)) / (1 + 2 * k); px, py = sx / f, sy / f                 # curve_unmap (crt.h)
ix = ((px + 1) / 2 * W).astype(int); iy = ((py + 1) / 2 * H).astype(int); inside = (ix >= 0) & (ix < W) & (iy >= 0) & (iy < H)
bent = np.where(inside, gf[iy.clip(0, H - 1), ix.clip(0, W - 1)], 0)
m = inside
x = gc[m] - gc[m].mean(); y = bent[m] - bent[m].mean(); corr = float((x * y).sum() / np.sqrt((x * x).sum() * (y * y).sum()))
print(f"  correlation of the curved glow with the flat glow bent by the formula: {corr:.3f}")
ok = corr >= 0.75
print(("  ok: " if ok else "  FAIL: ") + "the glow sits where it belongs (correlation >= 0.75)")
print("RESULT: PASS" if ok else "RESULT: FAIL"); sys.exit(0 if ok else 1)
