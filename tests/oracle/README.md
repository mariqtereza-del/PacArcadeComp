# tests/oracle: the Z80 core, kept as a test reference only

`z80.h` (Ivan Kosarev's MIT-licensed Z80 emulator, see CREDITS.md and LICENSE-z80) is **not part of the program you play**. Since step N49b the window program is built with
`-DPAC_NO_Z80` and contains no Z80 core (`tests/test_no_z80_symbols.sh` checks the binary, `tools/package.sh` and `tools/package_test.sh` check the Windows .exe).

It stays in the repository for the tests: the Z80 build of `Machine` (src/board/machine.h, the code outside `#ifdef PAC_NO_Z80`) runs the original ROM, and
every native routine, the whole native machine and every replay hash are proved against it (`tests/test_native_machine.sh`, the `tests/test_native_*.sh` family).
The ROM itself is never part of the repository or the release; the tests read it from the player's own pacman.zip.
