#!/bin/sh
# N50: the Z80 reference (the emulator core) lives in tests/oracle only. The program source (src/) holds no copy of it, the program build does not reach into tests/,
# and the only src/ file that names the folder is the test-reference branch of src/board/machine.h (compiled only when -DPAC_NO_Z80 is NOT given).
cd "$(dirname "$0")/.." || exit 2
bad=0
[ -d src/oracle ] && { echo "FAIL: src/oracle still exists"; bad=1; }
[ -f tests/oracle/z80.h ] && [ -f tests/oracle/LICENSE-z80 ] || { echo "FAIL: tests/oracle is incomplete"; bad=1; }
if find src -type f | xargs grep -l "z80_cpu" 2>/dev/null | grep -v "^src/board/machine.h$" | grep -q .; then echo "FAIL: Z80 core code under src/"; bad=1; fi
n=$(grep -rn "tests/oracle\|tests/" src --include=*.h --include=*.cpp | grep -v "^src/board/machine.h:" | grep -v "^\S*:[0-9]*:\s*//" | wc -l)
[ "$n" -eq 0 ] || { echo "FAIL: $n lines under src/ reach into tests/"; grep -rn "tests/oracle\|tests/" src --include=*.h --include=*.cpp | grep -v "^src/board/machine.h:" | grep -v ":\s*//" | head -3; bad=1; }
# the program is built without the folder on its include path
for f in tools/build_app.sh tools/build_windows.sh tools/package.sh tools/package_test.sh; do grep "src/app/main.cpp" $f | grep -q "Itests/oracle" && { echo "FAIL: $f puts tests/oracle on the program's include path"; bad=1; }; done
[ $bad -eq 0 ] || { echo "FAIL test_oracle_location"; exit 1; }
echo "PASS test_oracle_location (the Z80 reference is in tests/oracle only)"
