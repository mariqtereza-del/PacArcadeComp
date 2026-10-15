"""Throwaway renderer for Phase 3: draws tiles (+ rough sprites) from board RAM using the layouts in Lomont's guide."""
from PIL import Image
def palette(gfx):
    p7f, p4a = gfx['82s123.7f'], gfx['82s126.4a']
    cols = []
    for b in p7f:
        r = (b & 1) * 0x21 + ((b >> 1) & 1) * 0x47 + ((b >> 2) & 1) * 0x97
        g = ((b >> 3) & 1) * 0x21 + ((b >> 4) & 1) * 0x47 + ((b >> 5) & 1) * 0x97
        bl = ((b >> 6) & 1) * 0x51 + ((b >> 7) & 1) * 0xAE
        cols.append((r, g, bl))
    return [[cols[p4a[pal * 4 + i] & 0x0F] for i in range(4)] for pal in range(64)]

def tile_pixels(rom, code):
    """8x8, 2 bits per pixel. Layout per MAME/Lomont: 16 bytes; x 4..7 in bytes 0..7, x 0..3 in bytes 8..15; high nibble = MSB plane."""
    t = rom[code * 16:(code + 1) * 16]
    px = [[0] * 8 for _ in range(8)]
    for y in range(8):
        for x in range(8):
            b = t[y] if x >= 4 else t[y + 8]
            k = x - 4 if x >= 4 else x
            lo = (b >> (3 - k)) & 1          # low nibble = LSB plane
            hi = (b >> (7 - k)) & 1          # high nibble = MSB plane
            px[y][x] = hi * 2 + lo
    return px

def sprite_pixels(rom, code):
    s = rom[code * 64:(code + 1) * 64]
    xo = [8*8+0, 8*8+1, 8*8+2, 8*8+3, 16*8, 16*8+1, 16*8+2, 16*8+3, 24*8, 24*8+1, 24*8+2, 24*8+3, 0, 1, 2, 3]
    yo = [i * 8 for i in range(8)] + [32*8 + i * 8 for i in range(8)]
    px = [[0] * 16 for _ in range(16)]
    def bit(off):   # MAME numbering: bit 0 = MSB of byte 0
        return (s[off >> 3] >> (7 - (off & 7))) & 1
    for y in range(16):
        for x in range(16):
            px[y][x] = bit(xo[x] + yo[y]) * 2 + bit(xo[x] + yo[y] + 4)
    return px

def tile_offset(col, row):
    """Native (unrotated) tilemap, 36 columns x 28 rows -> video RAM offset (the board's odd addressing)."""
    r = row + 2; c = col - 2
    return r + ((c & 0x1f) << 5) if c & 0x20 else c + (r << 5)

def render(mem, gfx, sprites=True, flipxbit=1, flipybit=2, variant='A'):
    """Draw in the board's native 288x224 orientation, then rotate 90 degrees clockwise (the monitor was mounted sideways)."""
    pal = palette(gfx); img = Image.new('RGB', (288, 224), (0, 0, 0)); P = img.load()
    for row in range(28):
        for col in range(36):
            o = tile_offset(col, row)
            code, attr = mem[0x4000 + o], mem[0x4400 + o]
            colors = pal[attr & 0x3F]
            px = tile_pixels(gfx['pacman.5e'], code)
            for y in range(8):
                for x in range(8):
                    P[col * 8 + x, row * 8 + y] = colors[px[y][x]]
    if sprites:
        for n in range(7, -1, -1):
            a, col = mem[0x4FF0 + n * 2], mem[0x4FF1 + n * 2]
            xr, yr = mem[0x5060 + n * 2], mem[0x5061 + n * 2]
            sx, sy = (272 - yr, xr - 31) if variant == 'A' else (272 - xr, yr - 31)
            colors = pal[col & 0x3F]
            px = sprite_pixels(gfx['pacman.5f'], a >> 2)
            fx, fy = a & flipxbit, a & flipybit
            for yy in range(16):
                for xx in range(16):
                    v = px[yy if not fy else 15 - yy][xx if not fx else 15 - xx]
                    X, Y = sx + xx, sy + yy
                    if v and 0 <= X < 288 and 0 <= Y < 224: P[X, Y] = colors[v]
    return img.rotate(-90, expand=True)
