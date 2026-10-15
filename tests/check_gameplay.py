#!/usr/bin/env python3
"""Step S11 check: the S11 section of docs/disasm/labels.txt has 8 well-formed labels (unique names and addresses, HIGH/MED, address is an
instruction start in the S08 trace), each name and address appears in docs/notes/gameplay_routines.md, the notes refer to the evidence file, and
the prototype trace (tools/analysis/oracle_s11.py --check) reproduces docs/evidence/S11_gameplay_trace.txt with the key relations still true."""
import sys, os, subprocess
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.join(root, 'tools'))
import trace_disasm as T
T.main()
notes = open(os.path.join(root, 'docs/notes/gameplay_routines.md')).read()
ev = open(os.path.join(root, 'docs/evidence/S11_gameplay_trace.txt')).read()
fails = []; names = set(); addrs = set(); n = 0; inS11 = False
for i, l in enumerate(open(os.path.join(root, 'docs/disasm/labels.txt')), 1):
    if l.startswith('# ==== S11'): inS11 = True; continue
    if not inS11 or l.startswith('#') or not l.strip(): continue
    p = [x.strip() for x in l.split('|')]; n += 1
    if len(p) != 6: fails.append(f'line {i}: expected 6 fields'); continue
    a, name, conf, kind, purpose, e = p; ai = int(a, 16)
    if conf not in ('HIGH', 'MED'): fails.append(f'line {i}: confidence must be HIGH or MED, got {conf}')
    if not purpose or not e: fails.append(f'line {i}: purpose or evidence missing')
    if name in names or ai in addrs: fails.append(f'line {i}: duplicate')
    names.add(name); addrs.add(ai)
    if ai not in T.code: fails.append(f'line {i}: {a} is not an instruction start in the S08 trace')
    if name not in notes or a not in notes: fails.append(f'line {i}: {name} / {a} not in gameplay_routines.md')
    if 'trace' not in e.lower(): fails.append(f'line {i}: evidence does not cite a trace')
if n != 8: fails.append(f'expected 8 S11 labels, found {n}')
for need in ('never the reverse of the old one: True', 'equals Pac-Man tile word 4D39/4D3A every time: True', '{2: 3168}', 'executed 101 times and 4E0E rose by exactly 1: 101'.replace('executed 101 times and 4E0E rose by exactly 1: 101', 'frames where 4E0E rose by exactly 1: 101')):
    if need not in ev: fails.append('evidence file lacks: ' + need)
r = subprocess.run([sys.executable, os.path.join(root, 'tools/analysis/oracle_s11.py'), '--check'], capture_output=True, text=True)
if r.returncode != 0: fails.append('S11 trace differs from the stored evidence: ' + r.stdout.strip() + r.stderr[-300:])
print('\n'.join('FAIL: ' + f for f in fails) if fails else f'PASS S11: {n} gameplay labels well formed and cited, trace identical to the recorded evidence')
sys.exit(1 if fails else 0)
