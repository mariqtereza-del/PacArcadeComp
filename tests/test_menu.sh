#!/bin/sh
# S34: menu logic, Purist locks, settings file, drawing (needs roms/pacman.zip and g++)
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/s34
g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) tests/test_menu.cpp src/board/romset.cpp src/video/gfx.cpp \
  build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o build/s34/test_menu $(sdl2-config --libs) || { echo "FAIL: build"; exit 1; }
build/s34/test_menu roms/pacman.zip
