#!/usr/bin/env python3
"""Step S09 check: docs/disasm/labels.txt is well formed (10+ labels, unique names and addresses, confidence HIGH/MED/LOW, purpose and evidence
present, address is an instruction start or data start in the S08 classification), every label name appears in docs/notes/boot_flow.md, and the
prototype run still reaches the boot routines in the recorded frames (tools/analysis/oracle_s09.py --check)."""
import sys, os, subprocess
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, os.path.join(root, 'tools'))
import trace_disasm as T
T.main()
fails = []; names = set(); addrs = set(); n = 0
flow = open(os.path.join(root, 'docs/notes/boot_flow.md')).read()
for i, l in enumerate(open(os.path.join(root, 'docs/disasm/labels.txt')), 1):
    if l.startswith('# ==== S11'): break          # S11 labels are checked by tests/check_gameplay.py
    if l.startswith('#') or not l.strip(): continue
    p = [x.strip() for x in l.split('|')]; n += 1
    if len(p) != 6: fails.append(f'line {i}: expected 6 fields'); continue
    a, name, conf, kind, purpose, ev = p; a = int(a, 16)
    if conf not in ('HIGH', 'MED', 'LOW'): fails.append(f'line {i}: bad confidence {conf}')
    if not purpose or not ev: fails.append(f'line {i}: purpose or evidence missing')
    if name in names or a in addrs: fails.append(f'line {i}: duplicate name or address')
    names.add(name); addrs.add(a)
    if kind != 'data' and a not in T.code: fails.append(f'line {i}: {a:04X} is not an instruction start in the S08 trace')
    if kind == 'data' and a not in T.tables: fails.append(f'line {i}: {a:04X} is not a jump table start')
    if name not in flow: fails.append(f'line {i}: {name} not mentioned in boot_flow.md')
if n < 10: fails.append('fewer than 10 labels')
r = subprocess.run([sys.executable, os.path.join(root, 'tools/analysis/oracle_s09.py'), '--check'], capture_output=True, text=True)
if r.returncode != 0: fails.append('prototype trace differs from docs/evidence/S09_prototype_trace.txt: ' + r.stdout.strip() + r.stderr[-300:])
print('\n'.join('FAIL: ' + f for f in fails) if fails else f'PASS S09: {n} labels well formed, all in boot_flow.md, prototype trace identical to the recorded evidence')
sys.exit(1 if fails else 0)
