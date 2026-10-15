#!/usr/bin/env python3
"""Step S08: classify every byte of the 16 KB program ROM as code or data by tracing control flow, then emit
docs/disasm/pacman.asm (via z80dasm with a block file), re-assemble it (z80asm) and compare with the ROM.
Entry points: reset, RST vectors, the interrupt target (word at 3FFA/3FFC), everything the prototype really executed
(docs/evidence/S08_runtime_pcs.txt), plus targets found while tracing. RST helpers: 20h = jump table after the call (no return);
28h = 2 inline data bytes, 30h = 3 inline data bytes (then execution continues after them).
Usage: python3 tools/trace_disasm.py        (run from the project root)"""
import os, sys, subprocess, collections
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
PROG = os.path.join(ROOT, 'build/rom/prog.bin')
if not os.path.exists(PROG):          # rebuild the 16 KB program ROM from the player's own roms/pacman.zip (chips 6e, 6f, 6h, 6j in that order)
    import zipfile
    os.makedirs(os.path.dirname(PROG), exist_ok=True)
    z = zipfile.ZipFile(os.path.join(ROOT, 'roms', 'pacman.zip'))
    open(PROG, 'wb').write(b''.join(z.read(n) for n in ('pacman.6e', 'pacman.6f', 'pacman.6h', 'pacman.6j')))
ROM = open(PROG, 'rb').read()
assert len(ROM) == 0x4000
OUT = os.path.join(ROOT, 'docs/disasm')

def length(a):
    """Length in bytes of the instruction at a (Z80 incl. DD/FD/CB/ED)."""
    o = ROM[a]
    if o in (0xCB,): return 2
    if o == 0xED:
        return 4 if ROM[a+1] in (0x43,0x4B,0x53,0x5B,0x63,0x6B,0x73,0x7B) else 2
    if o in (0xDD, 0xFD):
        n = ROM[a+1]
        if n in (0xDD, 0xFD, 0xED): return 1
        if n == 0xCB: return 4
        base = base_len(n)
        if n in (0x21, 0x22, 0x2A): return 4
        if n == 0x36: return 4
        if n in (0x34,0x35,0x46,0x4E,0x56,0x5E,0x66,0x6E,0x7E,0x70,0x71,0x72,0x73,0x74,0x75,0x77,0x86,0x8E,0x96,0x9E,0xA6,0xAE,0xB6,0xBE): return 3
        return 1 + base
    return base_len(o)
def base_len(o):
    if o in (0x01,0x11,0x21,0x31,0x22,0x2A,0x32,0x3A,0xC3,0xCD) or (o & 0xC7) in (0xC2, 0xC4): return 3
    if o in (0x06,0x0E,0x16,0x1E,0x26,0x2E,0x36,0x3E,0xC6,0xCE,0xD6,0xDE,0xE6,0xEE,0xF6,0xFE,0x10,0x18,0x20,0x28,0x30,0x38,0xD3,0xDB): return 2
    return 1
def rel(a): return (a + 2 + (ROM[a+1] - 256 if ROM[a+1] > 127 else ROM[a+1])) & 0xFFFF

PUSHED = set()      # ROM addresses pushed as return addresses (ld hl,nn; push hl)
EXTRA = [0x2C44]  # never executed and never referenced, but a clean 20-instruction BCD routine that ends in RET exactly at the end of its data gap (2C44-2C5D): classified as code (dead code)
code = {}            # instruction start -> length
inline = {}          # start address of inline data after RST 28/30 -> length
tables = {}          # rst20 table start -> list of entries
work = []
parent = {}          # address -> address of the instruction that led the trace here (diagnostics)
CUR = [None]
def push(a):
    if 0 <= a < 0x4000 and a not in code:
        work.append(a); parent.setdefault(a, CUR[0])
def trace():
    while work:
        a = work.pop()
        while 0 <= a < 0x4000 and a not in code:
            o = ROM[a]; n = length(a); code[a] = n; CUR[0] = a
            pre = o in (0xDD, 0xFD, 0xED, 0xCB)
            nxt = a + n
            if pre:
                if o == 0xED and ROM[a+1] in (0x45, 0x4D): break
                if o in (0xDD, 0xFD) and ROM[a+1] == 0xE9: break
                parent.setdefault(nxt, a); a = nxt; continue
            if o == 0x21 and ROM[a+3] == 0xE5 and (ROM[a+1] | ROM[a+2] << 8) < 0x4000: PUSHED.add(ROM[a+1] | ROM[a+2] << 8)   # ld hl,nn / push hl: nn is probably a return address
            if o == 0xC3: push((ROM[a+1] | ROM[a+2] << 8)); break
            if o == 0xCD: push((ROM[a+1] | ROM[a+2] << 8))
            elif (o & 0xC7) == 0xC4: push((ROM[a+1] | ROM[a+2] << 8))
            elif (o & 0xC7) == 0xC2: push((ROM[a+1] | ROM[a+2] << 8))
            elif o == 0x18: push(rel(a)); break
            elif o in (0x10, 0x20, 0x28, 0x30, 0x38): push(rel(a))
            elif o in (0xC9, 0xE9): break
            elif (o & 0xC7) == 0xC7:                       # RST
                t = o & 0x38; push(t)
                if t == 0x20: tables[nxt] = None; break
                if t == 0x28: inline[nxt] = 2; nxt += 2
                if t == 0x30: inline[nxt] = 3; nxt += 3
                if t == 0x38: break
            parent.setdefault(nxt, a); a = nxt
