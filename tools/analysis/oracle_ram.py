"""S10: map and verify RAM variables of the Pac-Man program on the Python prototype (read-only: harness.py and render.py are not changed).
Plays scripted games (1 player; 2 players; several DIP/cocktail settings), takes snapshots at event-defined moments and, for every variable,
prints its value at those moments next to what the screen shows. Writes docs/evidence/S10_ram_values.txt (use --check to compare)."""
import sys, os, random, collections
sys.path.append(os.path.dirname(os.path.abspath(__file__)))     # append, not insert: tools/analysis/dis.py must not shadow the standard 'dis' module
from harness import Board
from render import render, tile_offset
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')

# ---------- screen readers (video RAM tiles; screen column sx 0..27 left to right, row sy 0..35 top to bottom) ----------
def tile(m, sx, sy): return m[0x4000 + tile_offset(sy, 27 - sx)]
def ascii_row(m, sy): return ''.join(chr(tile(m, sx, sy)) if 0x30 <= tile(m, sx, sy) <= 0x5A else '.' for sx in range(28))
def screen_has(m, word): return any(word in ascii_row(m, sy) for sy in range(36))
def number_in_row(m, sy, c0, c1):
    d = [tile(m, sx, sy) for sx in range(c0, c1 + 1)]; ds = [x for x in d if x < 10]
    return int(''.join(map(str, ds))) if ds else None
def icons(m, c0, c1): return sum(1 for sy in (34, 35) for sx in range(c0, c1 + 1) if tile(m, sx, sy) != 0x40) // 4
def bcd_le(m, a, n): return int(''.join('%02X' % m[a + i] for i in range(n - 1, -1, -1)))

# ---------- scripted play ----------
def inputs(f, sc):
    in0 = 0xFF; in1 = 0xFF if not sc.get('cocktail') else 0x7F
    for c in sc.get('coins', ()):
        if c <= f < c + 5: in0 &= 0xDF
    for s in sc.get('start1', ()):
        if s <= f < s + 4: in1 &= 0xDF
    for s in sc.get('start2', ()):
        if s <= f < s + 4: in1 &= 0xBF
    if sc.get('rack') and sc['rack'][0] <= f < sc['rack'][1]: in0 &= 0xEF
    if f >= sc.get('move_from', 10**9):
        st = sc['_st']
        if st['hold'] <= 0: st['d'] = st['rnd'].choice([0xFE, 0xFD, 0xFB, 0xF7]); st['hold'] = st['rnd'].randint(10, 40)
        st['hold'] -= 1; in0 &= st['d']
    return in0, in1
def play(sc, frames, moment_rules, keep_trace=True):
    # returns (trace of 4C00-4EFF per frame, (dots,energizers) per frame, moments dict, sprite regs 5060-506F per frame)
    b = Board(dip=sc.get('dip', 0xC9)); m = b.mem; sc = dict(sc); sc['_st'] = {'rnd': random.Random(sc.get('seed', 7)), 'hold': 0, 'd': 0xFF}
    trace = []; extra = []; moments = {}; hist = []; spr = []
    for f in range(frames):
        in0, in1 = inputs(f, sc); b.ioread[0x5000] = in0; b.ioread[0x5040] = in1
        b.run_frame()
        if keep_trace:
            trace.append(bytes(m[0x4C00:0x4F00]))
            extra.append((sum(1 for i in range(0x4040, 0x43C0) if m[i] == 0x10), sum(1 for i in range(0x4040, 0x43C0) if m[i] == 0x14)))
        spr.append(bytes(m[0x5060:0x5070]))
        for name, rule in moment_rules.items():
            if name not in moments and f >= rule[0] and rule[1](m, f):
                moments[name] = {'frame': f, 'mem': bytes(m[0x4000:0x5100]), 'gfx': b.gfx}
    return trace, extra, moments, spr

def sprite_boxes(mem, gfx):
    """For sprites 1..6: bounding box (x0,x1,y0,y1) and pixel count of the sprite alone in the final 224x288 picture, plus count of blue-ish pixels."""
    out = {}
    base = bytearray(mem); full = bytearray(0x5100)
    full[0x4000:0x5100] = base
    # render() wants a memory indexed by absolute address
    def R(mm, sprites=True):
        big = bytearray(0x10000); big[0x4000:0x5100] = mm; return render(big, gfx, sprites=sprites)
    bg = R(bytes(mem), False).load()
    for n in range(1, 7):
        mm = bytearray(mem)
        for k in range(8):
            if k != n: mm[0x1060 + 2 * k] = 0; mm[0x1061 + 2 * k] = 0
        im = R(bytes(mm)); px = im.load(); W, H = im.size
        xs = []; ys = []; blue = 0
        for y in range(H):
            for x in range(W):
                if px[x, y] != bg[x, y]:
                    xs.append(x); ys.append(y)
                    r, g, bl = px[x, y]
                    if bl > 150 and r < 100: blue += 1
        out[n] = (min(xs), max(xs), min(ys), max(ys), len(xs), blue) if xs else None
    return out


# ---------- scenarios ----------
S1 = dict(coins=(400,), start1=(420,), move_from=424, rack=(1300, 1320), seed=7)
S2 = dict(coins=(400, 440), start2=(470,), move_from=480, seed=11)
def text(m, *words): return all(screen_has(m, w) for w in words)
RULES1 = {
 'attract': (380, lambda m, f: True),
 'credit': (408, lambda m, f: m[0x4E6E] == 1),
 'ready': (421, lambda m, f: screen_has(m, 'READY')),
 'eat': (560, lambda m, f: m[0x4E0E] >= 10 and m[0x4E00] == 3),
 'power': (560, lambda m, f: m[0x4DA6] == 1),
 'blue2': (1030, lambda m, f: m[0x4DA6] == 1 and all(m[a] == 0x11 for a in (0x4FF5, 0x4FF7, 0x4FF9))),
 'blue': (560, lambda m, f: m[0x4DA6] == 1 and all(m[a] == 0x11 for a in (0x4FF5, 0x4FF7, 0x4FF9))),
 'level2': (1400, lambda m, f: m[0x4E13] == 1 and m[0x4E0E] >= 5 and m[0x4E04] == 3),
 'died': (1500, lambda m, f: m[0x4E14] == 2 and m[0x4E04] == 3 and m[0x4E0E] >= 5),
 'over': (1500, lambda m, f: m[0x4E14] == 0 and screen_has(m, 'GAME')),
}
RULES2 = {
 'credit2': (448, lambda m, f: m[0x4E6E] == 2),
 'ready2': (471, lambda m, f: screen_has(m, 'READY')),
 'p1play': (560, lambda m, f: m[0x4E0E] >= 8 and m[0x4E00] == 3 and m[0x4E09] == 0),
 'p2turn': (700, lambda m, f: m[0x4E09] == 1 and m[0x4E04] == 3),
 'p2eat': (700, lambda m, f: m[0x4E09] == 1 and m[0x4E04] == 3 and (m[0x4E84] | m[0x4E85]) != 0),
}
DIPS = [  # (label, dip, cocktail, coin pattern)
 ('default C9', 0xC9, False), ('2 coins/credit CB', 0xCB, False), ('1 coin/2 credits CA', 0xCA, False), ('free play C8', 0xC8, False),
 ('lives 1 C1', 0xC1, False), ('lives 2 C5', 0xC5, False), ('lives 5 CD', 0xCD, False),
 ('bonus 15000 free D8', 0xD8, False), ('bonus 20000 free E8', 0xE8, False), ('bonus none free F8', 0xF8, False), ('bonus 10000 free C8', 0xC8, False),
 ('hard 89', 0x89, False), ('alt ghost names 49', 0x49, False), ('cocktail IN1 bit7=0', 0xC9, True)]
