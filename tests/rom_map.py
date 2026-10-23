#!/usr/bin/env python3
"""N01: builds the routine map of the original program: docs/notes/rom_routines.md (addresses and counts only, never ROM bytes).
Inputs: docs/disasm/blocks.txt and docs/disasm/pacman.asm (call/jump targets, jump tables, inline data), the coverage files of tests/rom_map.cpp.
Usage: python3 tests/rom_map.py COVERAGE_REPLAY COVERAGE_PLAYER OUT.md      (exit 1 if a consistency check fails)
Definitions: an entry point = the reset/interrupt/RST vectors, every call target, every entry of a jump table (RST 20h). A routine = the instructions
from one entry point up to the next entry point (in address order) inside the code blocks. Callers = routines containing a call to it (or the jump table)."""
import re, sys, collections

def addr_of(name):
    m = re.search(r'(?:\bl|\bsub_)([0-9a-f]{4})h\b', name) or re.search(r'\b0([0-9a-f]{4})h\b', name)
    return int(m.group(1), 16) if m else None

def load_blocks(path):
    out = []
    for ln in open(path):
        m = re.match(r'(\w+?)(\d+): start 0x([0-9a-f]+) end 0x([0-9a-f]+) type (\w+)', ln)
        if m: out.append((m.group(1), int(m.group(3), 16), int(m.group(4), 16), m.group(5)))
    return out

def load_cov(path):
    s = set()
    for ln in open(path):
        if ln.startswith('#') or not ln.strip(): continue
        a, b = ln.split(); s.update(range(int(a, 16), int(b, 16) + 1))
    return s

