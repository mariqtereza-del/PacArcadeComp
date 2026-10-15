#!/bin/sh
# S32: builds and runs tests/test_controls.cpp (needs the ROM in roms/pacman.zip and libsdl2-dev).
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/s32
g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) tests/test_controls.cpp src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp \
  build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o build/s32/test_controls $(sdl2-config --libs) || { echo "FAIL: build"; exit 1; }
build/s32/test_controls roms/pacman.zip