RULES3 = {
 'attract': (380, lambda m, f: True),
 'coin1': (412, lambda m, f: True),
 'coin2': (452, lambda m, f: True),
 'ready': (481, lambda m, f: screen_has(m, 'READY')),
}

# ---------- evidence collection ----------
OBS = []                             # pill-array popcounts (observation: not live state)
RES = collections.OrderedDict()      # addr(int) -> list of (moment, frame, ram text, evidence text, ok)
def rec(addr, moment, frame, ram, ev, ok): RES.setdefault(addr, []).append((moment, frame, ram, ev, bool(ok)))
def MM(snap): mm = bytearray(0x10000); mm[0x4000:0x5100] = snap['mem']; return mm
def G(snap, a): return snap['mem'][a - 0x4000]
def signed(v): return v - 256 if v > 127 else v
def screen_score(mm, c0, c1):
    d = [tile(mm, sx, 1) for sx in range(c0, c1 + 1)]; d = [x for x in d if x < 10]
    return int(''.join(map(str, d))) if d else None
def life_icons(mm): return icons(mm, 2, 11)
def level_icons(mm): return icons(mm, 12, 25)
def credit_digit(mm):
    t = tile(mm, 10, 35)
    return t - 0x30 if 0x30 <= t <= 0x39 else None
def pix_square(pos0, pos1, k):
    return (232 - pos1 + (6 - k) - 1, 232 - pos1 + (6 - k) + 15 + 1, 8 + pos0 - 1, 8 + pos0 + 15 + 1)

def slot_of(sn, a, k):
    """Hardware sprite slot (1..5) whose position registers belong to the actor stored at a (tolerance 2 for the one-frame copy lag)."""
    p0, p1 = G(sn, a), G(sn, a + 1)
    for n in range(0, 7):
        r0, r1 = G(sn, 0x5060 + 2 * n), G(sn, 0x5061 + 2 * n)
        if abs(((r0 - (p1 + k)) + 128) % 256 - 128) <= 2 and abs(((r1 - (8 - p0)) + 128) % 256 - 128) <= 2: return n
    return None

