#!/bin/sh
# T02j: (1) every example mod in mods/ loads without wrong lines and changes what it says (slow ghosts, long blue time, double points), measured on the machine;
#       (2) the worked C++ example of MODDING.md: a copy of src/logic in which Pinky aims 6 tiles ahead of Pac-Man instead of 4 is built, and the target moves by 2 tiles;
#       (3) the change in the copy is NOT in src/logic (the real code is untouched);
#       (4) every file and link named in MODDING.md, mods/README.md and README.md exists.
# Usage: sh tests/test_mods.sh [FRAMES]   (default 30000; needs roms/pacman.zip for the ROM check build/rom/prog.bin, g++, python3)
cd "$(dirname "$0")/.." || exit 2
N=${1:-30000}
T=build/mods; mkdir -p $T
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
ls mods/*.cfg >/dev/null 2>&1 || { echo "FAIL: no example mods in mods/"; exit 1; }
[ "$(ls mods/*.cfg | wc -l)" -ge 3 ] || { echo "FAIL: fewer than three example mods"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle -Itests tests/test_mods.cpp src/board/machine.cpp src/board/bus.cpp -o $T/test_mods || { echo "FAIL: build"; exit 1; }
$T/test_mods build/rom/prog.bin "$N" mods/*.cfg || exit 1
# ---- the worked C++ example: Pinky aims 6 tiles ahead
rm -rf $T/logic_original $T/logic_pinky6; cp -r src/logic $T/logic_original; cp -r src/logic $T/logic_pinky6
grep -q 'pac_step \* 4' $T/logic_pinky6/ghost_targeting.h || { echo "FAIL: the line the guide tells you to change was not found in ghost_targeting.h"; exit 1; }
sed -i 's/pac_step \* 4/pac_step * 6/' $T/logic_pinky6/ghost_targeting.h
g++ -O2 -std=c++17 -Wall -Wextra -I$T/logic_original tests/test_mods_pinky.cpp -o $T/pinky_original || { echo "FAIL: build probe (original)"; exit 1; }
g++ -O2 -std=c++17 -Wall -Wextra -I$T/logic_pinky6 tests/test_mods_pinky.cpp -o $T/pinky_six || { echo "FAIL: build probe (changed copy)"; exit 1; }
A=$($T/pinky_original); B=$($T/pinky_six)
echo "Pinky's target (a b) with Pac-Man at 100 100 moving one tile step (1, 0): original rules: $A; copy with 6 tiles ahead: $B"
[ "$A" = "104 100" ] || { echo "FAIL: original Pinky target should be 104 100 (4 tiles ahead)"; exit 1; }
[ "$B" = "106 100" ] || { echo "FAIL: changed Pinky target should be 106 100 (6 tiles ahead)"; exit 1; }
grep -q 'pac_step \* 4' src/logic/ghost_targeting.h || { echo "FAIL: src/logic/ghost_targeting.h was changed (it must stay 4)"; exit 1; }
# ---- the documents: every path and link exists
python3 - <<'PY' || exit 1
import re, os, sys, glob
bad = 0
for doc in ["MODDING.md", "mods/README.md", "README.md", "ARCHITECTURE.md", "src/logic/README.md", "CHANGELOG.md"]:
    base = os.path.dirname(doc)
    text = open(doc, encoding="utf-8").read()
    for m in re.finditer(r"\]\(([^)#\s]+)(#[^)]*)?\)", text):          # markdown links
        t = m.group(1)
        if t.startswith(("http://", "https://", "mailto:", "../../")): continue   # (../../releases and ../../issues are GitHub pages)
        if not os.path.exists(os.path.normpath(os.path.join(base, t))):
            print(f"  missing link target in {doc}: {t}"); bad += 1
    for m in re.finditer(r"`((?:src|tests|tools|docs|mods|examples)/[A-Za-z0-9_./*-]+)`", text):  # `paths` in code spans
        t = m.group(1).rstrip(".")
        if not (os.path.exists(t) or glob.glob(t)):
            print(f"  missing path in {doc}: {t}"); bad += 1
print("documents: all linked files and paths exist" if not bad else f"documents: {bad} missing")
sys.exit(1 if bad else 0)
PY
echo "PASS test_mods: all example mods load and change what they say; the Pinky example moves the target from 104 to 106; the documents' links and paths exist"
