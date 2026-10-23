#!/bin/sh
# N49b: the window program (build/app/pacman-port, built with -DPAC_NO_Z80 by tools/build_app.sh) contains no Z80 core. Checks (1) the build scripts pass -DPAC_NO_Z80 and
# do not compile tests/oracle, (2) the preprocessed program source has no Z80 core class, (3) the binary has no Z80 core symbol (a control: the test programs that
# keep the core as the reference DO have them, so the check can see them).
cd "$(dirname "$0")/.." || exit 2
bad=0
for f in tools/build_app.sh tools/build_windows.sh tools/package.sh tools/package_test.sh; do
  # the lines that compile src/app/main.cpp must carry the macro and must not name tests/oracle
  for l in $(grep -n "src/app/main.cpp" $f | cut -d: -f1); do
    p=$((l-1)); [ $p -lt 1 ] && p=1
    t=$(sed -n "${p},${l}p" $f)
    echo "$t" | grep -q "PAC_NO_Z80" || { echo "FAIL: $f line $l does not pass -DPAC_NO_Z80"; bad=1; }
    echo "$t" | grep -q "tests/oracle" && { echo "FAIL: $f line $l names tests/oracle"; bad=1; }
  done
done
n_src=$(g++ -E -std=c++17 -DPAC_NO_Z80 -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) -Isrc src/app/main.cpp 2>/dev/null | grep -c "z80_cpu\|z80_executor\|z80_decoder\|z80_state")
[ -f build/app/pacman-port ] || sh tools/build_app.sh >/dev/null 2>&1
n_sym=$(nm -C build/app/pacman-port | grep -c "z80_cpu\|z80_executor\|z80_decoder\|z80_state")
n_str=$(strings build/app/pacman-port | grep -c "z80_cpu\|z80_executor")
mkdir -p build/n49a
[ -f build/n49a/native_z80 ] || g++ -O2 -std=c++17 -Itests/oracle -Isrc tests/test_native_machine.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o build/n49a/native_z80 >/dev/null 2>&1   # N50: needs no ROM (the control is only built, not run)
n_ref=$(nm -C build/n49a/native_z80 | grep -c "z80_cpu\|z80_executor\|z80_decoder\|z80_state")
echo "Z80 core in the window program: $n_src lines after the preprocessor, $n_sym symbols, $n_str names in the binary (control: the Z80 test program has $n_ref symbols)"
[ "$n_src" -eq 0 ] && [ "$n_sym" -eq 0 ] && [ "$n_str" -eq 0 ] && [ "$n_ref" -gt 0 ] || bad=1
[ $bad -eq 0 ] || { echo "FAIL test_no_z80_symbols"; exit 1; }
echo "PASS test_no_z80_symbols (the window program has no Z80 core)"
