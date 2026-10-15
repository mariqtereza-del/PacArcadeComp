#!/bin/sh
# S37: the tests that need NO ROM (so they work in the GitHub copy, where the game's ROM is not included). Stops at the first failure.
# The big suite that needs your own pacman.zip is tests/run_all.sh; BUILDING.md explains both.
cd "$(dirname "$0")/.." || exit 2
mkdir -p build
run() { # run COMMAND...: show the last 2 lines, stop (showing everything) if it fails
  echo "== $*"
  if "$@" > build/rom_free_last.log 2>&1; then tail -2 build/rom_free_last.log; else cat build/rom_free_last.log; echo "FAILED: $*"; exit 1; fi
}
run sh tools/build_native.sh        # the Z80 core builds and runs a known program
run sh tools/build_app.sh           # the window program builds
for t in test_audio_stall test_bindings test_haptics test_fruit_ach test_records; do run sh tests/$t.sh; done
for p in check_hardware_facts check_notes_cited; do run python3 tests/$p.py; done
run python3 tools/check_docs.py
echo "ROM-FREE TESTS PASSED"