RT = set()          # ROM bytes the prototype actually fetched (executed)
def stop_len(t):
    """Number of entries of the RST 20h table at t: words that look like ROM code addresses, stopping at a byte that is
    known code, was executed by the prototype, belongs to another table or inline data, or is not a plausible address."""
    n = 0; p = t
    while p + 1 < 0x4000:
        if p in code_bytes or (p in RT) or (p + 1 in RT) or p + 1 in code_bytes: break
        if p != t and (p in tables or p in inline): break
        w = ROM[p] | ROM[p+1] << 8
        if w >= 0x4000 or w < 0x0008: break
        n += 1; p += 2
    return n
code_bytes = set()
SEEN_PUSH = set()
def retrace(lens):
    global code, inline, tables, code_bytes
    code.clear(); inline.clear(); tables.clear(); parent.clear(); work.clear(); code_bytes = set(); PUSHED.clear(); SEEN_PUSH.clear()
    seeds = [0x0000, 0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030, 0x0038, 0x008D, ROM[0x3FFA] | ROM[0x3FFB] << 8, ROM[0x3FFC] | ROM[0x3FFD] << 8] + EXTRA
    for e in seeds: push(e)
    while True:
        trace()
        grew = False
        for t in sorted(tables):
            if tables[t] is None:
                n = lens[t] if t in lens else stop_len(t)
                tables[t] = [ROM[t + 2*i] | ROM[t + 2*i + 1] << 8 for i in range(n)]
                CUR[0] = t - 1
                for w in tables[t]: push(w)
                grew = True
        new = [x for x in sorted(PUSHED) if x not in code and x not in SEEN_PUSH]
        for x in new: SEEN_PUSH.add(x); CUR[0] = None; push(x)
        if not grew and not new: break
    code_bytes = set()
    for a, n in code.items(): code_bytes.update(range(a, a + n))
def read_tables():
    lens = {}
    for it in range(15):
        retrace(lens)
        new = {t: stop_len_final(t) for t in tables}
        if new == {t: len(e) for t, e in tables.items()}: return it
        lens = new
    raise SystemExit('table lengths did not converge')
def stop_len_final(t):
    # same rule, judged against the finished trace; the table's own entries are not code
    return stop_len(t)
def main():
    rt = [int(x, 16) for x in open(os.path.join(ROOT, 'docs/evidence/S08_runtime_pcs.txt')).read().split()]   # every ROM byte the prototype fetched while running (opcodes and operands)
    RT.update(rt)
    iters = read_tables()
    # check: every byte the prototype executed must lie inside a statically decoded instruction (or inline RST data read as code)
    covered = set()
    for a, n in code.items(): covered.update(range(a, a + n))
    miss = [a for a in rt if a not in covered]
    # overlap check
    owner = {}; overlaps = []
    for a, n in code.items():
        for k in range(a, a + n):
            if k in owner and owner[k] != a: overlaps.append((a, owner[k]))
            owner[k] = a
    kind = ['U'] * 0x4000
    for k in owner: kind[k] = 'C'
    for a, n in inline.items():
        for k in range(a, a + n):
            if kind[k] == 'U' or kind[k] == 'C' and k not in owner: kind[k] = 'I'
    for t, ents in tables.items():
        for k in range(t, t + 2 * len(ents)): kind[k] = 'T'
    # inline bytes that were also decoded as code: report but they stay code only if reached as code
    # Build block file: runs of same kind
    blocks = []; s = 0
    for i in range(1, 0x4001):
        if i == 0x4000 or kind[i] != kind[s]:
            blocks.append((s, i - 1, kind[s])); s = i
    names = {'C': 'code', 'I': 'bytedata', 'T': 'pointers', 'U': 'bytedata'}
    # z80dasm block file ('end' is exclusive). Comments are not allowed in this file; the legend is in docs/disasm/README.md
    with open(os.path.join(OUT, 'blocks.txt'), 'w') as f:
        for i, (a, b, k) in enumerate(blocks):
            f.write('%s%03d: start 0x%04x end 0x%04x type %s\n' % ({'C': 'code', 'I': 'inline', 'T': 'jumptable', 'U': 'data'}[k], i, a, b + 1, names[k]))
    return kind, blocks, miss, overlaps