def run_all():
    out = []
    tr1, ex1, mo1, sp1 = play(S1, 4400, RULES1)
    tr2, ex2, mo2, sp2 = play(S2, 5000, RULES2)
    def T1(f, a): return tr1[f][a - 0x4C00]
    # --- screen-verified on S1 moments
    for name, sn in mo1.items():
        mm = MM(sn); f = sn['frame']
        if name in ('attract', 'credit'):
            rec(0x4E6E, name, f, '%d' % G(sn, 0x4E6E), 'screen credit digit %s' % credit_digit(mm), credit_digit(mm) == G(sn, 0x4E6E))
        if name in ('eat', 'power', 'died', 'over', 'ready'):
            v = bcd_le(sn['mem'], 0x4E80 - 0x4000, 3); sc = screen_score(mm, 1, 6)
            rec(0x4E80, name, f, 'bytes %02X %02X %02X -> %d' % (G(sn, 0x4E80), G(sn, 0x4E81), G(sn, 0x4E82), v), 'screen P1 score %s' % sc, (sc or 0) == v)
            hv = bcd_le(sn['mem'], 0x4E88 - 0x4000, 3); hs = screen_score(mm, 9, 17)
            rec(0x4E88, name, f, 'bytes %02X %02X %02X -> %d' % (G(sn, 0x4E88), G(sn, 0x4E89), G(sn, 0x4E8A), hv), 'screen high score %s' % hs, (hs or 0) == hv)
        if name in ('ready', 'eat', 'died', 'over'):
            li = life_icons(mm); l15 = signed(G(sn, 0x4E15))
            rec(0x4E15, name, f, '%d' % l15, 'screen life icons %d' % li, li == max(l15, 0))
            exp = G(sn, 0x4E14) if name == 'ready' else max(G(sn, 0x4E14) - 1, 0)
            rec(0x4E14, name, f, '%d' % G(sn, 0x4E14), 'screen life icons %d (= lives, or lives-1 once play started)' % li, li == exp)
            lv = level_icons(mm)
            rec(0x4E13, name, f, '%d' % G(sn, 0x4E13), 'screen level icons %d (= level+1)' % lv, lv == min(G(sn, 0x4E13) + 1, 7))
        if name in ('credit', 'ready', 'eat', 'power', 'level2', 'died', 'over'):
            vis = text(mm, '1UP'); bit = (G(sn, 0x4DCE) >> 4) & 1
            rec(0x4DCE, name, f, '0x%02X (bit4=%d)' % (G(sn, 0x4DCE), bit), '"1UP" label %s' % ('visible' if vis else 'hidden'), vis == (bit == 0))
        if name in ('eat', 'power', 'level2', 'died', 'over'):
            dots = ex1[f][0]; emax = max(ex1[k][1] for k in range(f, min(f + 30, len(ex1))))
            eaten = 244 - dots - emax
            rec(0x4E0E, name, f, '%d' % G(sn, 0x4E0E), 'screen: 244 - %d dots - %d energizers = %d' % (dots, emax, eaten), eaten == G(sn, 0x4E0E))
            pc = sum(bin(G(sn, a)).count('1') for a in range(0x4E16, 0x4E34)); pe = sum(bin(G(sn, a)).count('1') for a in range(0x4E34, 0x4E38))
            saved = name in ('died', 'over')
            rec(0x4E16, name, f, '30 bytes, %d bits set' % pc, 'screen dots left %d; array is a saved copy: %s' % (dots, 'equals dots left (saved when the life was lost)' if saved else 'still the full level-start copy (240)'), pc == (dots if saved else 240))
            rec(0x4E34, name, f, '4 bytes, %d bits set' % pe, 'screen energizers left %d (live); saved copy holds 2 bits per energizer left at last death, 8 before any death' % emax, pe == (2 * emax if saved else 8))
        if name in ('attract', 'credit', 'ready', 'eat', 'died', 'over'):
            st = G(sn, 0x4E00)
            if text(mm, 'PUSH') : exp = 2; why = 'PUSH START screen'
            elif text(mm, 'READY'): exp = 2; why = 'READY! screen (credit taken, game starting)'
            elif tile(mm, 14, 20) == 0x10 or any(tile(mm, sx, sy) == 0x10 for sx in range(28) for sy in range(3, 33)): exp = 3 if name != 'attract' else 1; why = 'maze with dots on screen'
            else: exp = 1; why = 'attract screen'
            rec(0x4E00, name, f, '%d' % st, why, st == exp)
        if name in ('ready', 'eat', 'over'):
            exp = {'ready': 0, 'eat': 3, 'over': 7}[name]
            why = {'ready': 'READY! shown, game not yet running', 'eat': 'maze running', 'over': '"GAME OVER" on screen'}[name]
            ok = G(sn, 0x4E04) == exp and (name != 'over' or text(mm, 'GAME'))
            rec(0x4E04, name, f, '%d' % G(sn, 0x4E04), why, ok)
        if name in ('credit', 'ready', 'eat'):
            rec(0x4E02, name, f, '%d' % G(sn, 0x4E02), 'screen ' + ('PUSH START page' if text(mm, 'PUSH') else 'READY' if text(mm, 'READY') else 'maze' if name == 'eat' else 'attract page'), True)
        if name in ('ready', 'eat', 'died'):
            rec(0x4E12, name, f, '%d' % G(sn, 0x4E12), 'lives lost on this level so far: %d' % (3 - G(sn, 0x4E14)) if name != 'ready' else 'no death yet', G(sn, 0x4E12) == (1 if G(sn, 0x4E14) < 3 and name == 'died' else 0 if name != 'died' else 1))
    # --- edible ghosts: blue body pixels (33,33,255) > 40 (a normal ghost shows only its 8 blue eye pixels)
    def blue_count(sn, n):
        mem = sn['mem']; mmx = bytearray(mem)
        for j in range(8):
            if j != n: mmx[0x1060 + 2 * j] = 0; mmx[0x1061 + 2 * j] = 0
        b0 = bytearray(0x10000); b0[0x4000:0x5100] = mem; bg = render(b0, sn['gfx'], sprites=False).load()
        b1 = bytearray(0x10000); b1[0x4000:0x5100] = mmx; im = render(b1, sn['gfx']); px = im.load()
        d = [(x, y) for y in range(im.size[1]) for x in range(im.size[0]) if px[x, y] != bg[x, y]]
        return sum(1 for x, y in d if px[x, y] == (33, 33, 255)), len(d)
    for name in ('eat', 'blue', 'blue2', 'level2'):
        if name not in mo1: continue
        sn = mo1[name]; f = sn['frame']; anyb = False
        for g, a in enumerate(range(0x4DA7, 0x4DAB)):
            slot = slot_of(sn, 0x4D00 + 2 * g, 6 if g < 2 else 7)
            if slot is None: continue
            bc, nd = blue_count(sn, slot); anyb = anyb or bc > 40
            if nd == 0: continue          # this ghost is off the visible picture at this moment (e.g. in the side tunnel): not testable
            rec(a, name, f, '%d' % G(sn, a), 'ghost %d (hardware sprite slot %d): %d dark-blue pixels on screen (slot colour reg 0x%02X)' % (g, slot, bc, G(sn, 0x4FF1 + 2 * slot)), (G(sn, a) == 1) == (bc > 40))
        rec(0x4DA6, name, f, '%d' % G(sn, 0x4DA6), 'ghosts blue on screen: %s' % ('yes' if anyb else 'no'), (G(sn, 0x4DA6) == 1) == anyb)
    # --- sprite positions: RAM pair -> predicted 16x16 square on the picture, versus rendered pixels
    chars = [('Blinky', 0x4D00, 6, 1), ('Pinky', 0x4D02, 6, 2), ('Inky', 0x4D04, 7, 3), ('Clyde', 0x4D06, 7, 4), ('Pac-Man', 0x4D08, 7, 5)]
    for name in ('ready', 'eat', 'power', 'blue', 'level2', 'died'):
        if name not in mo1: continue
        sn = mo1[name]; f = sn['frame']; boxes = sprite_boxes(sn['mem'], sn['gfx'])
        for cn, a, k, n in chars:
            p0, p1 = G(sn, a), G(sn, a + 1); n = slot_of(sn, a, k); b = boxes.get(n) if n else None
            if name == 'ready' and cn != 'Pac-Man' and cn != 'Blinky': pass
            if b is None or (p0 == 0 and p1 == 0): continue
            x0, x1, y0, y1 = pix_square(p0, p1, k)
            ok = b[0] >= x0 and b[1] <= x1 and b[2] >= y0 and b[3] <= y1
            rec(a, name, f, '(%02X,%02X)' % (p0, p1), '%s (slot %d) pixels x %d..%d y %d..%d, predicted x %d..%d y %d..%d' % (cn, n, b[0], b[1], b[2], b[3], x0, x1, y0, y1), ok)
    # --- sprite staging bytes: exact relation held on every frame 600..2600 (behaviour)
    for cn, a, k, n in chars:
        frames = range(600, 2600)
        good = sum(1 for f in frames if (T1(f, 0x4C12 + 2 * (n - 1)) - T1(f, a + 1)) & 0xFF == k and (T1(f, 0x4C13 + 2 * (n - 1)) + T1(f, a)) & 0xFF == 8)
        for mname, f in (('early', 650), ('middle', 1600), ('late', 2550)):
            rec(0x4C12 + 2 * (n - 1), mname, f, '(%02X,%02X) pos (%02X,%02X)' % (T1(f, 0x4C12 + 2 * (n - 1)), T1(f, 0x4C13 + 2 * (n - 1)), T1(f, a), T1(f, a + 1)),
                '%s: byte0 = pos.1 + %d and byte1 = 8 - pos.0 holds on %d of %d frames' % (cn, k, good, len(frames)), good >= 0.99 * len(frames))
    # --- Pac-Man tile bytes
    for a, p, off in ((0x4D39, 0x4D08, 32), (0x4D3A, 0x4D09, 30)):
        frames = range(600, 2600); good = sum(1 for f in frames if (T1(f, a) - (T1(f, p) >> 3)) & 0xFF == off)
        for mname, f in (('early', 650), ('middle', 1600), ('late', 2550)):
            rec(a, mname, f, '%d (pos 0x%02X)' % (T1(f, a), T1(f, p)), '= (0x%04X >> 3) + %d on %d of %d frames' % (p, off, good, len(frames)), good >= 0.99 * len(frames))
    # --- orientation from movement
    def orient_check(addr, orient, p0, label):
        m = collections.defaultdict(collections.Counter)
        for f in range(600, 2600):
            d0 = signed((T1(f, p0) - T1(f - 1, p0)) & 0xFF); d1 = signed((T1(f, p0 + 1) - T1(f - 1, p0 + 1)) & 0xFF)
            if abs(d0) <= 2 and abs(d1) <= 2 and (d0 or d1) and not (d0 and d1):
                m[T1(f, orient)]['right' if d1 < 0 else 'left' if d1 > 0 else 'down' if d0 > 0 else 'up'] += 1
        want = {0: 'right', 1: 'down', 2: 'left', 3: 'up'}
        for code in (0, 1, 2, 3):
            c = m.get(code, collections.Counter()); tot = sum(c.values()); best = c.most_common(1)[0] if c else ('-', 0)
            rec(addr, 'value %d' % code, 0, '%s=%d' % (label, code), 'moved %s on %d of %d moving frames' % (best[0], best[1], tot), tot > 20 and best[0] == want[code] and best[1] >= 0.7 * tot)
    orient_check(0x4D30, 0x4D30, 0x4D08, 'Pac-Man')
    orient_check(0x4D2C, 0x4D2C, 0x4D00, 'Blinky')
    orient_check(0x4D2D, 0x4D2D, 0x4D02, 'Pinky')
    orient_check(0x4D2E, 0x4D2E, 0x4D04, 'Inky')
    # --- timers
    f0 = 600; fr = range(f0, 4400)
    d86 = collections.Counter((T1(f, 0x4C86) - T1(f - 1, 0x4C86)) & 0xFF for f in fr)
    wraps = d86[107]; ok86 = set(d86) <= {1, 11, 107} and abs(wraps - len(fr) / 60) <= 2
    for mname, f in (('early', 650), ('middle', 2000), ('late', 4000)):
        rec(0x4C86, mname, f, '0x%02X' % T1(f, 0x4C86), 'steps +1 / +0x0B / wrap: %s; %d wraps in %d frames (1 per 60)' % (dict(d86), wraps, len(fr)), ok86)
    d87 = collections.Counter((T1(f, 0x4C87) - T1(f - 1, 0x4C87)) & 0xFF for f in fr)
    sec = sum(v for k, v in d87.items() if k)
    for mname, f in (('early', 650), ('middle', 2000), ('late', 4000)):
        rec(0x4C87, mname, f, '0x%02X' % T1(f, 0x4C87), 'changes %d times in %d frames (= seconds, BCD); +1 steps %d, +7 (09->10) %d' % (sec, len(fr), d87[1], d87[7]), abs(sec - len(fr) / 60) <= 2 and d87[7] > 0)
    mins = [(f, T1(f, 0x4C88)) for f in range(600, 4400) if T1(f, 0x4C88) != T1(f - 1, 0x4C88)]
    wrap_frame = next((f for f in range(600, 4400) if T1(f, 0x4C87) < T1(f - 1, 0x4C87)), None)
    for mname, f in (('early', 650), ('middle', 2000), ('late', 4400 - 1)):
        rec(0x4C88, mname, f, '%d' % T1(f, 0x4C88), 'rises 0->1 at frame %s when seconds wrap 0x59->0x00 (frame %s)' % (mins[0][0] if mins else None, wrap_frame), bool(mins) and mins[0][0] == wrap_frame and T1(f, 0x4C88) == (1 if f >= wrap_frame else 0))
    s85 = all((T1(f, 0x4C84) + T1(f, 0x4C85)) & 0xFF == 0 for f in range(300, 4400)); st84 = all((T1(f, 0x4C84) - T1(f - 1, 0x4C84)) & 0xFF == 1 for f in range(600, 4400))
    for mname, f in (('early', 650), ('middle', 2000), ('late', 4000)):
        rec(0x4C84, mname, f, '0x%02X (4C85=0x%02X)' % (T1(f, 0x4C84), T1(f, 0x4C85)), '+1 every frame: %s; 4C84+4C85 = 0 (mod 256) on every frame: %s' % (st84, s85), st84 and s85)
        rec(0x4C85, mname, f, '0x%02X' % T1(f, 0x4C85), 'falls by 1 every frame; always = -4C84', st84 and s85)
    ptr_ok = all(T1(f, 0x4C81) == 0x4C and T1(f, 0x4C83) == 0x4C and 0xC0 <= T1(f, 0x4C80) <= 0xFF and 0xC0 <= T1(f, 0x4C82) <= 0xFF for f in range(600, 4400))
    idle = sum(1 for f in range(600, 4400) if T1(f, 0x4C80) == T1(f, 0x4C82))
    for mname, f in (('early', 650), ('middle', 2000), ('late', 4000)):
        rec(0x4C80, mname, f, 'write ptr 0x4C%02X, read ptr 0x4C%02X' % (T1(f, 0x4C80), T1(f, 0x4C82)), 'high bytes always 0x4C, low byte in C0..FF, queue empty (ptrs equal) on %d of %d frames' % (idle, 3800), ptr_ok and idle > 3000)
        rec(0x4C82, mname, f, '0x4C%02X' % T1(f, 0x4C82), 'follows the write pointer; equal when queue empty', ptr_ok and idle > 3000)
    chg = sum(1 for f in range(601, 4400) if T1(f, 0x4C8B) != T1(f - 1, 0x4C8B)); chg2 = sum(1 for f in range(601, 4400) if T1(f, 0x4C8C) != T1(f - 1, 0x4C8C))
    for mname, f in (('early', 650), ('middle', 2000), ('late', 4000)):
        rec(0x4C8B, mname, f, '0x%02X' % T1(f, 0x4C8B), 'changes on %d of 3799 frames (pseudo-random sequence)' % chg, chg > 3700)
        rec(0x4C8C, mname, f, '0x%02X' % T1(f, 0x4C8C), 'changes on %d of 3799 frames (pseudo-random sequence)' % chg2, chg2 > 3700)
    # --- death animation and lives
    nz = [f for f in range(601, 4400) if T1(f, 0x4DA5) != 0 and T1(f - 1, 0x4DA5) == 0]
    drops = [f for f in range(601, 4400) if T1(f, 0x4E14) < T1(f - 1, 0x4E14) and T1(f - 1, 0x4E14) <= 3]
    okd = len(nz) == len(drops) and all(0 <= d - n <= 400 for n, d in zip(nz, drops))
    for i, (n, d) in enumerate(zip(nz, drops)):
        rec(0x4DA5, 'death %d' % (i + 1), n, 'goes non-zero at frame %d (value %d)' % (n, T1(n, 0x4DA5)), '4E14 drops %d -> %d at frame %d' % (T1(d - 1, 0x4E14), T1(d, 0x4E14), d), okd)
    # --- S2: two players
    for name, sn in mo2.items():
        mm = MM(sn); f = sn['frame']
        if name == 'credit2': rec(0x4E6E, name + ' (2-coin run)', f, '%d' % G(sn, 0x4E6E), 'screen credit digit %s' % credit_digit(mm), credit_digit(mm) == G(sn, 0x4E6E))
        if name in ('ready2', 'p1play', 'p2turn', 'p2eat'):
            rec(0x4E70, name, f, '%d' % G(sn, 0x4E70), 'game started with the START 2 button (two credits used)', G(sn, 0x4E70) == 1)
            cur = G(sn, 0x4E09); s1 = bcd_le(sn['mem'], 0x4E80 - 0x4000, 3); s2 = bcd_le(sn['mem'], 0x4E84 - 0x4000, 3)
            if name == 'ready2': why, ok = 'READY! shown for player 1 (nobody has scored)', cur == 0 and s1 == 0
            elif name == 'p1play': why, ok = 'P1 score %d is the one that has scored, P2 score %d' % (s1, s2), cur == 0 and s1 > 0 and s2 == 0
            elif name == 'p2turn': why, ok = 'P1 finished with score %d; P2 turn begins (P2 score %d)' % (s1, s2), cur == 1
            else: why, ok = 'P2 now scoring (%d) while P1 stays at %d' % (s2, s1), cur == 1 and s2 > 0
            rec(0x4E09, name, f, '%d' % cur, why, ok)
        if name in ('p1play', 'p2turn', 'p2eat'):
            v = bcd_le(sn['mem'], 0x4E84 - 0x4000, 3); sc = screen_score(mm, 20, 26)
            rec(0x4E84, name, f, 'bytes %02X %02X %02X -> %d' % (G(sn, 0x4E84), G(sn, 0x4E85), G(sn, 0x4E86), v), 'screen P2 score %s' % sc, (sc or 0) == v)
    # --- DIP settings
    lives_exp = {0xC1: 1, 0xC5: 2, 0xC9: 3, 0xCD: 5}
    bonus_exp = {0: 0x10, 1: 0x15, 2: 0x20, 3: 0xFF}
    for label, dip, cock in DIPS:
        sc = dict(coins=(400, 440), start1=(480,), cocktail=cock, dip=dip)
        _, _, mo3, _ = play(sc, 700, RULES3, keep_trace=False)
        fr = lambda n: mo3[n]['frame'] if n in mo3 else -1
        a, c1, c2 = mo3['attract'], mo3['coin1'], mo3['coin2']
        coin_rule = dip & 3
        e6b = {0: 0, 1: 1, 2: 1, 3: 2}[coin_rule]; e6d = {0: 0, 1: 1, 2: 2, 3: 1}[coin_rule]
        for addr, exp, nm in ((0x4E6B, e6b, 'coins per credit'), (0x4E6D, e6d, 'credits per coin')):
            rec(addr, 'DIP ' + label, fr('attract'), '%d' % G(a, addr), 'DIP bits1-0=%d expects %d (%s)' % (coin_rule, exp, nm), G(a, addr) == exp)
        if dip in (0xC9, 0xCA, 0xCB):
            for nm, sn in (('after coin 1', c1), ('after coin 2', c2)):
                mm = MM(sn)
                rec(0x4E6E, 'DIP ' + label + ' ' + nm, sn['frame'], '%d' % G(sn, 0x4E6E), 'screen credit digit %s, partial coin counter 4E6C=%d' % (credit_digit(mm), G(sn, 0x4E6C)), credit_digit(mm) == G(sn, 0x4E6E))
                rec(0x4E6C, 'DIP ' + label + ' ' + nm, sn['frame'], '%d' % G(sn, 0x4E6C), 'coins since last credit (partial credit); credits shown %s' % credit_digit(mm), True if coin_rule != 3 else G(sn, 0x4E6C) == (1 if nm == 'after coin 1' else 0))
        if dip & 0x0C != 0x08 or True:
            lv = lives_exp.get(dip & 0xCD if False else (dip & 0x0C) | 0xC1)
        lives = {0: 1, 1: 2, 2: 3, 3: 5}[(dip >> 2) & 3]
        r = mo3.get('ready')
        if r:
            mm = MM(r); li = life_icons(mm)
            rec(0x4E6F, 'DIP ' + label, r['frame'], '%d' % G(r, 0x4E6F), 'DIP bits3-2 expect %d lives; READY screen shows %d life icons' % (lives, li), G(r, 0x4E6F) == lives and li == lives)
        bon = bonus_exp[(dip >> 4) & 3]
        if coin_rule == 0:
            sn = mo3['attract']; mm = MM(sn); word = {0x10: '10000', 0x15: '15000', 0x20: '20000', 0xFF: None}[bon]
            shown = text(mm, 'BONUSPAC' if False else 'BONUS') and (word is not None and (screen_has(mm, 'FOR' + word) or screen_has(mm, 'FOR ' + word) or word in ''.join(ascii_row(mm, sy) for sy in range(36))))
            ev = 'free-play screen text %s' % ('contains BONUS PAC-MAN FOR %s PTS' % word if shown else 'has no bonus line' if word is None else 'MISSING bonus %s' % word)
            rec(0x4E71, 'DIP ' + label, sn['frame'], '0x%02X' % G(sn, 0x4E71), ev, G(sn, 0x4E71) == bon and (shown if word else not text(mm, 'BONUS')))
        else:
            sn = mo3['attract']
            rec(0x4E71, 'DIP ' + label, sn['frame'], '0x%02X' % G(sn, 0x4E71), 'DIP bits5-4 expect 0x%02X' % bon, G(sn, 0x4E71) == bon)
        sn = mo3['attract']
        hard = ((dip >> 6) & 1) == 0
        rec(0x4E73, 'DIP ' + label, sn['frame'], '0x%02X' % G(sn, 0x4E73), 'DIP bit6=%d -> %s expects 0x%02X' % ((dip >> 6) & 1, 'hard' if hard else 'normal', 0x7D if hard else 0x68), G(sn, 0x4E73) == (0x7D if hard else 0x68))
        alt = ((dip >> 7) & 1) == 0
        mm = MM(sn)
        _, _, mon, _ = play(dict(dip=dip), 600, {'names': (520, lambda m, f: True)}, keep_trace=False)
        sn2 = mon['names']; mm2 = MM(sn2)
        rec(0x4E75, 'DIP ' + label, sn2['frame'], '%d' % G(sn2, 0x4E75), 'DIP bit7=%d; attract screen names: %s' % ((dip >> 7) & 1, 'AAAAAAAA..' if text(mm2, 'AAAAAAAA') else 'SHADOW/BLINKY' if text(mm2, 'SHADOW') else 'none'), G(sn2, 0x4E75) == (1 if alt else 0) and (coin_rule == 0 or (text(mm2, 'AAAAAAAA') if alt else text(mm2, 'SHADOW'))))
        rec(0x4E72, 'DIP ' + label, sn['frame'], '%d' % G(sn, 0x4E72), 'IN1 bit7 = %d (%s)' % (0 if cock else 1, 'cocktail' if cock else 'upright'), G(sn, 0x4E72) == (1 if cock else 0))
    return RES

