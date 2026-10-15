# X41: screenshots of the same replay tick with glow off / light / strong. Usage: glow_shot_check.py OFF.png LIGHT.png STRONG.png
import sys, numpy as np
from PIL import Image
o, l, s = [np.asarray(Image.open(f).convert("RGB")).astype(int) for f in sys.argv[1:4]]
H, W, _ = o.shape; ok = True
def chk(name, good): 
    global ok; ok &= bool(good); print(("  ok: " if good else "  FAIL: ") + name)
print(f"size {W}x{H}")
chk("light never darker than off", (l >= o).all()); chk("strong never darker than off", (s >= o).all())
chk("light adds light somewhere", (l > o).any()); chk("strong adds more than light", (s - o).sum() > (l - o).sum())
tot = lambda a: a.sum() / o.sum()
print(f"  brightness vs off: light {tot(l):.3f}, strong {tot(s):.3f}")
# pixels far from anything bright stay black: the black bottom-left corner of the picture area (outside the maze) and the cells where off is black and no bright pixel within 6*scale
sc = W / 224; r = int(10 * sc)
bright = o.max(axis=2) > 96
pad = np.pad(bright, r); near = np.zeros_like(bright)
for dy in range(0, 2 * r + 1, 2):
    for dx in range(0, 2 * r + 1, 2): near |= pad[dy:dy + H, dx:dx + W]
far = ~near
chk(f"far from bright parts ({int(far.sum())} pixels) strong glow stays black", (s[far].max() == 0) if far.any() else False)
nearblack = near & (o.max(axis=2) == 0)
chk(f"black pixels near bright parts get brighter ({int((s[nearblack].max(axis=1) > 0).sum())} pixels)", (s[nearblack].max(axis=1) > 0).sum() > 500)
print("RESULT: PASS" if ok else "RESULT: FAIL"); sys.exit(0 if ok else 1)