def island_scan(kind):
    """Look for code-like byte sequences in the data runs: at least 8 instructions in a row, ending in RET/JP/JR, with no 00/FF bytes.
    Returns a list of (start, end, instruction count) not classified as code."""
    U = set(i for i, k in enumerate(kind) if k == 'U'); out = []; lastend = -1
    for a in sorted(U):
        if a <= lastend: continue
        p = a; n = 0; end = None
        while p < 0x4000 and p in U and n < 60:
            o = ROM[p]
            if o in (0x00, 0xFF): break
            n += 1; l = length(p)
            if o in (0xC9, 0xC3, 0x18, 0xE9): end = p; break
            p += l
        if end is not None and n >= 8: out.append((a, end, n)); lastend = end
    return out
def emit(kind, blocks, miss, overlaps):
    """Run z80dasm on the classified blocks, tidy the result, re-assemble with z80asm and compare with the ROM."""
    import re, hashlib
    asm = os.path.join(OUT, 'pacman.asm'); rebuilt = os.path.join(ROOT, 'build/s08/rebuilt.bin')
    os.makedirs(os.path.dirname(rebuilt), exist_ok=True)
    r = subprocess.run(['z80dasm', '-a', '-l', '-g', '0', '-b', os.path.join(OUT, 'blocks.txt'), '-o', asm, os.path.join(ROOT, 'build/rom/prog.bin')], capture_output=True, text=True)
    t = open(asm).read()
    last = blocks[-1]; nm = 'data%03d' % (len(blocks) - 1) if last[2] == 'U' else None
    m = re.search(r'^(\w+_end):\s+equ 0x4000\n', t, re.M)          # z80dasm names the end-of-ROM address as a label; use the plain number instead
    if m:
        t = t.replace(m.group(0), '').replace(m.group(1) + ':', '').replace(m.group(1), '04000h')
    # z80dasm names block boundaries '<block>_start' / '<block>_end'; rename them to the usual lXXXXh address labels
    addr = {}
    for i, (ba, bb, bk) in enumerate(blocks):
        nm = {'C': 'code', 'I': 'inline', 'T': 'jumptable', 'U': 'data'}[bk] + '%03d' % i
        addr[nm + '_start'] = ba; addr[nm + '_end'] = bb + 1
    defined = set(re.findall(r'^(l[0-9a-f]{4}h):', t, re.M))
    for nm, ad in sorted(addr.items(), key=lambda x: -len(x[0])):
        lab = 'l%04xh' % ad if ad < 0x4000 else '04000h'
        if re.search(r'^' + nm + r':', t, re.M):
            if lab in defined or ad >= 0x4000: t = re.sub(r'^' + nm + r':[^\n]*\n', '', t, flags=re.M)
            else: t = re.sub(r'^' + nm + r':', lab + ':', t, flags=re.M); defined.add(lab)
        t = re.sub(r'\b' + nm + r'\b', lab, t)
    t = re.sub(r'^; command line:.*\n', '; command line: z80dasm -a -l -g 0 -b docs/disasm/blocks.txt build/rom/prog.bin\n', t, flags=re.M)
    head = ('; Pac-Man (Midway "pacman" set) program ROM 0000-3FFF, code and data separated.\n'
            '; GENERATED by tools/trace_disasm.py (z80dasm + docs/disasm/blocks.txt). Do not edit by hand; names are added in step S09 onwards.\n'
            '; Legend: blocks named codeNNN are traced code, inlineNNN are data bytes that follow RST 28h/30h, jumptableNNN are RST 20h jump tables,\n'
            '; dataNNN are bytes never reached as code (tables, text, padding).\n')
    open(asm, 'w').write(head + t)
    z = subprocess.run(['z80asm', '-o', rebuilt, asm], capture_output=True, text=True)
    same = z.returncode == 0 and open(rebuilt, 'rb').read() == ROM
    return same, z.stderr.strip(), r.stderr.strip()
