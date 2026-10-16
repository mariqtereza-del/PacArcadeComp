#!/bin/sh
# S37: builds a CLEAN copy of the project for GitHub, and refuses to finish if anything made from the original ROM is in it.
# Usage:  sh tools/export_github.sh [OUTPUT_FOLDER]        (default: ../PacArcadeComp-github next to the project folder)
# Needs the project's own ROM only for the final safety scan: it reads the ROM from roms/pacman.zip (or build/rom/) into a temporary folder
# and removes it again. The ROM itself is never copied into the export.
# What goes in: the source (src/), tests (tests/), tools (tools/), documentation (docs/), the licence files and the top-level documents.
# What stays out (and why):
#   build/ dist/PacArcadeComp dist/PacmanPortTest roms/ *.zip *.exe *.o   build output (dist/res, dist/README-PACKAGE.txt and dist/REPORT-A-PROBLEM.bat stay: tools/package.sh needs them), the player's ROM, release files (the exe and zip go on the GitHub Releases page)
#   docs/disasm/pacman.asm, pacman_raw.asm   the disassembly contains the original program's instructions (made from the ROM)
#   tests/data/rom_tables.json, tests/test_rom_tables.py   tables decoded from the ROM, and the test that compares them
#   docs/evidence/*.png, docs/evidence/prototype/   screenshots of the running game: they show the game's graphics (made from the ROM). Only docs/images/ (our own menus) is published
#   docs/evidence/*user*  the user's own test reports and screenshots (private)
#   PROJECT_STATUS.md has the "## User" section removed (personal notes)
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
OUT=${1:-"$HERE/../PacArcadeComp-github"}
case "$OUT" in /*) ;; *) OUT="$(pwd)/$OUT";; esac
cd "$HERE"
rm -rf "$OUT"; mkdir -p "$OUT"
echo "Copying files to $OUT ..."
tar cf - \
  --exclude='./build' --exclude='./dist/PacArcadeComp' --exclude='./dist/PacmanPortTest' --exclude='./dist/PacArcadeComp-*' --exclude='./roms' --exclude='./.git' \
  --exclude='__pycache__' --exclude='*.zip' --exclude='*.exe' --exclude='*.o' --exclude='*.bin' \
  --exclude='./docs/disasm/pacman.asm' --exclude='./docs/disasm/pacman_raw.asm' \
  --exclude='./tests/data/rom_tables.json' --exclude='./tests/test_rom_tables.py' \
  --exclude='./docs/evidence/*user*' --exclude='./docs/evidence/*.png' --exclude='./docs/evidence/prototype' --exclude='./MACHINE_NOTES.md.bak' \
  . | (cd "$OUT" && tar xf -)
# personal notes out of the status file
awk 'BEGIN{skip=0} /^## User/{skip=1; next} /^## /{skip=0} skip==0{print}' PROJECT_STATUS.md > "$OUT/PROJECT_STATUS.md"
echo 'This folder is the GitHub export: files made from the original ROM are left out (see docs/GITHUB_UPLOAD_GUIDE.md and docs/evidence/S37_github.txt).' > "$OUT/.github_export"
# the plan page is rebuilt from tools/plan_data.py; keep it as is
echo "Scanning the export for ROM bytes (runs of 16 bytes taken from any ROM file, raw, as hex text and as number tables) ..."
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
if [ -d build/rom ] && [ -f build/rom/pacman.6e ]; then cp build/rom/pacman.* build/rom/82s* "$TMP"/ 2>/dev/null || true
elif [ -f roms/pacman.zip ]; then python3 - "$TMP" <<'PY'
import sys, zipfile
z = zipfile.ZipFile('roms/pacman.zip')
for n in z.namelist():
    if not n.endswith('/'): open(sys.argv[1] + '/' + n.split('/')[-1], 'wb').write(z.read(n))
PY
else echo "STOP: no ROM found (roms/pacman.zip or build/rom), so the safety scan cannot run. Put your pacman.zip in roms/ and try again."; rm -rf "$OUT"; exit 2; fi
python3 - "$TMP" "$OUT" <<'PY'
import os, re, sys
rom_dir, root = sys.argv[1], sys.argv[2]
W = 16
rom = [(n, open(os.path.join(rom_dir, n), 'rb').read()) for n in sorted(os.listdir(rom_dir)) if os.path.isfile(os.path.join(rom_dir, n))]
wins = {}
for n, d in rom:
    for i in range(len(d) - W + 1):
        w = d[i:i + W]
        # skip runs that are too boring to be a copy (few different values, or counting 0,1,2,...)
        if len(set(w)) >= 8 and len({(w[k + 1] - w[k]) & 255 for k in range(W - 1)}) >= 4: wins.setdefault(w, (n, i))
print("  ROM files read: %d, 16-byte windows: %d" % (len(rom), len(wins)))
def raw_hit(b):
    for i in range(len(b) - W + 1):
        if b[i:i + W] in wins: return wins[b[i:i + W]]
def expansions(vals):
    out = []
    for mode in ('be', 'le', 'wordle'):
        bs = bytearray()
        for v, w in vals:
            if w == 1: bs.append(v & 255)
            elif w == 2: bs += v.to_bytes(2, 'big' if mode == 'be' else 'little')
            elif mode == 'be': bs += v.to_bytes(4, 'big')
            elif mode == 'le': bs += v.to_bytes(4, 'little')
            else: bs += (v >> 16).to_bytes(2, 'little') + (v & 0xFFFF).to_bytes(2, 'little')
        out.append(bytes(bs))
    return out
def text_hit(text):
    for m in re.finditer(r'(?:[0-9a-fA-F]{2}[ ,:]?){16,}', text):
        s = re.sub(r'[^0-9a-fA-F]', '', m.group(0))
        if len(s) % 2 == 0:
            try:
                h = raw_hit(bytes.fromhex(s))
                if h: return h
            except ValueError: pass
    for run in re.finditer(r'(?:(?:0[xX][0-9a-fA-F]+|(?<![\w.])[0-9]{1,5}(?![\w.]))[ \t\r\n,;{}\[\]]*){4,}', text):
        toks = []
        for m in re.finditer(r'0[xX]([0-9a-fA-F]+)|(?<![\w.])([0-9]{1,5})(?![\w.])', run.group(0)):
            if m.group(1):
                w = len(m.group(1)); w = 1 if w <= 2 else 2 if w <= 4 else 4; v = int(m.group(1), 16) & ((1 << (8 * w)) - 1)
            else:
                v = int(m.group(2)); w = 1 if v < 256 else 2
                if v > 65535: v = 0
            toks.append((v, w))
        for i in range(len(toks)):
            seg = toks[i:i + 16]
            if len(seg) < 4: continue
            for bs in expansions(seg):
                h = raw_hit(bs)
                if h: return h
bad = []; count = 0
for dp, dn, fn in os.walk(root):
    dn[:] = [d for d in dn if d not in ('.git', '__pycache__')]
    for f in fn:
        p = os.path.join(dp, f); rel = os.path.relpath(p, root); data = open(p, 'rb').read(); count += 1
        h = raw_hit(data)
        if not h:
            try: h = text_hit(data.decode('utf-8'))
            except UnicodeDecodeError: pass
        if h: bad.append((rel, h))
print("  files scanned: %d" % count)
for rel, h in bad: print("  ROM BYTES FOUND in %s (matches %s at offset %d)" % (rel, h[0], h[1]))
sys.exit(1 if bad else 0)
PY
RESULT=$?
if [ "$RESULT" != 0 ]; then echo "STOP: the export contains ROM bytes (see above). It was deleted."; rm -rf "$OUT"; exit 1; fi
echo "Scan clean. Export ready: $OUT"
# the release zip (exe) is NOT part of the repository; if it exists, copy it beside the export for the GitHub Releases page, after the same scan
REL=$(ls dist/PacArcadeComp-*-win64.zip 2>/dev/null | tail -1 || true)
if [ -n "$REL" ]; then
  RELDIR="$(dirname "$OUT")/PacArcadeComp-release"; rm -rf "$RELDIR"; mkdir -p "$RELDIR"; cp "$REL" "$RELDIR"/
  mkdir -p "$TMP/relcheck" && ( cd "$TMP/relcheck" && unzip -q "$HERE/$REL" )
  python3 - "$TMP" "$TMP/relcheck" <<'PY' || { echo "STOP: the release zip contains ROM bytes. Not copied."; rm -rf "$RELDIR"; exit 1; }
import os, sys
rom_dir, root = sys.argv[1], sys.argv[2]; W = 16; wins = set()
for n in os.listdir(rom_dir):
    p = os.path.join(rom_dir, n)
    if not os.path.isfile(p): continue
    d = open(p, 'rb').read()
    for i in range(len(d) - W + 1):
        w = d[i:i + W]
        if len(set(w)) >= 8 and len({(w[k + 1] - w[k]) & 255 for k in range(W - 1)}) >= 4: wins.add(w)
n = 0
for dp, dn, fn in os.walk(root):
    for f in fn:
        b = open(os.path.join(dp, f), 'rb').read(); n += 1
        if any(b[i:i + W] in wins for i in range(len(b) - W + 1)): print("  ROM BYTES FOUND in release file", f); sys.exit(1)
print("  release zip: %d files scanned, clean" % n)
PY
  echo "Release zip copied to $RELDIR (upload it on the GitHub Releases page)."
fi
echo "Done. Next: docs/GITHUB_UPLOAD_GUIDE.md"
