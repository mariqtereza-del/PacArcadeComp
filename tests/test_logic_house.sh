#!/bin/sh
# T02g: the readable ghost house release (src/logic/ghost_house.h) against the ROM, side by side. See tests/test_logic_house.cpp.
# Usage: sh tests/test_logic_house.sh [FRAMES [RANDOM_CASES]]
# Default: 300,000 frames and 100,000 random cases. The evidence run uses 3,000,000 frames and 1,000,000 random cases.
# Also builds deliberately wrong copies of the header and checks that the test catches every one of them.
cd "$(dirname "$0")/.." || exit 2
[ -f build/rom/prog.bin ] || python3 tools/check_rom.py >/dev/null || exit 2
mkdir -p build/t01
B="g++ -O2 -std=c++17 -Wall -Wextra -Itests/oracle"
SRC="src/board/machine.cpp src/board/bus.cpp"
$B tests/test_logic_house.cpp $SRC -o build/t01/test_logic_house || { echo "FAIL: build"; exit 1; }
build/t01/test_logic_house build/rom/prog.bin "${1:-300000}" "${2:-100000}" || exit 1
H=src/logic/ghost_house.h
fail=0
n=0
wrong() { # name, sed expression
    n=$((n+1))
    sed "$2" "$H" > build/t01/wrong_house_$n.h
    if cmp -s "$H" build/t01/wrong_house_$n.h; then echo "FAIL: wrong version '$1' is identical to the real header (sed did not apply)"; fail=1; return; fi
    $B -DHOUSE_HEADER="\"wrong_house_$n.h\"" -Ibuild/t01 -Itests -Isrc/logic tests/test_logic_house.cpp $SRC -o build/t01/test_wrong_house_$n 2>/dev/null || { echo "FAIL: wrong version '$1' does not build"; fail=1; return; }
    if build/t01/test_wrong_house_$n build/rom/prog.bin 30000 3000 >/dev/null; then echo "FAIL: wrong version '$1' was NOT caught"; fail=1; else echo "caught wrong version: $1"; fi
}
wrong "Pinky common limit 7 -> 8" 's/{7, 17, 32}/{8, 17, 32}/'
wrong "Inky common limit 17 -> 18" 's/{7, 17, 32}/{7, 18, 32}/'
wrong "Clyde common limit 32 -> 33" 's/{7, 17, 32}/{7, 17, 33}/'
wrong "Inky out flag 3 -> 2" 's/{2, 3, 3}/{2, 2, 3}/'
wrong "own counter must exceed the limit" 's/\] < ram\[kOwnDotLimitAddress\[who\]\]/] <= ram[kOwnDotLimitAddress[who]]/'
wrong "Clyde let out in common mode" 's/if (who == 2) {/if (who == 9) {/'
wrong "common mode not ended" 's/ram\[kCommonCounterModeAddress\] = 0;/ram[kCommonCounterModeAddress] = 1;/'
wrong "eaten ghost wait off by one" 's/(n - 4)/(n - 5)/'
wrong "eaten ghost 3 marks wrong ghost" 's/kDeadStateAddress\[n - 1\]\] = 1/kDeadStateAddress[n - 1]] = 2/'
wrong "inactivity counter not reset on a dot" 's/set(kInactivityCounterAddress, 0);$/set(kInactivityCounterAddress, 5);/;0,/set(kInactivityCounterAddress, 5);/!b'
wrong "inactivity limit off by one" 's/counter != word(kInactivityLimitAddress)/counter != word(kInactivityLimitAddress) + 1/'
wrong "inactivity lets out Clyde before Pinky" 's/for (unsigned who = 0; who < 3; ++who)$/for (unsigned who = 3; who-- > 0;)/'
exit $fail
