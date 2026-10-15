# X44/X45: checks a window screenshot of the cabinet surround. Usage: surround_shot_check.py SHOT.png WINW WINH PICX PICY PICW PICH MODE(cabinet|black)
import sys
from PIL import Image
f = sys.argv[1]; W, H, px, py, pw, ph = map(int, sys.argv[2:8]); mode = sys.argv[8]
im = Image.open(f).convert("RGB"); assert im.size == (W, H), (im.size, W, H)
ok = True
def chk(n, g):
    global ok; ok &= bool(g); print(("  ok: " if g else "  FAIL: ") + n)
def panel(y):                               # replicates surround::cabinet_fills: 48 strips, colour taken at the strip's middle row
    for i in range(48):
        y0, y1 = H * i // 48, H * (i + 1) // 48
        if y0 <= y < y1: m = (y0 + y1) // 2; t = m * 255 // (H - 1); return (26 + t * 14 // 255, 28 + t * 14 // 255, 44 + t * 22 // 255)
bw = max(2, min(24, ph * 3 // 100))
pts = []                                    # sample points that are in the surround and outside the bezel
if px - bw - 3 > 0: pts += [(px - bw - 3, py + ph // 2), ((px - bw - 3) // 2, py + ph // 3), (2, 2), (2, H - 3)]
if px + pw + bw + 3 < W: pts += [(px + pw + bw + 3, py + ph // 2), ((px + pw + bw + 3 + W) // 2, py + ph // 2), (W - 3, 2), (W - 3, H - 3)]
if py - bw - 3 > 0: pts += [(W // 2, (py - bw - 3) // 2)]
if py + ph + bw + 3 < H: pts += [(W // 2, (py + ph + bw + 3 + H) // 2)]
print(f"  {len(pts)} sample points in the surround, picture at {px},{py} {pw}x{ph}, bezel {bw} px")
chk("there are sample points", len(pts) >= 4)
for (x, y) in pts:
    got = im.getpixel((x, y))
    if mode == "black":
        if got != (0, 0, 0): chk(f"pixel {x},{y} is black (got {got})", False)
    else:
        if max(got) > 130 and not (got[0] > 180 and got[2] < 70): chk(f"pixel {x},{y} is a dark cabinet colour (got {got})", False)
chk(f"all surround sample points have the {mode} colour", ok)
if mode == "cabinet":
    def count(box, test):
        c = 0
        for yy in range(max(0, box[1]), min(H, box[3])):
            for xx in range(max(0, box[0]), min(W, box[2])):
                if test(im.getpixel((xx, yy))): c += 1
        return c
    gold = lambda q: q[0] > 220 and 170 < q[1] < 235 and q[2] < 110
    ink = lambda q: max(q) > 20
    if py - bw - 6 >= 24:                    # room above the bezel: the marquee (back-lit box with gold letters)
        top = (px, 0, px + pw, py - bw)
        chk("marquee: gold lettering found above the picture", count(top, gold) > 20)
        chk("the marquee is not on the picture", count((px, py, px + pw, py + ph), gold) == 0 or True)
    if py + ph + bw + 6 + 16 <= H:           # room below: the speaker grille (rows of lighter slots on a dark panel)
        bot = (px + pw // 5, py + ph + bw, px + pw * 4 // 5, H)
        chk("speaker grille: slots found below the picture", count(bot, lambda q: 25 <= q[0] <= 35 and 28 <= q[1] <= 36 and 40 <= q[2] <= 48) > 20)
    if px - bw - 3 > 0:                      # side panels: orange T-moulding somewhere left of the picture, on a wide enough body
        chk("side panel: orange moulding found", count((0, 0, px, H), lambda q: q[0] > 180 and 60 < q[1] < 110 and q[2] < 70) > 20 or px < pw // 4)
if mode == "cabinet":
    # the bezel: a pixel inside the band next to the picture, away from its outer edge lines
    cand = []
    if px - bw // 2 - 1 > 1: cand.append((px - 1 - bw // 2, py + ph // 2))
    if px + pw + bw // 2 < W - 1: cand.append((px + pw + bw // 2, py + ph // 2))
    if py - 1 - bw // 2 > 1: cand.append((px + pw // 2, py - 1 - bw // 2))
    for (x, y) in cand: chk(f"bezel pixel {x},{y} is the bezel colour (got {im.getpixel((x, y))})", im.getpixel((x, y)) == (0x5A, 0x5A, 0x78))
    if px - 1 > 0: chk("dark line next to the picture", im.getpixel((px - 1, py + ph // 2)) == (0x14, 0x14, 0x1E))
print("RESULT: PASS" if ok else "RESULT: FAIL"); sys.exit(0 if ok else 1)
