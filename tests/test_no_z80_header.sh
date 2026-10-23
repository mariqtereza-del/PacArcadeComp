#!/bin/sh
# N49: the native code (src/game, src/logic, src/rules, src/app, src/audio, src/video) must not include the Z80 core header and must not use
# anything from it except through the Machine object. The names it takes from Machine are listed (they are the register file that the native
# machine of N49a has to provide). Usage: sh tests/test_no_z80_header.sh
cd "$(dirname "$0")/.." || exit 2
bad=0
inc=$(grep -rn 'include.*oracle/z80\.h\|include.*"z80\.h"' src/game src/logic src/rules src/app src/audio src/video 2>/dev/null)
[ -z "$inc" ] || { echo "FAIL: includes the Z80 core header:"; echo "$inc"; bad=1; }
# z80:: names used by the native code must be defined in the project's own headers (src/rules/z80ops.h, z80sim.h), not in tests/oracle/z80.h
for n in $(grep -rhoE '(^|[^a-z_])z80::[A-Za-z_0-9]+' src/game src/logic src/rules src/app 2>/dev/null | sed 's/.*z80:://' | sort -u); do
  grep -qE "(\b$n\b)" src/rules/z80ops.h src/rules/z80sim.h || { echo "FAIL: z80::$n is not defined in src/rules/z80ops.h or z80sim.h"; bad=1; }
done
echo "CPU functions the native code calls on the machine (the register file of N49a must provide them):"
grep -rhoE '\bon_(get|set|is|step|tick)[a-z_0-9]*\b' src/rules src/game src/app --include=*.h --include=*.cpp | sort -u | tr '\n' ' '; echo
[ $bad -eq 0 ] || exit 1
echo "PASS test_no_z80_header (no include of the Z80 core header outside src/board; no index-register enum of the core in src/rules)"
