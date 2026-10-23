#!/bin/sh
# N05: every ROM instruction is either executed by a coverage player or listed with a reason and a decision. Builds docs/notes/rom_unreached.md.
# Uses the coverage files of the tests N01 to N04 and N05 (it runs the ones that are missing). Needs build/rom/prog.bin and docs/disasm/*.
cd "$(dirname "$0")/.." || exit 2
[ -f build/n01/cov_replay.txt ] || sh tests/test_rom_map.sh >/dev/null || { echo "FAIL: N01"; exit 1; }
[ -f build/n02/cov_a.txt ] || sh tests/test_coverage_a.sh >/dev/null || { echo "FAIL: player A"; exit 1; }
[ -f build/n03/cov_b.txt ] || sh tests/test_coverage_b.sh >/dev/null || { echo "FAIL: player B"; exit 1; }
[ -f build/n04/cov_c.txt ] || sh tests/test_coverage_c.sh >/dev/null || { echo "FAIL: player C"; exit 1; }
[ -f build/n04/cov_d.txt ] || sh tests/test_coverage_d.sh >/dev/null || { echo "FAIL: player D"; exit 1; }
python3 tests/rom_unreached.py docs/notes/rom_unreached.md build/n01/cov_replay.txt build/n01/cov_player.txt build/n02/cov_a.txt build/n03/cov_b.txt build/n04/cov_c.txt build/n04/cov_d.txt || { echo "FAIL: an unreached instruction has no reason, or a reason is stale"; exit 1; }
# no ROM bytes in the notes file (16-byte window rule of the export)
python3 - <<'P' || { echo "FAIL: ROM bytes in the notes file"; exit 1; }
import zipfile, sys
b = open('docs/notes/rom_unreached.md', 'rb').read()
with zipfile.ZipFile('roms/pacman.zip') as z:
    for n in z.namelist():
        d = z.read(n)
        for i in range(0, len(d) - 15):
            w = d[i:i+16]
            if len(set(w)) >= 8 and (w in b or w.hex().encode() in b.replace(b' ', b'')): print('FOUND', n, i); sys.exit(1)
print('notes file: no run of 16 ROM bytes')
P
echo "PASS test_rom_unreached: every instruction is executed or listed with a reason and a decision"
