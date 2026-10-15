# X45: writes made-up PNG files (own pictures, good and bad) into the folder given. Usage: make_cabinet_pngs.py DIR
import sys, zlib, struct, os
D = sys.argv[1]; os.makedirs(D, exist_ok=True)
def chunk(t, d): return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
def png(w, h, ctype, depth, rows, plte=None, interlace=0, trns=None, filt=None):
    raw = b""
    for y, r in enumerate(rows):
        f = (filt[y % len(filt)] if filt else 0); ch = {0:1,2:3,3:1,4:2,6:4}[ctype]; bpp = ch * (depth // 8); out = bytearray()
        for i, v in enumerate(r):
            a = r[i-bpp] if i >= bpp else 0; b = rows[y-1][i] if y else 0; c = rows[y-1][i-bpp] if (y and i >= bpp) else 0
            if f == 0: o = v
            elif f == 1: o = v - a
            elif f == 2: o = v - b
            elif f == 3: o = v - (a + b) // 2
            else:
                p = a + b - c; pa, pb, pc = abs(p-a), abs(p-b), abs(p-c); pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c); o = v - pr
            out.append(o & 255)
        raw += bytes([f]) + bytes(out)
    d = struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, interlace)
    s = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", d)
    if plte: s += chunk(b"PLTE", plte)
    if trns: s += chunk(b"tRNS", trns)
    return s + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b"")
def w(n, b): open(os.path.join(D, n), "wb").write(b)
w("rgb.png", png(200, 100, 2, 8, [bytes(sum(([200,30,30] if x < 100 else [30,30,200] for x in range(200)), [])) for _ in range(100)]))
w("rgba.png", png(128, 96, 6, 8, [bytes(sum(([0,0,0,0] if x < 64 else [60,120,60,255] for x in range(128)), [])) for _ in range(96)]))
w("gray.png", png(80, 80, 0, 8, [bytes([80]*80) for _ in range(80)]))
w("grada.png", png(96, 64, 4, 8, [bytes([100, 255] * 96) for _ in range(64)]))
w("pal.png", png(64, 64, 3, 8, [bytes([x % 2 for x in range(64)]) for _ in range(64)], plte=bytes([10,20,30, 200,100,50])))
w("filters.png", png(100, 100, 2, 8, [bytes(sum(([x*2, y*2, (x+y) & 255] for x in range(100)), [])) for y in range(100)], filt=[0,1,2,3,4]))
w("small.png", png(32, 32, 2, 8, [bytes(96) for _ in range(32)]))
w("huge.png", png(9000, 64, 0, 8, [bytes(9000) for _ in range(64)]))
good = png(200, 100, 2, 8, [bytes(600) for _ in range(100)])
bad = bytearray(good); i = bad.index(b"IDAT") + 12; bad[i:i+8] = b"\xff" * 8; w("corrupt.png", bytes(bad))
w("trunc.png", good[:len(good) // 2])
w("interlaced.png", png(100, 100, 2, 8, [bytes(300) for _ in range(100)], interlace=1))
w("deep.png", png(100, 100, 0, 16, [bytes(200) for _ in range(100)]))
w("notpng.png", b"this is only text, not a picture\n" * 10)
print("wrote made-up pictures into", D)
