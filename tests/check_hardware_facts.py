#!/usr/bin/env python3
"""Step S07 check: every fact row in docs/notes/hardware_facts.md carries a status (CONFIRMED, CORRECTED, NEW or UNRESOLVED);
at most 3 UNRESOLVED facts, each listed in section 8 as U1..U3 with an 'Experiment'; the 3 known inconsistencies are settled in section 7."""
import re, sys, os
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'notes', 'hardware_facts.md')
t = open(P).read(); fails = []
WORDS = ('CONFIRMED', 'CORRECTED', 'NEW', 'UNRESOLVED')
rows = 0; unresolved_rows = 0
for n, line in enumerate(t.split('\n'), 1):
    if line.startswith('|') and not re.match(r'\|[-| ]+\|$', line):
        cells = [c.strip() for c in line.strip('|').split('|')]
        if cells[0] in ('Fact', 'Range', 'Address', 'Bit', 'Bits'): continue
        sec = None
        if len(cells) >= 3 and any(w in ' '.join(cells[1:]) for w in WORDS):
            rows += 1; unresolved_rows += 'UNRESOLVED' in line
        elif len(cells) >= 3 and cells[0] and not re.match(r'^[0-9A-Fa-f -]+$', cells[0]) and not any(k in cells[0] for k in ('joystick', 'coin', 'Rack', 'service', 'start', 'cabinet', 'coinage', 'lives', 'bonus', 'difficulty', 'ghost')):
            fails.append(f'line {n}: no status: {line[:80]}')
if rows < 40: fails.append(f'only {rows} fact rows with a status')
sec8 = t.split('## 8.')[-1]
u = re.findall(r'^- U(\d)\. ', sec8, re.M)
if not 0 <= len(u) <= 3: fails.append('more than 3 UNRESOLVED facts')
if len(u) != unresolved_rows and unresolved_rows > len(u): fails.append('UNRESOLVED row without U entry')
for block in re.split(r'\n- U\d\. ', sec8)[1:]:
    if 'Experiment' not in block: fails.append('a U entry has no Experiment')
sec7 = t.split('## 7.')[1].split('## 8.')[0]
if len(re.findall(r'^\d\. ', sec7, re.M)) != 3: fails.append('section 7 must settle 3 inconsistencies')
print('\n'.join('FAIL: ' + f for f in fails) if fails else f'PASS S07: {rows} fact rows have a status, {len(u)} UNRESOLVED each with an experiment, 3 inconsistencies settled')
sys.exit(1 if fails else 0)