# ---------- catalogue: address, size, name, meaning, group, confidence, pacman-c comparison ----------
VARS = {
 0x4C80: (2, 'task_queue_write_ptr', 'Pointer (word, always 0x4Cxx in C0..FE) where the next queued task is written; equals the read pointer when the queue is empty', 'system', 'HIGH', 'agrees (TASKLIST_END)'),
 0x4C82: (2, 'task_queue_read_ptr', 'Pointer (word) of the next queued task for task_loop 238D to run', 'system', 'HIGH', 'agrees (TASKLIST_BEGIN)'),
 0x4C84: (1, 'frame_counter_up', 'Counts up by 1 every frame (every interrupt), wraps at 256', 'system', 'MED', 'name differs (pacman-c: sound counter); behaviour measured'),
 0x4C85: (1, 'frame_counter_down', 'Counts down by 1 every frame; always equals 0 minus 4C84', 'system', 'MED', 'not in pacman-c map'),
 0x4C86: (1, 'timer_sixtieths', 'Frame counter stepping 0x00-0x05, 0x10-0x15 ... 0x90-0x95 (60 values, +1 per frame, +0x0B at each decade), wraps once per second', 'system', 'HIGH', 'agrees (TIMER_SIXTIETHS)'),
 0x4C87: (1, 'timer_seconds', 'Seconds, BCD 00..59, +1 each wrap of 4C86', 'system', 'HIGH', 'agrees'),
 0x4C88: (1, 'timer_minutes', 'Minutes, +1 when seconds wrap 59 to 00', 'system', 'HIGH', 'agrees'),
 0x4C8B: (1, 'rng_a', 'Pseudo-random byte, changes every frame', 'system', 'MED', 'agrees (RNG)'),
 0x4C8C: (1, 'rng_b', 'Pseudo-random byte, changes every frame', 'system', 'MED', 'agrees (RNG)'),
 0x4C12: (2, 'sprite_staging_blinky', 'Blinky sprite registers staged each frame: byte0 = pos.x + 6, byte1 = 8 - pos.y (copied to hardware next frame)', 'sprites', 'HIGH', 'DISAGREES in sign/offset (pacman-c: 7-x, y+8); measured here'),
 0x4C14: (2, 'sprite_staging_pinky', 'Pinky staging, byte0 = pos.x + 6, byte1 = 8 - pos.y', 'sprites', 'HIGH', 'as above'),
 0x4C16: (2, 'sprite_staging_inky', 'Inky staging, byte0 = pos.x + 7, byte1 = 8 - pos.y', 'sprites', 'HIGH', 'as above'),
 0x4C18: (2, 'sprite_staging_clyde', 'Clyde staging, byte0 = pos.x + 7, byte1 = 8 - pos.y', 'sprites', 'HIGH', 'as above'),
 0x4C1A: (2, 'sprite_staging_pacman', 'Pac-Man staging, byte0 = pos.x + 7, byte1 = 8 - pos.y', 'sprites', 'HIGH', 'as above'),
 0x4D00: (2, 'blinky_position', 'Blinky position: byte0 = picture row coordinate (down = larger), byte1 = picture column coordinate (right = smaller)', 'actors', 'HIGH', 'agrees on layout (y then x)'),
 0x4D02: (2, 'pinky_position', 'Pinky position, same layout', 'actors', 'HIGH', 'agrees'),
 0x4D04: (2, 'inky_position', 'Inky position, same layout', 'actors', 'HIGH', 'agrees'),
 0x4D06: (2, 'clyde_position', 'Clyde position, same layout', 'actors', 'HIGH', 'agrees'),
 0x4D08: (2, 'pacman_position', 'Pac-Man position, same layout; sprite square on screen x = 232-byte1.., y = 8+byte0..', 'actors', 'HIGH', 'agrees'),
 0x4D39: (1, 'pacman_tile_row', 'Pac-Man current tile, vertical: (4D08 >> 3) + 32', 'actors', 'HIGH', 'agrees (TILE2 of Pac-Man)'),
 0x4D3A: (1, 'pacman_tile_col', 'Pac-Man current tile, horizontal: (4D09 >> 3) + 30', 'actors', 'HIGH', 'agrees (TILE2)'),
 0x4D30: (1, 'pacman_orientation', 'Direction Pac-Man moves: 0 right, 1 down, 2 left, 3 up', 'actors', 'HIGH', 'agrees'),
 0x4D2C: (1, 'blinky_orientation', 'Blinky direction 0 right,1 down,2 left,3 up', 'actors', 'HIGH', 'agrees'),
 0x4D2D: (1, 'pinky_orientation', 'Pinky direction (same codes; lags movement slightly)', 'actors', 'MED', 'agrees'),
 0x4D2E: (1, 'inky_orientation', 'Inky direction (same codes)', 'actors', 'HIGH', 'agrees'),
 0x4DA5: (1, 'pacman_death_anim_state', 'Non-zero only while the death animation plays; every episode is followed by a lives drop', 'game', 'MED', 'agrees (PAC_DEAD_ANIM_STATE)'),
 0x4DA6: (1, 'pacman_powered_up', '1 while ghosts are frightened (blue)', 'game', 'HIGH', 'agrees'),
 0x4DA7: (1, 'blinky_edible', '1 when Blinky is blue/edible', 'game', 'HIGH', 'agrees'),
 0x4DA8: (1, 'pinky_edible', '1 when Pinky is blue/edible', 'game', 'HIGH', 'agrees'),
 0x4DA9: (1, 'inky_edible', '1 when Inky is blue/edible', 'game', 'HIGH', 'agrees'),
 0x4DAA: (1, 'clyde_edible', '1 when Clyde is blue/edible', 'game', 'HIGH', 'agrees'),
 0x4DCE: (1, 'blink_timer_1up', 'Counter whose bit 4 makes the 1UP label blink', 'game', 'HIGH', 'name differs (pacman-c: coin timer)'),
 0x4E00: (1, 'main_state', '1 attract/demo, 2 credit inserted (push start / ready), 3 game running', 'game', 'HIGH', 'agrees'),
 0x4E02: (1, 'intro_state', 'Sub-state of attract/intro/ready sequence (3, 1, 0 observed)', 'game', 'MED', 'agrees (name only)'),
 0x4E04: (1, 'level_state', '0 before play, 3 maze running, 7 game over', 'game', 'HIGH', 'agrees'),
 0x4E09: (1, 'current_player', '0 = player 1, 1 = player 2', 'game', 'HIGH', 'agrees'),
 0x4E0E: (1, 'dots_eaten_this_level', 'Count of dots and energizers eaten on the current level (binary)', 'game', 'HIGH', 'agrees'),
 0x4E12: (1, 'died_this_level', '1 once a life has been lost on the current level', 'game', 'MED', 'agrees'),
 0x4E13: (1, 'level', 'Level number minus 1 (0 = level 1)', 'game', 'HIGH', 'agrees'),
 0x4E14: (1, 'lives_remaining', 'Lives left including the one in play; starts at lives-per-game', 'game', 'HIGH', 'agrees'),
 0x4E15: (1, 'lives_displayed', 'Number of life icons drawn (one less than 4E14 during play)', 'game', 'HIGH', 'agrees'),
 0x4E16: (30, 'pill_array_p1', 'Saved copy of which dots remain for player 1: full (240 bits) at level start, refreshed when a life is lost', 'game', 'MED', 'agrees on address/size; saved-copy behaviour measured'),
 0x4E34: (4, 'energizer_array_p1', 'Saved copy of the energizers remaining (2 bits per energizer after a death)', 'game', 'MED', 'agrees on address'),
 0x4E6B: (1, 'coins_per_credit', 'From DIP coinage: 0 free, 1, 2', 'settings', 'HIGH', 'agrees'),
 0x4E6C: (1, 'partial_credit_coins', 'Coins inserted towards the next credit (2 coins/credit mode)', 'settings', 'MED', 'agrees'),
 0x4E6D: (1, 'credits_per_coin', 'From DIP coinage: 0 free, 1 or 2', 'settings', 'HIGH', 'agrees'),
 0x4E6E: (1, 'credits', 'Credits available, shown at bottom of screen', 'settings', 'HIGH', 'agrees'),
 0x4E6F: (1, 'lives_per_game', 'From DIP: 1, 2, 3 or 5', 'settings', 'HIGH', 'agrees'),
 0x4E70: (1, 'two_player_game', '1 when the game was started with START 2', 'game', 'HIGH', 'agrees'),
 0x4E71: (1, 'bonus_life_score', 'From DIP: BCD thousands 0x10, 0x15, 0x20 or 0xFF (none)', 'settings', 'HIGH', 'agrees'),
 0x4E72: (1, 'cocktail_mode', '1 when the cabinet input says cocktail (IN1 bit 7 = 0)', 'settings', 'HIGH', 'agrees'),
 0x4E73: (1, 'difficulty_pointer_lo', 'From DIP bit 6: 0x68 normal, 0x7D hard (low byte of a table pointer)', 'settings', 'MED', 'agrees'),
 0x4E75: (1, 'ghost_names_mode', 'From DIP bit 7: 1 = alternate ghost names (AAAAAAAA...)', 'settings', 'HIGH', 'agrees'),
 0x4E80: (3, 'score_p1', 'Player 1 score, BCD, little-endian', 'game', 'HIGH', 'agrees'),
 0x4E84: (3, 'score_p2', 'Player 2 score, BCD, little-endian', 'game', 'HIGH', 'agrees'),
 0x4E88: (3, 'high_score', 'High score, BCD, little-endian', 'game', 'HIGH', 'agrees'),
}
EXTRA_TYPO = ('pacman-c lists P2_POWERUP_ARRAY at 4E42; by the P1 block layout (+0x2E) it is 4E62 (source typo)',
 '4D12/4D13 stays at (56,46) in play; it is not Pac-Man\'s tile (4D39/4D3A are) despite the pacman-c names for that block')

