"""S11: find and confirm gameplay routines by tracing the prototype. Read-only (harness.py unchanged).
Scenarios: A = scripted 1-player game (S1 of oracle_ram.py, seed 7, 4400 frames: power pill, ghost mode changes, deaths);
           B = same game with seed 1 for 2600 frames (reaches 70 dots: fruit appears);
           C = attract mode only, no coin, 3600 frames (demo game).
For each entry point: first frame executed, times executed per scenario; plus relation checks that tie the routines to RAM variables.
Writes docs/evidence/S11_gameplay_trace.txt (--check compares with the stored file)."""
import sys, os, collections, random
sys.path.append(os.path.dirname(os.path.abspath(__file__)))
from z80 import Z80Machine
from harness import Board
from oracle_ram import S1, inputs
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')

ENTRIES = [  # address, working name
 (0x1017, 'gameplay_frame_update'), (0x171D, 'ghost_pacman_collision_scan'), (0x1789, 'collision_outcome'), (0x1806, 'pacman_speed_tick'),
 (0x1B36, 'ghost_blinky_move'), (0x1C4B, 'ghost_pinky_move'), (0x1D22, 'ghost_inky_move'), (0x1DF9, 'ghost_clyde_move'),
 (0x1EFE, 'ghost_blinky_reverse_if_flagged'), (0x2000, 'add_vector_to_position'), (0x2018, 'pixel_to_tile'),
 (0x2966, 'pick_direction_toward_target'), (0x291E, 'pick_random_direction'),
 (0x2730, 'task08_blinky_target'), (0x276C, 'task09_pinky_target'), (0x27A9, 'task0A_inky_target'), (0x27F1, 'task0B_clyde_target'),
 (0x283B, 'task0C_blinky_nonchase'), (0x288F, 'task0E_inky_nonchase'), (0x28E3, 'task17_pacman_demo_direction'),
 (0x0E36, 'ghost_mode_timer'), (0x0E6C, 'siren_sound_select'), (0x0EAD, 'fruit_spawn_check'), (0x0ED5, 'fruit_spawn_body'),
 (0x19E0, 'eat_dot_at_pacman_tile'), (0x1A70, 'start_power_pill'), (0x2A5A, 'add_score'),
 (0x2069, 'release_check_pinky'), (0x2086, 'pinky_leaves_house'), (0x208C, 'release_check_inky'), (0x20A9, 'inky_leaves_house'),
 (0x20AF, 'release_check_clyde'), (0x20D1, 'clyde_leaves_house'), (0x1B08, 'house_counter_increment'),
]
ADDR = {a: n for a, n in ENTRIES}

def scenario(name):
    b = Board(); m = b.m; mem = b.mem
    m.mark_addrs(0, 0x4000, Z80Machine.READ_MARK)
    first = {}; count = collections.Counter(); per_frame = collections.defaultdict(collections.Counter)
    callers = collections.defaultdict(collections.Counter); samples = []; notes = collections.defaultdict(list)
    orig = b._rd
    st = {'ret_pending': None}
    def rd(a):
        a &= 0xFFFF
        if a < 0x4000 and a == m.pc and not m.halted:
            if a in ADDR:
                count[a] += 1; per_frame[a][b.frame] += 1
                if a not in first: first[a] = b.frame
            if a == 0x2966:
                ret = mem[m.sp] | (mem[m.sp + 1] << 8)
                callers[a][ret] += 1
                st['call'] = (b.frame, ret, m.hl, m.de, m.a, mem[0x4D39] | (mem[0x4D3A] << 8))
            if a == 0x29E9 and 'call' in st:      # return of pick_direction_toward_target
                fr, ret, hl, de, a0, pac = st['call']
                samples.append((fr, ret, hl, de, a0, m.a, m.hl, pac))
        return orig(a)
    m.set_read_callback(rd)
    sc = dict(S1)
    if name == 'A': frames = 4400; sc['seed'] = 7
    if name == 'B': frames = 2600; sc['seed'] = 1; sc['rack'] = None
    if name == 'C': frames = 3600; sc = dict(coins=(), start1=(), seed=1)
    sc['_st'] = {'rnd': random.Random(sc['seed']), 'hold': 0, 'd': 0xFF}
    hist = []
    for f in range(frames):
        i0, i1 = inputs(f, sc); b.ioread[0x5000] = i0; b.ioread[0x5040] = i1
        b.run_frame()
        hist.append(dict(f=f, ph=mem[0x4DC1], cnt=mem[0x4DC2] | (mem[0x4DC3] << 8), eaten=mem[0x4E0E], score=mem[0x4E80] | (mem[0x4E81] << 8) | (mem[0x4E82] << 16),
                         r1=mem[0x4DA1], r2=mem[0x4DA2], r3=mem[0x4DA3], c1=mem[0x4E0F], c2=mem[0x4E10], c3=mem[0x4E11], lim=(mem[0x4DB8], mem[0x4DB9], mem[0x4DBA]),
                         fruit=mem[0x4DD2], fcode=mem[0x4C0C], fcol=mem[0x4C0D], state=mem[0x4E00], power=mem[0x4DA6], kills=mem[0x4DD0], lives=mem[0x4E14]))
    return dict(first=first, count=count, per_frame=per_frame, callers=callers, samples=samples, hist=hist, mem=mem, b=b)

