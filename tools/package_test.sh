#!/bin/sh
# T02i: builds a small Windows test folder for the user (dist/PacArcadeComp-<LABEL>-test-win64.zip): PacArcadeComp.exe (this source), the example mod, and a plain-words README.
# NEVER the ROM. Usage: sh tools/package_test.sh T02i
set -e
SDL=${SDL2_MINGW:-/tmp/sdl2win/SDL2-2.32.10/x86_64-w64-mingw32}
cd "$(dirname "$0")/.."
L=${1:?give a label like T02i}
OUT=dist/PacArcadeComp-$L-test; ZIP=dist/PacArcadeComp-$L-test-win64.zip
sh tools/build_windows.sh >/dev/null 2>&1
mkdir -p build/app/win; rm -rf $OUT $ZIP; mkdir -p $OUT
( cd dist/res && x86_64-w64-mingw32-windres app.rc -O coff -o ../../build/app/win/app_res.o )
x86_64-w64-mingw32-g++ -O2 -s -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I"$SDL/include/SDL2" -Dmain=SDL_main \
  src/app/main.cpp src/app/firstrun.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp \
  build/app/win/miniz.o build/app/win/miniz_tdef.o build/app/win/miniz_tinfl.o build/app/win/miniz_zip.o build/app/win/app_res.o \
  -o $OUT/PacArcadeComp.exe \
  -L"$SDL/lib" -lmingw32 -lSDL2main -lSDL2 -mwindows \
  -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32 -lcomdlg32 \
  -static -static-libgcc -static-libstdc++
if [ -d mods ]; then cp -r mods $OUT/mods; cp MODDING.md $OUT/MODDING.md; else cp examples/rules/slow_ghosts.cfg $OUT/slow_ghosts.cfg; fi
cp dist/REPORT-A-PROBLEM.bat $OUT/
[ -f dist/README-TEST-$L.txt ] && cp dist/README-TEST-$L.txt $OUT/README-TEST.txt
( cd dist && zip -q -r -X $(basename $ZIP) PacArcadeComp-$L-test )
echo "built $ZIP"; ls -l $ZIP
