#!/usr/bin/env python3
"""Step S06 check: every fact line in docs/notes/hardware_facts_lomont.md carries a citation like [p14] or [Table 1, p3]
(or an explicit [source: ...] tag when the fact is not from Lomont). Table rows inherit the citation in their section heading.
Exempt: the final 'Points that look inconsistent' section (our own analysis) must exist and be non-empty."""
import re, sys, os
P = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'docs', 'notes', 'hardware_facts_lomont.md')
bad, section, head_cited, incons = [], '', False, 0
for n, line in enumerate(open(P).read().split('\n'), 1):
    if line.startswith('## '):
        section = line; head_cited = '[' in line; continue
    if section.startswith('## Points that look inconsistent'):
        if re.match(r'\d+\. ', line): incons += 1
        continue
    if section and line.startswith('- ') and '[' not in line and not head_cited: bad.append((n, line[:90]))
for n, l in bad: print(f'line {n}: no citation: {l}')
if incons < 1: print('missing: list of inconsistencies'); bad.append((0, ''))
print('PASS S06: every fact line cited; %d inconsistencies listed' % incons if not bad else 'FAIL S06')
sys.exit(1 if bad else 0)