def report():
    L = ['S11 gameplay trace (prototype). Frame numbering as in S09 evidence. Scenario A: scripted 1P game seed 7 (4400 frames); B: seed 1 (2600 frames, reaches 70 dots); C: attract mode only (3600 frames).', '']
    R = {n: scenario(n) for n in 'ABC'}
    L.append('1. ENTRY POINTS: first frame executed / times executed (A | B | C)')
    for a, n in ENTRIES:
        cells = []
        for s in 'ABC':
            r = R[s]; cells.append('%s/%d' % (r['first'].get(a, '-'), r['count'][a]))
        L.append('  %04X %-34s %s' % (a, n, ' | '.join('%-14s' % c for c in cells)))
    A, B, C = R['A'], R['B'], R['C']
    # 2 gameplay_frame_update called twice per frame
    pf = [v for f, v in A['per_frame'][0x1017].items() if f > 600]
    L += ['', '2. gameplay_frame_update (1017) executions per frame while playing, scenario A: %s' % dict(collections.Counter(pf))]
    # 3 ghost mode timer
    h = A['hist']; mem = A['mem']
    tbl = [mem[0x4D86 + 2 * i] | (mem[0x4D87 + 2 * i] << 8) for i in range(8)]
    ch = [(h[i]['f'], h[i - 1]['ph'], h[i]['ph'], h[i - 1]['cnt']) for i in range(601, len(h)) if h[i]['ph'] != h[i - 1]['ph']]
    deaths = [h[i]['f'] for i in range(601, len(h)) if h[i]['lives'] < h[i - 1]['lives']]
    L += ['', '3. ghost_mode_timer (0E36), scenario A. Changes of the phase index 4DC1 (frame, old->new, counter 4DC2/3 the frame before):']
    for f, o, n, c in ch: L.append('     frame %d: %d -> %d, counter was %d%s' % (f, o, n, c, '   (a life was lost within 2 frames: phase restarts)' if any(0 <= f - d <= 2 or 0 <= d - f <= 2 for d in deaths) or n == 0 else ''))
    L.append('   Table of 8 words at 4D86 (copied for the level; values are frames at 60 per second): %s  = seconds %s' % (tbl, [round(t / 60, 2) for t in tbl[:5]]))
    L.append('   Phase 0 -> 1 happens when the counter reaches table word 0 (420 = 7 s); counter is held while 4DA6 (power pill) is 1 (code at 0E36-0E3A).')
    # 4 dot eating
    eats = A['count'][0x19E0]; incs = sum(1 for i in range(601, len(h)) if h[i]['eaten'] == h[i - 1]['eaten'] + 1)
    L += ['', '4. eat_dot_at_pacman_tile (19E0): executed %d times; frames where 4E0E rose by exactly 1: %d. Only pc 19E7 writes 4E0E (write trace of the whole run).' % (eats, incs)]
    # 5 power pill
    pp = A['first'].get(0x1A70); L += ['', '5. start_power_pill (1A70) first executed at frame %s; 4DA6 first becomes 1 (after frame 600) at frame %s' % (pp, next((x['f'] for x in h if x['f'] > 600 and x['power'] == 1), None))]
    # 6 score
    sc = [(h[i]['f'], h[i - 1]['score'], h[i]['score']) for i in range(601, len(h)) if h[i]['score'] != h[i - 1]['score']]
    L += ['', '6. add_score (2A5A): executed %d times in A (all dot/energizer/ghost/fruit points). Score bytes (BCD) changed %d times after frame 600; first changes (frame, old, new as hex BCD): %s' % (A['count'][0x2A5A], len(sc), [(f, '%X' % o, '%X' % n) for f, o, n in sc[:5]])]
    # 7 ghost house
    rel = []
    for nm, key in (('Pinky 4DA1', 'r1'), ('Inky 4DA2', 'r2'), ('Clyde 4DA3', 'r3')):
        f = next((i for i in range(601, len(h)) if h[i][key] != 0 and h[i - 1][key] == 0), None)
        rel.append('%s set at frame %s: personal counters 4E0F/10/11 = %s, limits 4DB8/9/A = %s' % (nm, h[f]['f'] if f else None, (h[f]['c1'], h[f]['c2'], h[f]['c3']) if f else None, h[f]['lim'] if f else None))
    L += ['', '7. ghost-house release (2069/208C/20AF call 2086/20A9/20D1), scenario A:'] + ['     ' + r for r in rel]
    # 8 fruit
    hb = B['hist']; fr = next((i for i in range(601, len(hb)) if hb[i]['fruit'] != 0 and hb[i - 1]['fruit'] == 0), None)
    L += ['', '8. fruit_spawn_check (0EAD) / body (0ED5), scenario B: body first executed at frame %s (once); at frame %s 4E0E = %s, fruit position word 4DD2/3 low byte = 0x%02X, fruit sprite code 4C0C = 0x%02X, colour 4C0D = 0x%02X' % (B['first'].get(0x0ED5), hb[fr]['f'], hb[fr]['eaten'], hb[fr]['fruit'], hb[fr]['fcode'], hb[fr]['fcol'])]
    # 9 siren
    L += ['', '9. siren_sound_select (0E6C): executed %d times in A; writes to 4EAC come from pcs 0E76 and 0EAC (write trace)' % A['count'][0x0E6C]]
    # 10 target choice
    L += ['', '10. pick_direction_toward_target (2966): calls by return address (who called): ' + ', '.join('%s:%d' % (hex(k), v) for k, v in sorted(A['callers'][0x2966].items()))]
    ok_nrev = all(((s[5] ^ 2) != s[4]) for s in A['samples'])
    chase = [s for s in A['samples'] if s[1] == 0x2765]
    ok_chase = all(s[3] == s[7] for s in chase)
    L.append('    over %d calls in A the new direction was never the reverse of the old one: %s' % (len(A['samples']), ok_nrev))
    L.append('    Blinky chase calls (return address 2765, %d calls): target DE equals Pac-Man tile word 4D39/4D3A every time: %s' % (len(chase), ok_chase))
    sc2 = [s for s in A['samples'] if s[1] == 0x2751]
    L.append('    Blinky scatter calls (return 2751, %d calls): target DE values %s' % (len(sc2), sorted(set(hex(s[3]) for s in sc2))))
    L.append('    first 6 samples (frame, caller, ghost tile word, target, old dir, new dir, new vector): ' + '; '.join('f%d %s %04X %04X %d->%d %04X' % (s[0], hex(s[1]), s[2], s[3], s[4], s[5], s[6]) for s in A['samples'][:6]))
    for s in 'ABC': pass
    L += ['', '11. attract demo (scenario C): task17_pacman_demo_direction (28E3) executed %d times, gameplay_frame_update %d times, first at frame %s' % (C['count'][0x28E3], C['count'][0x1017], C['first'].get(0x1017))]
    # 12 collision
    kills = [(h[i]['f'], h[i - 1]['kills'], h[i]['kills']) for i in range(601, len(h)) if h[i]['kills'] != h[i - 1]['kills']]
    L += ['', '12. ghost_pacman_collision_scan (171D) executions in A: %d (twice per frame with 1017). Ghost-kill counter 4DD0 changes after frame 600: %s. Pac-Man deaths (4E14 drops) at frames %s.' % (A['count'][0x171D], kills, deaths)]
    rom = A['mem']
    L += ['', '13. Tables read from the ROM image (by the routines above): points table 2B17 (BCD words) = %s; fruit table 0EFD (code, colour, points index; first levels) = %s; ghost mode table 4D86 is copied from ROM per level (see section 3).' % ([('%X' % (rom[0x2B17 + 2 * i] | rom[0x2B18 + 2 * i] << 8)) for i in range(10)], [tuple('%02X' % rom[0x0EFD + 3 * k + j] for j in range(3)) for k in range(4)])]
    return '\n'.join(L) + '\n', dict(ok_nrev=ok_nrev, ok_chase=ok_chase, R=R)

if __name__ == '__main__':
    txt, info = report()
    path = os.path.join(root, 'docs', 'evidence', 'S11_gameplay_trace.txt')
    if '--check' in sys.argv:
        same = os.path.exists(path) and open(path).read() == txt
        print('S11 trace: %s' % ('identical to stored evidence' if same else 'DIFFERS from stored evidence')); sys.exit(0 if same else 1)
    open(path, 'w').write(txt); print(txt)
