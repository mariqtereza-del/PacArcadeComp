#!/bin/sh
# Builds the Windows test program (single .exe, no extra files needed).
set -e
SDL=${SDL2_MINGW:-/tmp/sdl2win/SDL2-2.32.10/x86_64-w64-mingw32}
cd "$(dirname "$0")/.."
x86_64-w64-mingw32-g++ -O2 -std=c++17 -Wall -I"$SDL/include/SDL2" -Dmain=SDL_main src/hello/main.cpp -o build/pacman-port-test.exe \
  -L"$SDL/lib" -lmingw32 -lSDL2main -lSDL2 -mwindows \
  -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32 -lcomdlg32 \
  -static -static-libgcc -static-libstdc++

# S14: Z80 core test as a Windows console program (compile check; run it on Windows or under wine if available)
x86_64-w64-mingw32-g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/oracle/z80_test.cpp -o build/z80_test_win.exe -static -static-libgcc -static-libstdc++

# S22: the window app (pacman-port.exe). The player puts their own pacman.zip next to it. No ROM is inside the program.
mkdir -p build/app/win
for f in miniz miniz_tdef miniz_tinfl miniz_zip; do
  [ build/app/win/$f.o -nt src/third_party/miniz/$f.c ] || x86_64-w64-mingw32-gcc -O2 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -c src/third_party/miniz/$f.c -o build/app/win/$f.o
done
x86_64-w64-mingw32-g++ -O2 -std=c++17 -Wall -Wextra -DPAC_NO_Z80 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I"$SDL/include/SDL2" -Dmain=SDL_main \
  src/app/main.cpp src/app/firstrun.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp \
  build/app/win/miniz.o build/app/win/miniz_tdef.o build/app/win/miniz_tinfl.o build/app/win/miniz_zip.o \
  -o build/app/pacman-port.exe \
  -L"$SDL/lib" -lmingw32 -lSDL2main -lSDL2 -mwindows \
  -lm -ldinput8 -ldxguid -ldxerr8 -luser32 -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -lshell32 -lsetupapi -lversion -luuid -lcfgmgr32 -lcomdlg32 \
  -static -static-libgcc -static-libstdc++
