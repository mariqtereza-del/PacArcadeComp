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
sh tests/test_logic_house.sh 100000 20000   # T02g: the readable ghost house release equals the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random runs per routine)
sh tests/test_logic_movement.sh 100000 20000   # T01e: the readable movement equals the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random runs per routine)
sh tests/test_logic_ghosts.sh 100000 20000   # T01d: the readable ghost targeting equals the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random cases)
sh tests/test_logic_dots.sh 100000 20000     # T01c: the readable dots and fruit equal the ROM (the evidence run uses 3,000,000 frames and 1,000,000 random cases per piece)
sh tests/test_rules_random.sh 100000   # T02a: the game with the readable random numbers equals the original (the evidence run uses 3,000,000 frames)
sh tests/test_rules_scoring.sh 100000 100000   # T02b: the game with the readable scoring and extra life equals the original (the evidence run uses 3,000,000 frames and 2,000,000 isolated calls)
sh tests/test_rules_dots.sh 100000 100000      # T02c: the game with the readable dots, power pill and fruit equals the original (the evidence run uses 3,000,000 frames and 1,000,000 isolated calls per routine)
sh tests/test_rules_ghosts.sh 100000 100000    # T02d: the game with the readable ghost targeting and direction chooser equals the original (the evidence run uses 3,000,000 frames and 1,000,000 isolated calls per routine)
sh tests/test_rules_movement.sh 100000 100000    # T02e: the game with the readable movement of Pac-Man and the ghosts equals the original (the evidence run uses 3,000,000 frames and 1,000,000 isolated calls per routine)
sh tests/test_rules_timers.sh 100000 100000    # T02f: the game with the readable phase timer, frightened timer and level start equals the original (the evidence run uses 3,000,000 frames and 1,000,000 isolated calls per routine)
sh tests/test_rules_house.sh 100000 100000    # T02h: the game with the readable ghost house release equals the original (the evidence run uses 3,000,000 frames and 1,000,000 isolated calls per routine)
sh tests/test_rules_all.sh 100000    # T02f: ALL readable rules together equal the original (the evidence run uses 3,000,000 frames, with and without garbage)
sh tests/test_rules_file.sh 20000    # T02i: the rules file and the RULES setting (reader, numbers, a changed game, MODDED, Purist ignores the file)
sh tests/test_mods.sh 20000    # T02j: the example mods load and change what they say; the worked Pinky example; every link and path in the guides exists
python3 tools/check_docs.py
echo "ALL TESTS PASSED"
