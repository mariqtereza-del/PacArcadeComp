#!/bin/sh
# N01: the coverage recorder and the routine map. Builds tests/rom_map.cpp, runs the S27 replay and the T01a player (default 60,000 frames), builds
# docs/notes/rom_routines.md with tests/rom_map.py and checks it. Needs build/rom/prog.bin and docs/disasm/*. Usage: sh tests/test_rom_map.sh [PLAYER_FRAMES]
cd "$(dirname "$0")/.." || exit 2
N=${1:-60000}
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
D=build/n01; mkdir -p $D
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/rom_map.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/rom_map || { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make $D/script.replay 7273 >/dev/null || { echo "FAIL: replay script"; exit 1; }
$D/rom_map build/rom/prog.bin $D/script.replay $N $D/cov || { echo "FAIL: run"; exit 1; }
python3 tests/rom_map.py $D/cov_replay.txt $D/cov_player.txt docs/notes/rom_routines.md || { echo "FAIL: map"; exit 1; }
# the same run twice gives the same map (deterministic)
cp docs/notes/rom_routines.md $D/map1.md
$D/rom_map build/rom/prog.bin $D/script.replay $N $D/cov2 >/dev/null && python3 tests/rom_map.py $D/cov2_replay.txt $D/cov2_player.txt $D/map2.md >/dev/null && cmp -s $D/map1.md $D/map2.md || { echo "FAIL: the map is not repeatable"; exit 1; }
# the notes file holds no ROM bytes: only the hexadecimal addresses and counts the script writes (scan with the export's 16-byte window rule)
python3 - <<'P' || { echo "FAIL: ROM bytes in the notes file"; exit 1; }
import zipfile, sys
b = open('docs/notes/rom_routines.md','rb').read()
hexs = b.hex().encode()
with zipfile.ZipFile('roms/pacman.zip') as z:
    for n in z.namelist():
        d = z.read(n)
        for i in range(0, len(d) - 15):
            w = d[i:i+16]
            if len(set(w)) >= 8 and (w in b or w.hex().encode() in b.replace(b' ', b'')): print('FOUND', n, i); sys.exit(1)
print('notes file: no run of 16 ROM bytes')
P
echo "PASS test_rom_map: map complete (every code block once), coverage consistent, repeatable, no ROM bytes"