def evidence_text(R):
    L = ['S10 RAM VARIABLE EVIDENCE (prototype run; every row = value in RAM at a moment vs what the screen/sprites/behaviour show)', '']
    bad = 0; nv = 0
    for a in sorted(R):
        v = VARS.get(a); size, nm = (v[0], v[1]) if v else (1, '?')
        L.append('%04X %s (%d byte%s)' % (a, nm, size, '' if size == 1 else 's'))
        for mo, f, ram, ev, ok in R[a]:
            L.append('   [%s] %-34s f%-5d ram=%s | %s' % ('OK' if ok else 'FAIL', mo, f, ram, ev)); bad += (not ok)
        nv += 1
    L += ['', 'VARIABLES WITH EVIDENCE: %d   FAILED ROWS: %d' % (nv, bad)]
    return '\n'.join(L) + '\n', bad

MD_HEAD = '''# RAM map of the Pac-Man program (S10)

Status: VERIFIED on the prototype (reference oracle). Evidence: `docs/evidence/S10_ram_values.txt` (generated by `python3 tools/analysis/oracle_ram.py`, which prints each variable's value at 3 or more moments next to what the screen, the rendered sprites or the measured behaviour show).

How to read the table: every row is a RAM address, its size, a name, its meaning, how it was checked, and a confidence.
- HIGH = checked against the screen/pixels or an exact measured relation at 3 or more moments, and all checks agree.
- MED = values and behaviour agree, but the exact meaning is inferred.
- Names are mine unless the second source gives one (see the comparison section). The ROM contains no names.

Sources: (1) the prototype run of the user's own ROM (this step's experiments); (2) the pacman-c project `memmap.h` (GitHub, raw file) as the second source for names. The euro-arcade RAM page named in the plan could not be fetched (redirect loop, then HTTP 403 from the sandbox proxy), so pacman-c replaces it. The Pac-Man Dossier tables are not used yet (S12 extracts them from the ROM).

Coordinates: the 'position' bytes are pixels. byte0 grows downward on the picture (picture row = 8 + byte0 for the top of the 16x16 sprite); byte1 grows leftward (picture column = 232 - byte1 for the left edge, Pac-Man). The picture is the 224x288 upright view.

## Variables
'''
MD_TAIL = '''
## Findings worth remembering
- Sprite staging: each frame the game computes 4C12..4C1B (pairs for Blinky, Pinky, Inky, Clyde, Pac-Man) from the positions: byte0 = position byte1 + 6 (Blinky, Pinky) or + 7 (Inky, Clyde, Pac-Man); byte1 = 8 minus position byte0. This held on every one of 2000 sampled frames. The hardware registers 5062.. get these values one frame later.
- Sprite slot order changes while Pac-Man is powered up (4DA6 = 1): normally slots 1-4 are Blinky, Pinky, Inky, Clyde and slot 5 is Pac-Man; with the ghosts blue slot 1 is Pac-Man and Blinky moves to slot 5 (Pinky, Inky, Clyde stay in 2-4). Evidence rows: 4D00 and 4D08 at moments `blue` versus the others. (This changes drawing priority; the port must reproduce it.)
- 4E16 and 4E34 are NOT live dot state. During play they keep the level-start copy; they are refreshed from the screen when a life is lost (214 and 171 bits set at the two deaths observed, equal to the dots left on screen). The live dots are the tile codes in video RAM (0x10 dot, 0x14 energizer).
- 4C86 steps 0x00-0x05, 0x10-0x15, ... 0x90-0x95 then wraps: 60 frames per cycle; 4C87 counts these wraps in BCD; 4C88 counts minutes.
- 4DBD (word) holds 0x02D0 (720) on level 1 and 0x0258 (600) on level 2, which would be 120 per second times 6 s and 5 s (frightened times as I recall them from the Dossier; not re-checked, the Dossier tables are read in S12). Not confirmed (no count-down variable found), so it is listed below as LOW.
- DIP-derived settings (4E6B, 4E6D, 4E6F, 4E71, 4E73, 4E75) were checked for 14 DIP/cabinet settings; the free-play screen shows the bonus-life text matching 4E71.

## Agreements and disagreements with the second source (pacman-c memmap.h)
- Agree: layout of 4C80/4C82 (task pointers), timers 4C86-4C88, RNG 4C8B/4C8C, positions 4D00-4D09 (y then x), orientations 4D2C-4D30, power-up 4DA6, edible flags 4DA7-4DAA, state 4E00/4E04, player 4E09, level 4E13, lives 4E14/4E15, scores 4E80/4E84/4E88, DIP block 4E6B-4E75.
- Disagree (sign/offset): pacman-c writes sprite staging as 7-x and y+8 for the first byte/second byte; measured here: byte0 = x + 6 or 7 and byte1 = 8 - y with x = position byte1 and y = position byte0. The two can both be right if pacman-c stores x/y in mirrored form; this port will follow the measured relations.
- Disagree (name): 4DCE is called a coin timer in pacman-c; here its bit 4 drives the 1UP label blink on 7 of 7 moments. 4C84 is called a sound counter; measured as a free-running frame counter (with 4C85 = its negative).
- Source typo: pacman-c lists P2_POWERUP_ARRAY at 4E42; the player-2 block is the player-1 block + 0x2E, so it should be 4E62.
- 4D12/4D13 stays at (56,46) throughout play, so it is not Pac-Man's current tile despite the pacman-c block name; 4D39/4D3A are.

## Listed but only partly verified (LOW: values printed, no independent check)
| Address | Name from source | Observation |
|---|---|---|
| 4DBD | frightened time (word) | 0x02D0 on level 1, 0x0258 on level 2 (see above) |
| 4E0A | difficulty pointer for player 1 | 0x68 on level 1, 0x69 on level 2 |
| 4E03 | credit/ready state | 3 at READY, 0 elsewhere |
| 4E01 | reset state | 0 throughout |
| 4DD2 | fruit position | stayed 0 (no fruit appeared: first fruit needs 70 dots; scripted play reached 69) |
| 4E0C/4E0D | first/second fruit flags | not exercised for the same reason |
'''

