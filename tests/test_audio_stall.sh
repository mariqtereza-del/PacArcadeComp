#!/bin/sh
# S32 audio bug test (see tests/test_audio_stall.cpp). No ROM or sound card needed.
cd "$(dirname "$0")/.." || exit 2
mkdir -p build/s32
g++ -std=c++17 -O2 -Wall -Wextra tests/test_audio_stall.cpp -o build/s32/test_audio_stall || { echo "FAIL: build"; exit 1; }
build/s32/test_audio_stall
