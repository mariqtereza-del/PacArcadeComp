#!/bin/sh
# S22: builds the window app for this (Linux) machine into build/app/pacman-port. Needs libsdl2-dev. The Windows build is the last block of tools/build_windows.sh.
set -e
cd "$(dirname "$0")/.."
mkdir -p build/app
OBJ=build/app
for f in miniz miniz_tdef miniz_tinfl miniz_zip; do
  [ build/app/$f.o -nt src/third_party/miniz/$f.c ] || gcc -O2 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -c src/third_party/miniz/$f.c -o build/app/$f.o
done
g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) \
  src/app/main.cpp src/app/firstrun.cpp src/video/sprites.cpp src/video/tilemap.cpp src/video/gfx.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp \
  build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o \
  -o build/app/pacman-port $(sdl2-config --libs)
echo "built build/app/pacman-port"
