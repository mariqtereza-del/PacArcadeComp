#!/bin/sh
# T02i, rebuilt in N09: builds the Windows TEST folder for the user: dist/PacArcadeCompTest-<LABEL>/ and dist/PacArcadeCompTest-<LABEL>-win64.zip with
#   PacArcadeComp.exe (this source), PLAY-ORIGINAL.bat (the original ROM routines), PLAY-READABLE.bat (--rules readable: the readable rules, later also the
#   native routines), README-TEST.txt (one page, plain words) and the licence file. NEVER the ROM.
# Each .bat starts the game with its own settings folder (settings-original, settings-readable next to the exe), so a rules choice saved earlier can not mix
# the two, and writes a log (log-original.txt, log-readable.txt) that says which rules ran.
# Usage: sh tools/package_test.sh N09        (the label shows in the README; dist/README-TEST-<LABEL>.txt is used instead of dist/README-TEST.txt when it exists)
set -e
SDL=${SDL2_MINGW:-/tmp/sdl2win/SDL2-2.32.10/x86_64-w64-mingw32}
cd "$(dirname "$0")/.."
sh tests/test_no_z80_symbols.sh >/dev/null || { echo "STOP: the program source still contains the Z80 core"; exit 1; }   # N49c: the release never contains the Z80 core
L=${1:?give a label like N09}
OUT=dist/PacArcadeCompTest-$L; ZIP=dist/PacArcadeCompTest-$L-win64.zip
sh tools/build_windows.sh >/dev/null 2>&1
mkdir -p build/app/win; rm -rf $OUT $ZIP; mkdir -p $OUT
( cd dist/res && x86_64-w64-mingw32-windres app.rc -O coff -o ../../build/app/win/app_res.o )
x86_64-w64-mingw32-g++ -O2 -s -std=c++17 -Wall -Wextra -DPAC_NO_Z80 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I"$SDL/include/SDL2" -Dmain=SDL_main \
  src/app/main.cpp src/app/firstrun.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp \
  build/app/win/miniz.o build/app/win/miniz_tdef.o build/app/win/miniz_tinfl.o build/app/win/miniz_zip.o build/app/win/app_res.o \
  -o $OUT/PacArcadeComp.exe \
  -L"$SDL/lib" -lmingw32 -lSDL2main -lSDL2 -mwindows \
  -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32 -lcomdlg32 \
  -static -static-libgcc -static-libstdc++
# the two double-click files (Windows line ends)
mk_bat() { # $1 file, $2 title line, $3 settings folder, $4 log, $5 extra option
  {
    printf '@echo off\r\n'
    printf 'rem %s\r\n' "$2"
    printf 'cd /d "%%~dp0"\r\n'
    printf 'if not exist pacman.zip (\r\n'
    printf '  echo pacman.zip is not next to PacArcadeComp.exe. Copy your pacman.zip into this folder, then double-click this file again.\r\n'
    printf '  pause\r\n'
    printf '  exit /b 1\r\n'
    printf ')\r\n'
    printf 'echo %s\r\n' "$2"
    printf 'echo Play for a minute, then close the game with Esc and QUIT GAME.\r\n'
    printf 'PacArcadeComp.exe --config "%%~dp0%s\\controls.cfg" --report "%%~dp0%s" %s\r\n' "$3" "$4" "$5"
    printf 'echo.\r\n'
    printf 'echo Done. The file %s (next to this file) says which rules ran.\r\n' "$4"
    printf 'pause\r\n'
  } > "$1"
}
mk_bat $OUT/PLAY-ORIGINAL.bat "PLAY-ORIGINAL: the game with the original arcade routines" settings-original log-original.txt "--rules original"
mk_bat $OUT/PLAY-READABLE.bat "PLAY-READABLE: the game with the readable (and later native) routines" settings-readable log-readable.txt "--rules readable"
README=dist/README-TEST.txt; [ -f dist/README-TEST-$L.txt ] && README=dist/README-TEST-$L.txt
sed "s/@LABEL@/$L/g" $README | sed 's/$/\r/' > $OUT/README-TEST.txt
cp THIRD-PARTY-LICENSES.txt $OUT/
( cd dist && zip -q -r -X $(basename $ZIP) PacArcadeCompTest-$L )
# the ROM scan on the finished zip (same rule as tools/export_github.sh: no run of 16 bytes taken from any ROM file); needs roms/pacman.zip
if [ -f roms/pacman.zip ]; then
python3 - "$ZIP" <<'PY' || { echo "STOP: the test zip contains ROM bytes. Deleted."; rm -rf "$ZIP"; exit 1; }
import sys, zipfile
W = 16; wins = set()
rom = zipfile.ZipFile('roms/pacman.zip')
for n in rom.namelist():
    d = rom.read(n)
    for i in range(len(d) - W + 1):
        w = d[i:i + W]
        if len(set(w)) >= 8 and len({(w[k + 1] - w[k]) & 255 for k in range(W - 1)}) >= 4: wins.add(w)
z = zipfile.ZipFile(sys.argv[1]); n = 0
for name in z.namelist():
    if name.endswith('/'): continue
    b = z.read(name); n += 1
    if any(b[i:i + W] in wins for i in range(len(b) - W + 1)): print("  ROM BYTES FOUND in", name); sys.exit(1)
print("  test zip: %d files scanned, no run of 16 ROM bytes (clean)" % n)
PY
else echo "  (no roms/pacman.zip here: the ROM scan was skipped)"; fi
echo "built $ZIP"; ls -l $ZIP; unzip -l $ZIP
