#!/bin/sh
# S34R: logic of the vibration engine (needs g++; no ROM, no window)
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/s34r
g++ -std=c++17 -O2 -Wall -Wextra tests/test_haptics.cpp -o build/s34r/test_haptics || { echo "FAIL: build"; exit 1; }
build/s34r/test_haptics
