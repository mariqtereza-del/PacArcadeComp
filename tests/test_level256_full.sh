#!/bin/sh
# S30A: builds and runs the level-256 tests and compares the result with MAME.
#   1. tests/test_level256_full.cpp  (about 25 s)  natural run level 1 -> 256 + the rack-test switch, writes build/s30a/ours_*.txt|bin
#   2. tests/test_high_levels.cpp    (about 35 s)  speeds, timing, power pill, Elroy and ghost targets on levels 5, 21 and 256
#   3. MAME 0.264 (apt package 'mame'; optional)   plays the same script (tools/analysis/mame_level256.lua), about 20 s, then
#      tools/analysis/compare_mame_level256.py compares level tables, events and the level-256 video RAM byte for byte.
#      MAME reads roms/pacman.zip in place; nothing from the ROM is copied into the project or the checkpoint zips.
#    With --pixels the MAME pass is the slower one (soft video, about 2.5 minutes) and also takes a screenshot of the level-256 screen, which is compared
#    pixel for pixel with the picture the port draws (tiles and sprites).
# Total about 2 minutes (5 with --pixels), so it is NOT part of tests/run_all.sh. Needs the ROM (roms/pacman.zip), g++ and build/app/miniz*.o (from the app build).
# Usage: sh tests/test_level256_full.sh [--no-mame | --pixels]
cd "$(dirname "$0")/.." || exit 2
W=build/s30a; mkdir -p "$W" docs/evidence
[ -f roms/pacman.zip ] || { echo "FAIL: roms/pacman.zip missing"; exit 2; }
[ -f build/app/miniz.o ] || { echo "FAIL: build/app/miniz*.o missing (build the app once first)"; exit 2; }
python3 tools/analysis/make_level_expect.py >/dev/null || exit 2
B="src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o"
F="-std=c++17 -O2 -Wall -Wextra -I src/third_party/miniz -DMINIZ_NO_STDIO -DMINIZ_NO_TIME"
g++ $F tests/test_level256_full.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp $B -o $W/t256full || { echo "FAIL: build test_level256_full"; exit 1; }
g++ $F tests/test_high_levels.cpp $B -o $W/thigh || { echo "FAIL: build test_high_levels"; exit 1; }
RC=0
$W/t256full roms/pacman.zip docs/evidence $W | tail -3 || RC=1
$W/thigh roms/pacman.zip docs/evidence | tail -2 || RC=1
if [ "$1" = "--no-mame" ] || [ ! -x /usr/games/mame ]; then
  echo "MAME comparison SKIPPED (MAME not installed or --no-mame). Install with: apt-get install mame"
  [ "$1" = "--no-mame" ] || echo "SKIPPED: MAME is not installed here; the C++ checks above still ran." > docs/evidence/S30A_mame_compare.txt
else
  T=$(mktemp -d); rm -f $W/mame_snapshot.png
  if [ "$1" = "--pixels" ]; then
    SNAP=1 OUT=$W/mame SDL_VIDEODRIVER=dummy /usr/games/mame -rompath roms -snapshot_directory "$T/snap" -cfg_directory "$T" -nvram_directory "$T" -noreadconfig pacman \
        -video soft -window -frameskip 10 -sound none -nothrottle -seconds_to_run 6000 -skip_gameinfo -autoboot_script tools/analysis/mame_level256.lua >/dev/null 2>&1
    cp "$T/snap/pacman/0000.png" $W/mame_snapshot.png 2>/dev/null
  else
    OUT=$W/mame /usr/games/mame -rompath roms -cfg_directory "$T" -nvram_directory "$T" -noreadconfig pacman -video none -sound none -nothrottle \
        -seconds_to_run 6000 -skip_gameinfo -autoboot_script tools/analysis/mame_level256.lua >/dev/null 2>&1
  fi
  python3 tools/analysis/compare_mame_level256.py $W docs/evidence/S30A_mame_compare.txt || RC=1
  rm -rf "$T"
fi
[ $RC -eq 0 ] && echo "S30A: ALL PASSED" || echo "S30A: FAILED"
exit $RC
