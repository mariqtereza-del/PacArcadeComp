#!/bin/sh
# S34A: logic of the bindings screen (needs g++ and libsdl2-dev; no ROM, no window)
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/s34
g++ -std=c++17 -O2 -Wall -Wextra $(sdl2-config --cflags) tests/test_bindings.cpp -o build/s34/test_bindings $(sdl2-config --libs) || { echo "FAIL: build"; exit 1; }
SDL_VIDEODRIVER=dummy build/s34/test_bindings
