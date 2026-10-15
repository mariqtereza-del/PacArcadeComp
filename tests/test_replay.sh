#!/bin/sh
# S27: replay and determinism. The same scripted two-minute play (7,273 frames: a game for player 1, then a two-coin game started with START 2) is
#  1. written by the Python side (tools/analysis/oracle_replay.py --make),
#  2. run twice on the C++ machine: the two frame-hash logs must be identical (determinism),
#  3. run on the Python prototype (exact 50,688-cycle frames, address mirrors): its log must equal the C++ log frame for frame,
#  4. passed through the recorder: recording the inputs while replaying and loading the saved file must give the same replay.
# Each log line is "frame sha1" over memory 4000-4FFF plus the 48 bytes at 5040-506F. Needs the ROM (roms/pacman.zip) and g++.
# Usage: sh tests/test_replay.sh [FRAMES]   (default 7273 = 120 s)
cd "$(dirname "$0")/.." || exit 2
N=${1:-7273}
T=$(mktemp -d)
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || { echo "FAIL: no ROM (put pacman.zip in roms/)"; exit 2; }
g++ -O2 -std=c++17 -Wall -Wextra -DREPLAY_MAIN src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o "$T/replay_tool" || { echo "FAIL: build"; exit 1; }
python3 tools/analysis/oracle_replay.py --make "$T/script.replay" "$N" || { echo "FAIL: could not write the replay"; exit 1; }
"$T/replay_tool" build/rom/prog.bin "$T/script.replay" "$T/cpp1.log" --record "$T/recorded.replay" > "$T/summary1.txt" || { echo "FAIL: C++ run 1"; exit 1; }
"$T/replay_tool" build/rom/prog.bin "$T/script.replay" "$T/cpp2.log" > /dev/null || { echo "FAIL: C++ run 2"; exit 1; }
if ! cmp -s "$T/cpp1.log" "$T/cpp2.log"; then
  L=$(cmp "$T/cpp1.log" "$T/cpp2.log" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p'); echo "FAIL: two C++ runs differ, first at frame $L"; exit 1; fi
python3 tools/analysis/oracle_replay.py --hashes "$T/script.replay" "$T/py.log" || { echo "FAIL: Python run"; exit 1; }
if ! cmp -s "$T/cpp1.log" "$T/py.log"; then
  L=$(cmp "$T/cpp1.log" "$T/py.log" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p')
  echo "FAIL: C++ and prototype differ, first at frame $L"; echo "  c++   : $(sed -n "${L}p" "$T/cpp1.log")"; echo "  python: $(sed -n "${L}p" "$T/py.log")"; exit 1; fi
# the recorder must reproduce the replay it was fed (the file is compared after both are rewritten in the same normal form)
if ! cmp -s "$T/script.replay" "$T/recorded.replay"; then echo "FAIL: recorded replay differs from the original"; diff "$T/script.replay" "$T/recorded.replay" | head -5; exit 1; fi
# the run must be a real game: many different states, more than one game started
D=$(awk '{print $2}' "$T/cpp1.log" | sort -u | wc -l)
if [ "$D" -lt 1000 ]; then echo "FAIL: only $D distinct frame states, the script did not play a game"; exit 1; fi
echo "PASS: $(wc -l < "$T/cpp1.log") frame hashes identical (C++ run 1 = run 2 = Python prototype); recorder round trip identical"
echo "  $(cat "$T/summary1.txt")"
rm -rf "$T"
