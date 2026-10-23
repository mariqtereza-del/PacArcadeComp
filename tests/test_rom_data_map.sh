#!/bin/sh
# N10: ROM data map. Runs the four coverage players with a test-only copy of the bus that reports every ROM read, groups the data reads
# (reads that are not instruction bytes) and checks that every one lies inside a table listed in docs/notes/rom_data_tables.md.
# Usage: sh tests/test_rom_data_map.sh [FRAMES_A FRAMES_B FRAMES_C FRAMES_D]   (default: the full run, about 5 minutes; the N10 evidence used the defaults)
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n10; mkdir -p $D
# the test-only bus copy (the game's src/board/bus.cpp is NOT changed): one hook call per ROM read
python3 - <<'PY' || { echo "FAIL: could not make the test bus"; exit 1; }
s = open("src/board/bus.cpp").read()
a = "uint8_t Bus::read(uint16_t a) {"
b = "Decoded d = decode_read(a);"
assert s.count(a) == 1 and s.count(b) >= 1 and s.count('#include "bus.h"') == 1
s = s.replace(a, "extern void (*rom_read_hook)(uint16_t);\n" + a, 1)
s = s.replace(b, b + "\n    if (d.region == Region::Rom && rom_read_hook) rom_read_hook(d.offset);", 1)
s = s.replace('#include "bus.h"', '#include "../../src/board/bus.h"', 1)
open("build/n10/bus_spy.cpp", "w").write(s)
PY
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/rom_data_map.cpp src/app/replay.cpp src/board/machine.cpp $D/bus_spy.cpp -o $D/rom_data_map || { echo "FAIL: build"; exit 1; }
if [ -n "$REUSE" ] && [ -f $D/data_map.txt ]; then echo "(reusing the report of the last run)" > $D/run.log; else $D/rom_data_map build/rom/prog.bin docs/notes/rom_routines.md docs/disasm/blocks.txt $D/data_map.txt "$@" > $D/run.log 2>&1; fi
rc=$?
cat $D/run.log
[ $rc -eq 0 ] || { echo "FAIL: the self-checks of the tool are not all 0"; exit 1; }
python3 - <<'PY' || exit 1
import re, sys
out = open("build/n10/data_map.txt").read()
doc = open("docs/notes/rom_data_tables.md").read()
tabs = [(int(a,16), int(b,16), n) for n, a, b in re.findall(r"^\| ([TS]\d\d) \| ([0-9A-F]{4})-([0-9A-F]{4}) \|", doc, re.M)]
# category rows: the blocks of docs/disasm/blocks.txt named jumptableNNN / inlineNNN
blocks = [l.split() for l in open("docs/disasm/blocks.txt") if l.strip() and not l.startswith("#")]
def starts(name): return [l for l in open("docs/disasm/blocks.txt") if l.startswith(name)]
nj, ni = len(starts("jumptable")), len(starts("inline"))
for tag, n in (("jumptable", nj), ("inline", ni)):
    m = re.search(r"^\| J\d\d \| [^|]*\| all blocks named %sNNN .*?\| (\d+) blocks" % tag, doc, re.M)
    if not m or int(m.group(1)) != n:
        print("FAIL: docs/notes/rom_data_tables.md does not quote the right number of %s blocks (blocks.txt has %d)" % (tag, n)); sys.exit(1)
sec = out.split("== 2b.")[1].split("== 3.")[0]
runs = re.findall(r"^([0-9A-F]{4})-([0-9A-F]{4}) \| \d+ \| (\S*) \|", sec, re.M)
bad = 0
for a, z, blk in runs:
    a, z = int(a,16), int(z,16)
    if blk.startswith("jumptable") or blk.startswith("inline"): continue
    if not any(lo <= a and z <= hi for lo, hi, _ in tabs):
        print("NOT LISTED: %04X-%04X (block %s)" % (a, z, blk)); bad += 1
print("data-read runs checked: %d (jump-table and inline blocks counted by block); listed tables: %d; not listed: %d" % (len(runs), len(tabs), bad))
# the special reads must lie in their listed ranges
sec3 = out.split("== 3.")[1].split("== 4.")[0]
for key, row in (("checksum loop", "S01"), ("random numbers", "S02"), ("interrupt vector", "S03")):
    line = [l for l in sec3.splitlines() if key in l]
    rng = [t for t in tabs if t[2] == row]
    if not line or not rng: print("FAIL: missing special read", key); sys.exit(1)
    for a, z in re.findall(r"([0-9A-F]{4})-([0-9A-F]{4})", line[0].split("ranges:")[1]):
        if not (rng[0][0] <= int(a,16) and int(z,16) <= rng[0][1]): print("NOT LISTED:", key, a, z); bad += 1
if bad: print("FAIL: data reads outside the listed tables"); sys.exit(1)
PY
echo "PASS test_rom_data_map: every data read of the coverage runs lies inside a listed table (report in $D/data_map.txt)"
