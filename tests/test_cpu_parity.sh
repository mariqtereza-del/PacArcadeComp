#!/bin/sh
# S15: C++ z80 core versus the Python prototype core, instruction by instruction, from reset.
# Usage: sh tests/test_cpu_parity.sh [COUNT]   (default 20000; WIN=1 also runs the mingw build under wine if wine exists)
cd "$(dirname "$0")/.." || exit 2
N=${1:-20000}
T=$(mktemp -d)
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || exit 2
g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle tests/oracle/boot_test.cpp -o "$T/boot_test" || { echo "FAIL: build"; exit 1; }
"$T/boot_test" build/rom/prog.bin "$N" > "$T/cpp.txt" || { echo "FAIL: C++ run"; exit 1; }
python3 tools/analysis/oracle_trace.py "$N" > "$T/py.txt" || { echo "FAIL: Python run"; exit 1; }
compare() {  # $1 = file to compare with the Python log, $2 = label
  if cmp -s "$T/py.txt" "$1"; then return 0; fi
  L=$(cmp "$T/py.txt" "$1" 2>&1 | sed -n 's/.*line \([0-9]*\).*/\1/p')
  echo "FAIL ($2): first difference at instruction $L"
  echo "  python: $(sed -n "${L}p" "$T/py.txt")"
  echo "  c++   : $(sed -n "${L}p" "$1")"
  echo "  (columns: PC AF BC DE HL IX IY SP I R IFF1 IFF2 AF' BC' DE' HL')"
  exit 1
}
compare "$T/cpp.txt" "native C++"
# the log must be a real boot: the halt-and-interrupt path must have run (PC 3000 = self-test entry reached after the interrupt)
if [ "$N" -ge 7000 ] && ! grep -q '^3000 ' "$T/py.txt"; then echo "FAIL: trace never reached the interrupt handler (3000)"; exit 1; fi
LINES=$(wc -l < "$T/py.txt"); PCS=$(awk '{print $1}' "$T/py.txt" | sort -u | wc -l)
EXTRA=""
if [ -n "$WIN" ] && command -v wine >/dev/null && [ -f build/boot_test_win.exe ]; then
  WINEDEBUG=-all wine build/boot_test_win.exe build/rom/prog.bin "$N" 2>/dev/null | tr -d '\r' > "$T/win.txt"
  compare "$T/win.txt" "mingw under wine"; EXTRA=" and mingw build under wine"
fi
echo "PASS: $LINES instructions identical (C++ core vs Python prototype$EXTRA), $PCS distinct PCs, interrupt path exercised"
rm -rf "$T"
