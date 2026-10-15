#!/bin/sh
# Runs every regression test that exists. Add new tests here as steps create them. Stops at the first failure.
cd "$(dirname "$0")/.." || exit 2
set -e
python3 tools/check_rom.py
python3 tests/test_prototype.py
python3 tests/check_notes_cited.py
python3 tests/check_hardware_facts.py
python3 tests/check_disasm.py
python3 tests/check_labels.py
python3 tests/check_ram_map.py
python3 tests/check_gameplay.py
if [ -f tests/test_rom_tables.py ]; then python3 tests/test_rom_tables.py; else echo "(tests/test_rom_tables.py is not in the GitHub copy: skipped)"; fi
sh tests/test_logic_random.sh 100000     # T01a: the readable random numbers equal the ROM routine (the evidence run uses 3,000,000 frames)
sh tests/test_logic_scoring.sh 100000 100000 20000     # T01b: the readable scoring equals the ROM routine (the evidence run uses 3,000,000 frames, 4,000,000 random cases, all 1,000,000 scores)
sh tests/test_logic_timers.sh 100000 20000   # T01f: the readable mode timers equal the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random runs per routine)
sh tests/test_logic_movement.sh 100000 20000   # T01e: the readable movement equals the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random runs per routine)
sh tests/test_logic_ghosts.sh 100000 20000   # T01d: the readable ghost targeting equals the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random cases)
sh tests/test_logic_dots.sh 100000 20000     # T01c: the readable dots and fruit equal the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random cases per piece)
python3 tools/check_docs.py
echo "ALL TESTS PASSED"