def report(kind, blocks, miss, overlaps, same, aerr, derr):
    cnt = collections.Counter(kind)
    runs = [(a, b) for a, b, k in blocks if k == 'U']
    L = []
    L.append('S08 round-trip report, ' + __import__('datetime').date.today().isoformat())
    L.append('ROM: build/rom/prog.bin sha1 ' + __import__('hashlib').sha1(ROM).hexdigest())
    L.append('Re-assembled docs/disasm/pacman.asm with z80asm and compared with prog.bin (cmp): ' + ('IDENTICAL, 16384 bytes' if same else 'DIFFERENT: ' + aerr))
    L.append('Coverage of the 16384 bytes:')
    L.append('  code (traced instructions):            %5d bytes, %d instructions' % (cnt['C'], len(code)))
    L.append('  inline data after RST 28h/30h:         %5d bytes' % cnt['I'])
    L.append('  RST 20h jump tables:                   %5d bytes in %d tables (%d entries)' % (cnt['T'], len(tables), sum(len(v) for v in tables.values())))
    L.append('  data / never reached as code:          %5d bytes in %d runs' % (cnt['U'], len(runs)))
    L.append('  unclassified:                          %5d bytes (every byte is either traced code, inline data, a jump table, or data listed in the runs below)' % 0)
    L.append('Checks: every ROM byte the prototype fetched while playing %d distinct bytes (docs/evidence/S08_runtime_pcs.txt) lies inside traced code: missing = %d; overlapping instructions = %d' % (len(RT), len(miss), len(overlaps)))
    L.append('Data runs (never reached as code), each described only by what refers to it (meaning is decided in later steps):')
    refs = collections.defaultdict(list)
    for ia, n in code.items():
        o = ROM[ia]; v = None
        if o in (0x01, 0x11, 0x21, 0x31, 0x22, 0x2A, 0x32, 0x3A) and n == 3: v = ROM[ia+1] | ROM[ia+2] << 8
        elif o == 0xED and n == 4: v = ROM[ia+2] | ROM[ia+3] << 8
        elif o in (0xDD, 0xFD) and n == 4 and ROM[ia+1] in (0x21, 0x22, 0x2A): v = ROM[ia+2] | ROM[ia+3] << 8
        if v is not None and v < 0x4000: refs[v].append(ia)
    def describe(a, b):
        d = ROM[a:b+1]; r = sorted(v for v in refs if a <= v <= b)
        what = 'all bytes %02X (padding)' % d[0] if len(set(d)) == 1 else '%d distinct byte values' % len(set(d))
        who = ('addresses loaded by code at %s' % ', '.join('%04X->%04X' % (refs[v][0], v) for v in r[:4]) + (' ...' if len(r) > 4 else '')) if r else 'no instruction loads an address inside it'
        return what + '; ' + who
    expl = {a: describe(a, b) for a, b in runs}
    for a, b in runs: L.append('  %04X-%04X (%4d bytes) %s' % (a, b, b - a + 1, expl.get(a, '')))
    L.append('Seeds used for tracing: reset 0000, RST vectors 08 10 18 20 28 30 38, interrupt code 008D and 3000 (word at 3FFA), word at 3FFC (008D), return addresses pushed with LD HL,nn / PUSH HL (%s), and dead code 2C44 (never executed, never referenced; a clean 20-instruction BCD routine ending in RET).' % ', '.join('%04X' % x for x in sorted(PUSHED)))
    L.append('RST helpers: 20h = jump table after the call, no return (%d tables); 28h = 2 inline data bytes (%d sites); 30h = 3 inline data bytes (%d sites).' % (len(tables), sum(1 for v in inline.values() if v == 2), sum(1 for v in inline.values() if v == 3)))
    L.append('RST 20h tables (start: entries): ' + '; '.join('%04X:%d' % (t, len(e)) for t, e in sorted(tables.items())))
    isl = island_scan(kind)
    L.append('Scan of the data runs for code-like sequences (8+ instructions in a row, ending in RET/JP/JR, no 00/FF bytes): %d found, none reached by any jump, call, table or executed by the prototype; they are left as data:' % len(isl))
    L.append('  ' + ', '.join('%04X-%04X (%d)' % x for x in isl))
    L.append('Known limits: tables never executed by the prototype (0417, 10B8, 21A6, 229B, 23A8) get their length from the end-of-table rule only; data runs may hide code that is only reached through unusual computed jumps: the scan above lists every code-like sequence so S09 to S11 can check them. What each data table means is NOT decided here (steps S10 and S12).')
    open(os.path.join(ROOT, 'docs/evidence/S08_roundtrip.txt'), 'w').write('\n'.join(L) + '\n')
    print('\n'.join(L))
if __name__ == '__main__':
    kind, blocks, miss, overlaps = main()
    same, aerr, derr = emit(kind, blocks, miss, overlaps)
    report(kind, blocks, miss, overlaps, same, aerr, derr)
    sys.exit(0 if same and not miss and not overlaps else 1)