def main():
    # optional (N02): --cover NAME=FILE (repeatable) and --report FILE: coverage by routine of those sets, and the routines not reached by them together
    args = sys.argv[1:]; covers = []; report = None
    while '--cover' in args:
        i = args.index('--cover'); n, f = args[i + 1].split('=', 1); covers.append((n, f)); del args[i:i + 2]
    if '--report' in args:
        i = args.index('--report'); report = args[i + 1]; del args[i:i + 2]
    sys.argv = [sys.argv[0]] + args
    cov_r, cov_p, out = load_cov(sys.argv[1]), load_cov(sys.argv[2]), sys.argv[3]
    raw_r, raw_p, raw_all = len(cov_r), len(cov_p), len(cov_r | cov_p)
    cov = cov_r | cov_p
    # The Z80 core executes a prefix byte (DD, FD, ED, CB) as its own step, so the machine's PC is seen once more INSIDE the instruction (1 to 3 bytes after
    # its start). Such addresses are folded into the instruction that contains them (checked below: never more than 3 bytes after a start).
    blocks = load_blocks('docs/disasm/blocks.txt')
    kind = {}                       # block name -> kind
    def block_of(a):
        for n, s, e, t in blocks:
            if s <= a < e: return n, s, e, t
    instrs = []                     # (addr, text, block)
    tables = collections.defaultdict(list)   # jump-table block -> entries
    cur = None
    for ln in open('docs/disasm/pacman.asm'):
        mb = re.match(r"; BLOCK '(\w+?)(\d+)' \(start 0x([0-9a-f]+) end 0x([0-9a-f]+)\)", ln)
        if mb: cur = (mb.group(1), int(mb.group(3), 16), int(mb.group(4), 16)); continue
        mi = re.match(r'\t([a-z]+[^;]*?)\s*;([0-9a-f]{4})\s*$', ln)
        if not mi or not cur: continue
        text, a = mi.group(1).strip(), int(mi.group(2), 16)
        if cur[0] == 'code': instrs.append((a, text, cur))
        elif cur[0] == 'jumptable' and text.startswith('defw'):
            t = addr_of(text); tables[cur[1]].append((a, t))
    addrs = [a for a, _, _ in instrs]
    iset = set(addrs)
    assert len(addrs) == len(iset), 'duplicate instruction address'
    import bisect
    def fold(c):
        out, bad = set(), []
        for a in c:
            if a in iset: out.add(a); continue
            i = bisect.bisect_right(addrs, a) - 1
            if i >= 0 and a - addrs[i] <= 3 and (i + 1 >= len(addrs) or a < addrs[i + 1]): out.add(addrs[i])
            else: bad.append(a)
        return out, bad
    cov_r, bad_r = fold(cov_r); cov_p, bad_p = fold(cov_p)
    folded = len(load_cov(sys.argv[1]) | load_cov(sys.argv[2])) - len(cov_r | cov_p) if False else 0
    cov = cov_r | cov_p
    stray = sorted(bad_r + bad_p)
    # entry points
    entries = {0x0000: 'reset', 0x0066: 'nmi', 0x008D: 'interrupt handler (vblank)'}
    for v in (0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38): entries.setdefault(v, 'rst vector')
    callers = collections.defaultdict(set); jumpsto = collections.defaultdict(set)
    for a, t, _ in instrs:
        m = re.match(r'call\s+(?:[a-z]+,)?(\S+)', t)
        if m and addr_of(m.group(1)) is not None:
            tg = addr_of(m.group(1)); entries.setdefault(tg, 'call target'); callers[tg].add(('call', a))
        m = re.match(r'rst\s+([0-9a-f]+)h', t)
        if m: callers[int(m.group(1), 16)].add(('rst', a))
    for ts, lst in tables.items():
        for slot, tg in lst:
            if tg is not None: entries.setdefault(tg, 'jump table entry'); callers[tg].add(('table', ts))
    ent = sorted(e for e in entries if e in iset)
    missing = sorted(e for e in entries if e not in iset)
    def routine_of(a):
        lo = 0
        import bisect
        i = bisect.bisect_right(ent, a) - 1
        return ent[i] if i >= 0 else None
    # check: instructions of each code block are all assigned to exactly one routine; every block starts at its first instruction
    rcount = collections.Counter(routine_of(a) for a in addrs)
    assert None not in rcount, 'instruction before the first entry'
    assert sum(rcount.values()) == len(addrs)
    per_block = collections.defaultdict(list)
    for a, _, b in instrs: per_block[b[1]].append(a)
    problems = []
    for n, s, e, t in blocks:
        if t != 'code': continue
        l = per_block.get(s)
        if not l: problems.append('code block at %04X has no instructions' % s); continue
        if l[0] != s or l[-1] >= e or e - l[-1] > 4 or l != sorted(l): problems.append('code block %04X-%04X not consistent' % (s, e))
    nblk = sum(1 for b in blocks if b[3] == 'code')
    # data blocks and which routines read them
    data_blocks = [(n, s, e, t) for n, s, e, t in blocks if t != 'code']
    reads = collections.defaultdict(set)
    for a, t, _ in instrs:
        if re.match(r'ld\s+(hl|de|bc|ix|iy|sp),', t) or re.match(r'ld\s+\(.*\),(hl|de|bc|ix|iy|sp|a)$', t) or re.match(r'ld\s+a,\(0', t) or re.match(r'ld\s+(hl|de|bc),\(0', t):
            for tok in re.findall(r'(?:\bl|\bsub_)[0-9a-f]{4}h\b|\b0[0-9a-f]{4}h\b', t):
                v = addr_of(tok)
                if v is not None and v < 0x4000:
                    d = block_of(v)
                    if d and d[3] != 'code': reads[routine_of(a)].add(d[1])
    # coverage per routine
    ends = ent[1:] + [0x10000]
    rows = []
    for i, e in enumerate(ent):
        mine = [a for a in addrs if e <= a < ends[i]]
        ex = sum(1 for a in mine if a in cov)
        cl = sorted(callers.get(e, ()))
        crs = sorted({routine_of(a) if k == 'call' or k == 'rst' else None for k, a in cl if k in ('call', 'rst')} - {None})
        tb = sorted({a for k, a in cl if k == 'table'})
        rows.append((e, len(mine), ex, len(cl), crs, tb, sorted(reads.get(e, ()))))
    tot_i, tot_e = len(addrs), len(cov & iset)
    L = []
    L.append('# ROM routine map (generated by tests/rom_map.py, step N01; do not edit by hand)')
    L.append('')
    L.append('Addresses and counts only; no ROM bytes. Sources: docs/disasm/blocks.txt and docs/disasm/pacman.asm (both stay private), and the coverage of tests/rom_map.cpp.')
    L.append('Definitions: an *entry point* is the reset, NMI, interrupt handler, an RST vector, a call target or an entry of a jump table (RST 20h). A *routine* is the run of instructions from one entry point up to the next one. *Calls* = number of places that call it (CALL, RST, jump-table slot). *Called from* = routines holding such a call. *Tables read* = data blocks (start address) whose address the routine loads.')
    L.append('')
    L.append('## Totals')
    L.append('- blocks in blocks.txt: %d (code %d, other %d)' % (len(blocks), nblk, len(blocks) - nblk))
    L.append('- code blocks covered exactly once: %d of %d (every instruction belongs to exactly one routine)' % (nblk - len(problems), nblk))
    L.append('- instructions in the code blocks: %d' % tot_i)
    L.append('- entry points / routines: %d' % len(ent))
    L.append('- executed instruction addresses: S27 replay (7,273 frames) %d, T01a player (60,000 frames) %d, together %d of %d (%.1f percent)' % (len(cov_r), len(cov_p), tot_e, tot_i, 100.0 * tot_e / tot_i))
    L.append('- the same as raw machine addresses (prefix steps counted separately): replay %d, player %d, together %d (the planning measurement for the player was 4,947)' % (raw_r, raw_p, raw_all))
    L.append('- routines never entered in these runs: %d; routines entered but only partly run: %d' % (sum(1 for r in rows if r[2] == 0), sum(1 for r in rows if 0 < r[2] < r[1])))
    L.append('- executed addresses that are not inside any instruction of the map: %d (prefix bytes DD, FD, ED, CB that the core runs as their own step are counted with their instruction)' % len(stray))
    L.append('- entry points not on an instruction start (ignored): %d (%s: the NMI vector, which lies in padding data; the board has no NMI)' % (len(missing), ', '.join('%04X' % x for x in missing)))
    L.append('')
    L.append('## Routines')
    L.append('| Entry | Kind | Instructions | Run | Calls | Called from | Tables read |')
    L.append('|---|---|---|---|---|---|---|')
    for e, n, ex, nc, crs, tb, rd in rows:
        cf = ', '.join('%04X' % x for x in crs[:6]) + (' +%d' % (len(crs) - 6) if len(crs) > 6 else '')
        if tb: cf = (cf + ', ' if cf else '') + 'table ' + ', '.join('%04X' % x for x in tb)
        L.append('| %04X | %s | %d | %d | %d | %s | %s |' % (e, entries[e], n, ex, nc, cf or '-', ', '.join('%04X' % x for x in rd[:6]) or '-'))
    L.append('')
    L.append('## Data blocks (not code)')
    L.append('| Start | End | Kind |')
    L.append('|---|---|---|')
    for n, s, e, t in data_blocks: L.append('| %04X | %04X | %s |' % (s, e, t))
    open(out, 'w').write('\n'.join(L) + '\n')
    if report:
        sets = [(n, fold(load_cov(f))[0]) for n, f in covers]
        union = set().union(*[c for _, c in sets]) if sets else set()
        R = ['Coverage by routine (tests/rom_map.py --report). Instruction counts; a routine is "reached" when at least one of its instructions ran.']
        for n, c in sets:
            R.append('set %s: %d of %d instructions, %d of %d routines reached' % (n, len(c & iset), tot_i, sum(1 for e, k, *_ in rows if any(a in c for a in addrs if e <= a < (ent[ent.index(e) + 1] if ent.index(e) + 1 < len(ent) else 0x10000))), len(rows)))
        R.append('together: %d of %d instructions (%.1f percent)' % (len(union & iset), tot_i, 100.0 * len(union & iset) / tot_i))
        base = cov_r | cov_p
        R.append('')
        R.append('Routines: entry | instructions | run by the sets together | run by the N01 runs (replay+player)')
        notreached = []
        for i, e in enumerate(ent):
            mine = [a for a in addrs if e <= a < ends[i]]
            u = sum(1 for a in mine if a in union); b = sum(1 for a in mine if a in base)
            R.append('%04X | %d | %d | %d' % (e, len(mine), u, b))
            if u < len(mine): notreached.append((e, len(mine), u))
        R.append('')
        R.append('Routines not fully reached by the sets together: %d (never entered: %d)' % (len(notreached), sum(1 for x in notreached if x[2] == 0)))
        for e, n, u in notreached: R.append('  %04X: %d of %d instructions run' % (e, u, n))
        new = len((union & iset) - base)
        R.append('')
        R.append('Instructions run by the sets that the N01 runs (replay+player) did not run: %d; the other way round: %d' % (new, len((base & iset) - union)))
        open(report, 'w').write('\n'.join(R) + '\n')
        print('\n'.join(R[:len(sets) + 2])); print(R[-1])
    print('entry points not on an instruction start:', ['%04X' % x for x in missing])
    print('blocks %d (code %d); instructions %d; routines %d' % (len(blocks), nblk, tot_i, len(ent)))
    print('coverage: replay %d, player %d, together %d of %d instruction addresses (%.1f%%); stray %d' % (len(cov_r), len(cov_p), tot_e, tot_i, 100.0 * tot_e / tot_i, len(stray)))
    print('routines never entered %d, partly run %d' % (sum(1 for r in rows if r[2] == 0), sum(1 for r in rows if 0 < r[2] < r[1])))
    bad = problems + (['%d executed addresses are outside the instructions: %s' % (len(stray), ['%04X' % x for x in stray[:5]])] if stray else [])
    for p in bad: print('FAIL:', p)
    return 1 if bad else 0

sys.exit(main())
