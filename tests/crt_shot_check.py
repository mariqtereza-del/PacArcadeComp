# X40: compares two window screenshots (mask off / mask on) of the same replay tick. Usage: crt_shot_check.py OFF.png ON.png LEVEL(light|strong)
import sys, numpy as np
from PIL import Image
a = np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(float); b = np.asarray(Image.open(sys.argv[2]).convert("RGB")).astype(float)
k = {"light": 215, "strong": 150}[sys.argv[3]]; H, W, _ = a.shape; ok = True
print(f"size {W}x{H}")
bright = a.max(axis=2) >= 200                                  # strongly lit pixels of the unmasked picture (so the ratio is measurable)
print("lit pixels:", int(bright.sum()))
if bright.sum() < 500: print("FAIL: too few lit pixels"); sys.exit(1)
cols = np.arange(W) % 3
for c in range(3):                                             # per colour channel and column class: ratio on / off
    for cls in range(3):
        sel = bright & (cols[None, :] == cls)
        # only channels that are lit in the unmasked picture
        sel = sel & (a[:, :, c] >= 200)
        if sel.sum() < 50: continue
        r = b[:, :, c][sel].sum() / a[:, :, c][sel].sum()
        want = 1.0 if cls == c else k / 255.0
        good = abs(r - want) < 0.02
        ok &= good
        print(f"channel {'RGB'[c]} column class {cls}: ratio {r:.3f} wanted {want:.3f} {'ok' if good else 'FAIL'}")
tot = b.sum() / a.sum(); want = (255 + 2 * k) / 3 / 255
good = abs(tot - want) < 0.03; ok &= good
print(f"whole picture brightness ratio {tot:.3f} wanted about {want:.3f} {'ok' if good else 'FAIL'}")
black = a.max(axis=2) == 0
print("black stays black:", "ok" if (b[black].max() == 0) else "FAIL"); ok &= (b[black].max() == 0)
print("RESULT: PASS" if ok else "RESULT: FAIL"); sys.exit(0 if ok else 1)