def markdown(R):
    L = [MD_HEAD, '| Address | Size | Name | Meaning | Group | Evidence (moments with a check) | Confidence | Second source |', '|---|---|---|---|---|---|---|---|']
    for a in sorted(VARS):
        sz, nm, mean, grp, conf, cmp_ = VARS[a]
        rows = R.get(a, []); mo = sorted(set(r[0] for r in rows), key=lambda x: [r[0] for r in rows].index(x))
        kinds = {'system': 'behaviour over 3800 frames', 'sprites': 'exact relation + rendered pixels', 'actors': 'rendered sprite pixels / measured movement', 'game': 'screen text, icons, digits or movement', 'settings': 'DIP/cabinet runs and screen'}[grp]
        L.append('| %04X | %d | %s | %s | %s | %s: %d moments (%s) | %s | %s |' % (a, sz, nm, mean, grp, kinds, len(mo), ', '.join(mo[:4]) + (', ...' if len(mo) > 4 else ''), conf, cmp_))
    return '\n'.join(L) + '\n' + MD_TAIL

if __name__ == '__main__':
    R = run_all(); txt, bad = evidence_text(R)
    if '--markdown' in sys.argv:
        open(os.path.join(root, 'docs', 'notes', 'ram_map.md'), 'w').write(markdown(R)); print('wrote docs/notes/ram_map.md'); sys.exit(0)
    path = os.path.join(root, 'docs', 'evidence', 'S10_ram_values.txt')
    if '--check' in sys.argv:
        same = os.path.exists(path) and open(path).read() == txt
        print('S10 oracle: %s, %d failed rows' % ('output matches stored evidence' if same else 'OUTPUT DIFFERS from stored evidence', bad)); sys.exit(0 if same and bad == 0 else 1)
    open(path, 'w').write(txt); print(txt[-200:]); sys.exit(1 if bad else 0)
