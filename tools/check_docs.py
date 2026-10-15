#!/usr/bin/env python3
"""S13: every path cited in MACHINE_NOTES.md and docs/SUMMARY_for_user.md must exist; required sections must be present."""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
need = ['## 1. Memory map', '## 2. Video', '## 3. Sound', '## 4. Timing', '## 5. Inputs', '## 6. Tables',
        '## 7. Routines', '## 8. Disagreements', '## 9. Sources']
bad, n = [], 0
# the GitHub export (tools/export_github.sh) leaves out the files made from the ROM and writes the marker file .github_export; those cited paths may then be missing
OMITTED = ('tests/data/rom_tables.json', 'tests/test_rom_tables.py', 'docs/disasm/pacman.asm', 'docs/disasm/pacman_raw.asm')
EXPORT = os.path.exists(os.path.join(ROOT, '.github_export'))
for doc in ('MACHINE_NOTES.md', 'docs/SUMMARY_for_user.md'):
    p = os.path.join(ROOT, doc)
    if not os.path.exists(p): bad.append('missing ' + doc); continue
    t = open(p).read()
    if doc == 'MACHINE_NOTES.md':
        bad += ['missing section ' + s for s in need if s not in t]
    for m in set(re.findall(r'`((?:docs|tools|tests|build|src)/[A-Za-z0-9_./-]+)`', t)) | set(re.findall(r'`(PROJECT_STATUS\.md|MACHINE_NOTES\.md)`', t)):
        n += 1
        if EXPORT and m in OMITTED: continue
        if not os.path.exists(os.path.join(ROOT, m)): bad.append('%s cites missing path %s' % (doc, m))
if bad:
    print('FAIL'); [print(' -', b) for b in bad]; sys.exit(1)
print('check_docs OK: %d cited paths exist, all 9 sections present' % n)
