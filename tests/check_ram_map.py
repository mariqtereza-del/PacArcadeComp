"""S10 check: ram_map.md has >= 25 variables with address, meaning, evidence and confidence; every one has >= 3 checked moments in the evidence file; the oracle re-run reproduces the stored evidence with 0 failed rows."""
import os, re, subprocess, sys
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
md = open(os.path.join(root, 'docs/notes/ram_map.md')).read()
ev = open(os.path.join(root, 'docs/evidence/S10_ram_values.txt')).read()
rows = [l.split('|')[1:-1] for l in md.splitlines() if re.match(r'\| [0-9A-F]{4} \|', l) and len(l.split('|')) > 8]
rows = [[c.strip() for c in r] for r in rows]
bad = []
if len(rows) < 25: bad.append('only %d variables in table' % len(rows))
for r in rows:
    if len(r) < 8 or not all(r[:8]): bad.append('incomplete row %s' % r[0]); continue
    if r[6] not in ('HIGH', 'MED', 'LOW'): bad.append('%s: bad confidence %s' % (r[0], r[6]))
    m = re.search(r'^%s .*?\n((?:   \[.*\n)+)' % r[0], ev, re.M)
    if not m: bad.append('%s: no evidence block' % r[0]); continue
    lines = m.group(1).splitlines()
    if len(set(re.sub(r'\s+f\d+.*', '', l) for l in lines)) < 3: bad.append('%s: fewer than 3 moments' % r[0])
    if any('[FAIL]' in l for l in lines): bad.append('%s: FAIL row' % r[0])
res = subprocess.run([sys.executable, os.path.join(root, 'tools/analysis/oracle_ram.py'), '--check'], capture_output=True, text=True)
if res.returncode: bad.append('oracle --check: ' + res.stdout.strip())
if bad:
    print('check_ram_map FAILED'); [print(' ', b) for b in bad]; sys.exit(1)
print('check_ram_map OK: %d variables, each with >=3 checked moments, oracle reproduces stored evidence' % len(rows))
