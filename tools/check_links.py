#!/usr/bin/env python3
"""N54: every link and every cited path in the top-level documents and in src/logic/README.md must exist.
Checks markdown links [text](path) (not http, not #anchors) and backticked repository paths (src/..., tests/..., tools/..., docs/..., mods/...).
In a GitHub export (.github_export present) the files the export leaves out are allowed to be missing."""
import os, re, sys
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOCS = ['README.md', 'ARCHITECTURE.md', 'BUILDING.md', 'CHANGELOG.md', 'CONTRIBUTING.md', 'MODDING.md', 'MACHINE_NOTES.md', 'src/logic/README.md', 'tests/oracle/README.md']
OMITTED = ('tests/data/rom_tables.json', 'tests/test_rom_tables.py', 'docs/disasm/pacman.asm', 'docs/disasm/pacman_raw.asm')
EXPORT = os.path.exists(os.path.join(ROOT, '.github_export'))
GENERATED = ('build/', 'dist/', 'roms/')   # made by building; not in the repository
bad, n = [], 0
def exists(base, rel):
    rel = rel.split('#')[0].rstrip('/')
    if not rel: return True
    return os.path.exists(os.path.normpath(os.path.join(base, rel)))
for d in DOCS:
    p = os.path.join(ROOT, d)
    if not os.path.exists(p): bad.append('missing document ' + d); continue
    t = open(p, encoding='utf-8').read()
    base = os.path.dirname(p)
    for m in set(re.findall(r'\]\(([^)\s]+)\)', t)):
        if m.startswith(('http://', 'https://', 'mailto:', '#')) or m.startswith('../../releases') or m.startswith('../../issues'): continue
        n += 1
        if not exists(base, m): bad.append('%s: link to missing %s' % (d, m))
    for m in set(re.findall(r'`((?:src|tests|tools|docs|mods)/[A-Za-z0-9_./-]+[A-Za-z0-9_])`', t)):
        if any(c in m for c in '*<>') or m.startswith(GENERATED): continue
        n += 1
        if EXPORT and m in OMITTED: continue
        if not exists(ROOT, m) and not exists(base, m): bad.append('%s: cites missing path %s' % (d, m))
if bad:
    print('FAIL'); [print(' -', b) for b in sorted(bad)]; sys.exit(1)
print('check_links OK: %d links and cited paths exist in %d documents' % (n, len(DOCS)))
