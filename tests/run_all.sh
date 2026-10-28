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
sh tests/test_rom_map.sh      # N01: routine map and coverage recorder
sh tests/test_coverage_a.sh 300000   # N02: coverage player A, deterministic complete games
sh tests/test_coverage_b.sh 300000   # N03: coverage player B, ghosts, fruit, extra life, two players, DIP settings
sh tests/test_coverage_c.sh 300000   # N04: coverage player C, level changes, intermissions, levels 21 and 256
sh tests/test_coverage_d.sh 54000   # N05: service-switch player D
sh tests/test_rom_unreached.sh   # N05: every ROM instruction executed or listed with reason and decision
sh tests/test_timing_study.sh   # N06: timing study (where the interrupt lands inside a task)
sh tests/test_cycle_meter.sh   # N07: cycle meter (exact cycles of a ROM routine)
sh tests/test_lockstep.sh   # N08: the shared lockstep harness (T02a test on it, coverage players as input)
sh tests/test_rom_data_map.sh 20000 20000 20000 20000   # N10: the ROM data map on a short run (the full run is about 5 minutes)
sh tests/test_rom_reader.sh   # N11: the ROM data reader returns exactly the ROM table bytes (needs the N10 report; made with a short run if missing)
sh tests/test_native_helpers.sh 30000 10000 20000   # N12: the native helper routines equal the ROM routines (the evidence run uses 1,000,000 frames)
sh tests/test_native_screen.sh 30000 10000 20000   # N13: the native screen address helpers equal the ROM routines (the evidence run uses 1,000,000 frames)
sh tests/test_native_numbers.sh 30000 10000 20000   # N14: the native number helpers equal the ROM routines (the evidence run uses 1,000,000 frames)
sh tests/test_native_text.sh 30000 10000 20000   # N15: the native text printer equals the ROM routine (the evidence run uses 1,000,000 frames)
sh tests/test_native_status.sh 30000 10000 20000   # N16: the native score, life, credit and fruit pictures equal the ROM routines (the evidence run uses 1,000,000 frames)
sh tests/test_native_sliced.sh 30000 10000 20000   # N17: the native screen clears and the cut fill (the interrupt arrives in the middle) equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_maze.sh 30000 10000 20000     # N18: the native maze drawing, dot placing / collecting and maze colours (cut in the middle by the interrupt) equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_scan.sh 30000 10000 20000     # N19: the native video-RAM clean-up loop (task 13), cut at every instruction, equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_sprites.sh 30000 10000 20000     # N20: the native sprite staging in the interrupt handler equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_pics.sh 30000 10000 20000     # N21: the native sprite pictures (mouth, ghosts, fruit, score and death sprites) equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_clocks.sh 30000 10000 20000     # N22: the native frame counters, clocks and 1UP blink equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_coins.sh 30000 10000 20000      # N23: the native coin inputs, credits and coin counter pulses equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_labels.sh 30000 10000 20000     # N24: the native label writers and bonus sprite positions equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_handler.sh 30000 10000 20000    # N25: the native rest of the interrupt handler equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_events.sh 30000 10000 20000     # N26: the native timed events equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_mode.sh 30000 10000 20000       # N27: the native game-mode dispatchers equal the ROM (the evidence run uses 3,000,000 frames)
sh tests/test_native_attract.sh 30000 10000 2000     # N28: the native attract-mode introduction equals the ROM (the evidence run uses 1,000,000 frames and 40,000 isolated cases per entry)
sh tests/test_native_demo.sh 30000 10000 2000
sh tests/test_native_game.sh 30000 10000 2000
sh tests/test_native_level.sh 30000 10000 2000
sh tests/test_native_frame.sh 30000 10000 2000
sh tests/test_native_meet.sh 30000 10000 2000
sh tests/test_native_eaten.sh 30000 10000 2000
sh tests/test_native_death.sh 30000 10000 2000
sh tests/test_native_clear.sh 30000 10000 2000
sh tests/test_native_fruit.sh 30000 10000 2000
sh tests/test_native_over.sh 30000 10000 2000
sh tests/test_native_players.sh 30000 10000
sh tests/test_native_inter1.sh 30000 10000 2000
sh tests/test_native_inter2.sh 30000 10000 2000
sh tests/test_native_inter3.sh 30000 10000 2000
sh tests/test_native_snd.sh 30000 10000 2000   # N43: the native sound effects equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_tune.sh 30000 10000 2000   # N44: the native tune driver equals the ROM (the evidence run uses 3,000,000 frames)
sh tests/test_native_sched.sh 30000 10000 2000   # N45: the native scheduling glue equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45a.sh 30000 10000 2000   # N45a: the native per-frame update part 1 equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45b.sh 30000 10000 2000   # N45b: the native per-frame update part 2 equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45c.sh 30000 10000 2000   # N45c: the native siren selection and ghost turn-around calls equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45d.sh 30000 10000 2000   # N45d: the native ghost state tables equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45e.sh 30000 10000 2000   # N45e: the native ghost movement helpers equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45f.sh 30000 10000 2000   # N45f: the native ghost speed routines equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45g.sh 30000 10000 2000   # N45g: the native Pac-Man mover and board start-up equal the ROM (the evidence run uses 3,000,000 frames at this milestone)
sh tests/test_native_f45h.sh 30000 10000 2000   # N45h: the native ghost targeting and direction equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45i.sh 30000 10000 2000   # N45i: the native direction and demo-game tasks equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45j.sh 30000 10000 2000   # N45j: the native task-table entries equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f45k.sh 30000 10000 2000   # N45k: the native DIP-switch reader and text printer equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f46.sh 30000 10000 2000   # N46: the native main loop and fill/table routines equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f47.sh 30000 10000 2000   # N47: the native reset and boot equal the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f47a.sh 30000 10000 2000   # N47a: the native power-on self test equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_native_f47b.sh 30000 10000 2000   # N47b: the native service-switch test screen equals the ROM (the evidence run uses 1,000,000 frames)
sh tests/test_dip_live.sh   # S34D: new DIP switches put into the running machine equal a power-on with them
sh tests/test_reset_defaults.sh   # S34D/S34E: the two RESET TO DEFAULTS items and the live DIP change
sh tests/test_achievement_settings.sh   # S34E: achievements follow the game settings, not the look
sh tests/test_no_z80_header.sh
sh tests/test_padbuttons.sh   # N49b: (was not in run_all; the Purist refusal text changed in S34D)
sh tests/test_no_z80_symbols.sh   # N49b: the window program has no Z80 core
sh tests/test_oracle_location.sh   # N50: the Z80 reference is in tests/oracle only
sh tests/test_names.sh   # N51: the named actors block equals the memory; no raw 4Dxx addresses in the code
sh tests/test_names_output.sh   # N53: the named output (screen, sprites, sound, latches) equals the memory; lockstep compares through the names
sh tests/test_native_machine.sh 100000 20000   # N49a: the machine without the Z80 equals the Z80 machine frame by frame (evidence run: 1,000,000 frames)   # N49
sh tests/test_native_f48.sh 30000 10000 2000   # N48: the last routines (0010, 1AD9, 276C, 2B7E) equal the ROM
sh tests/test_native_ops.sh 300   # N49c: every instruction of the native list equals the Z80 on random states
sh tests/test_no_rom_code.sh 100000 20000 300   # N48: zero ROM instructions executed (the evidence run: 3,000,000 scripted frames)
sh tests/test_fps.sh   # 2.1.0: frame rate (sprites between ticks), the new setting; hash logs identical at every frame rate
sh tests/test_cheat_invincible.sh   # the INVINCIBLE test cheat on the real ROM, original and readable rules
python3 tools/check_docs.py
python3 tools/check_links.py   # N54: every link and cited path in the documents exists
echo "ALL TESTS PASSED"
