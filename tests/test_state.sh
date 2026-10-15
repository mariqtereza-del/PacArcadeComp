#!/bin/sh
# S33: save state / rewind / high score test (needs roms/pacman.zip and g++). About 1 minute.
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/s33
g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME tests/test_state.cpp src/board/inputs.cpp src/board/romset.cpp src/board/machine.cpp src/board/bus.cpp src/audio/wsg.cpp \
  build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o build/s33/test_state || { echo "FAIL: build"; exit 1; }
build/s33/test_state roms/pacman.zip
