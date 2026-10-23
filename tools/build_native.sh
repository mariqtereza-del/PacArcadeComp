#!/bin/sh
# Builds and runs the Z80 core test with the native compiler (S14).
set -e
cd "$(dirname "$0")/.."
mkdir -p build
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/oracle/z80_test.cpp -o build/z80_test_native
./build/z80_test_native
