#!/bin/sh
# S35: builds the release folder dist/PacArcadeComp/ and its zip (Windows, 64-bit): the program (with icon and version information), the user README, MODDING.txt, the example mods, the licences. NEVER the ROM.
set -e
SDL=${SDL2_MINGW:-/tmp/sdl2win/SDL2-2.32.10/x86_64-w64-mingw32}
cd "$(dirname "$0")/.."
VER=1.1.0; OUT=dist/PacArcadeComp; ZIP=dist/PacArcadeComp-$VER-win64.zip
sh tools/build_windows.sh >/dev/null 2>&1          # (re)builds the object files for miniz
mkdir -p build/app/win; rm -rf $OUT $ZIP; mkdir -p $OUT
( cd dist/res && x86_64-w64-mingw32-windres app.rc -O coff -o ../../build/app/win/app_res.o )
x86_64-w64-mingw32-g++ -O2 -s -std=c++17 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I"$SDL/include/SDL2" -Dmain=SDL_main \
  src/app/main.cpp src/app/firstrun.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp \
  build/app/win/miniz.o build/app/win/miniz_tdef.o build/app/win/miniz_tinfl.o build/app/win/miniz_zip.o build/app/win/app_res.o \
  -o $OUT/PacArcadeComp.exe \
  -L"$SDL/lib" -lmingw32 -lSDL2main -lSDL2 -mwindows \
  -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32 -lcomdlg32 \
  -static -static-libgcc -static-libstdc++
cp dist/README-PACKAGE.txt $OUT/README.txt; cp THIRD-PARTY-LICENSES.txt $OUT/; cp dist/REPORT-A-PROBLEM.bat $OUT/; cp dist/MODDING-PACKAGE.txt $OUT/MODDING.txt; mkdir -p $OUT/mods; cp mods/*.cfg mods/README.md $OUT/mods/
( cd dist && zip -q -r -X $(basename $ZIP) PacArcadeComp )
echo "built $OUT and $ZIP"; ls -l $OUT $ZIP
