# Architecture: how the program is put together

This page is a map of the whole repository: what each folder and file is for, and how the pieces fit. It is written for someone who has never seen the code. Every source file also starts with a one-line comment saying what it is.

## The big picture

The game is **native C++**. The program contains no processor emulator: the arcade program's logic has been rewritten as C++ (`src/game/`, `src/rules/`, `src/logic/`) and was proved, frame by frame and cycle by cycle, to do exactly what the original program does on a Z80. The only things taken from your ROM at run time are data tables (maze, text, level records, sound and graphics data).

```
 your pacman.zip ──► romset ──► src/data (table reader: the ONLY code that reads ROM bytes)
   (never copied)                    │
                                     ▼
   Inputs (keys, DIP switches) ──► Machine = native game (src/game, src/rules) + Bus (video RAM, work RAM, sound registers …)
                                     │        one frame at a time (60.6 per second), exact cycle count per frame
                                     │
                       named output state (src/game/output.h): screen, sprites, sound registers, latches
              ┌──────────────────────┼─────────────────────┐
              ▼                      ▼                     ▼
        video (tiles,          audio (sound chip       app (window, menu, pads,
        sprites → picture)      → sound card)           save states, effects …)
```

1. **The ROM** (`src/board/romset.*`, `src/data/`): the player's own `pacman.zip` is opened, every file is checked (name, size, checksum), and the bytes are kept in memory. `src/data/rom_data.*` is the only code that reads them, and only as tables (see "What the ROM is still needed for" below). Nothing is written back or copied.
2. **The board** (`src/board/`): `Bus` holds the machine's memory (work RAM, video RAM, colour RAM, sprite registers, sound registers, output latches). `Inputs` holds the joystick, coin and start buttons and the DIP switches. `Machine` runs one frame's worth of cycles of the native game, then the vertical-blank interrupt work, exactly like the arcade hardware. `cpuregs.h` is the small register file the native code keeps so timing and interrupts stay identical to the original.
3. **The game** (`src/game/`): the arcade program's routines written as C++ (screen, text, maze, sprites, sound requests, timed events, coins, attract mode, demo, level start, play, intermissions, boot). Its state has names: `state.h` (work RAM: actors, flow, scores, settings, system, sprite staging) and `output.h` (what goes to the screen, sprites, sound chip and latches). See "The native engine" below.
4. **The processor emulator is a test tool** (`tests/oracle/z80.h`): a third-party, unmodified, single-file Z80 emulator (MIT licence, credit in `tests/oracle/CREDITS.md`). It runs the original program in the tests so the native game can be compared with it. It is **not** in the program you play (`tests/test_no_z80_symbols.sh` proves the built program has no trace of it).
5. **Video** (`src/video/`): `gfx` decodes the graphics ROMs and colour PROMs into pixel arrays; `tilemap` draws the 28x36 tile layer; `sprites` draws the 8 hardware sprites. They are pure functions of the machine's output memory: they read, they never change the game.
6. **Audio** (`src/audio/`): `wsg` is the 3-voice Namco sound generator (it turns the sound registers into samples); `audio_out` is the ring buffer and resampler that hands them to the sound card.
7. **The app** (`src/app/`): everything a player sees around the machine: the window, menus, controls, pads, save states, effects, statistics. It only talks to the machine through the same inputs a player has and by *reading* its memory.

**The rule that keeps the game accurate:** features live outside the machine. Overlay, statistics, achievements, lightbar and vibration only read memory after each frame. Save states, rewind and replays copy or restore the machine as a whole. Purist mode switches all of them off, and a test proves the machine then runs bit-for-bit like the reference.

## The native engine

How the original program became C++ (steps N01 to N53; each has a step file in `docs/steps/` and a result file in `docs/evidence/`):

| Part of the game | Where it lives | Step / proof |
|---|---|---|
| Which routine does what (the map) | `docs/notes/rom_routines.md` | N01 |
| Fill, copy, table reading; screen addresses; number maths; text printing | `src/game/helpers.h`, `screen.h`, `numbers.h`, `text.h` | N12 to N15 |
| Score, credits, lives and fruit pictures on screen | `src/game/status.h` | N16 |
| Routines the interrupt may cut in the middle | `src/game/sliced.h` | N17 |
| Maze drawing, dot placement, screen scans | `src/game/maze.h`, `scan.h` | N18, N19 |
| Sprite staging and sprite pictures | `src/game/sprites.h`, `sprite_pics.h` | N20, N21 |
| Counters, clocks, coins, labels, the interrupt handler's copies, timed events | `src/game/clocks.h`, `coins.h`, `labels.h`, `handler.h`, `events.h` | N22 to N26 |
| Attract mode, demo, credit screens, level start, the play frame, ghost and Pac-Man meetings, death, level clear, fruit, game over, two players, the three intermissions, sound requests, scheduler, frame driver, boot and self test | `src/game/attract.h` running the list in `src/game/attract_prog.h` | N27 to N47b |
| The readable rules (random numbers, scoring, dots, ghost targeting, movement, timers, ghost house) | `src/logic/`, wired in by `src/rules/` | T01, T02 |
| Names for the work RAM | `src/game/state.h` (made by `tools/gen_names.py`) | N51, N52 |
| Names for the screen, sprites, sound registers and latches | `src/game/output.h` | N53 |

**Honest note on `attract_prog.h`.** Most of the play logic is not hand-written readable code. `tools/gen_attract.py` turned the original routines into an instruction list (C++ data: operation names and numbers, no ROM bytes) that `src/game/attract.h` runs with plain C++ (no processor emulator). It is exact (proved below) but it is a translation, not yet readable rules. The readable rules in `src/logic/` cover the parts listed in `src/logic/README.md`. Turning more of the list into readable code is future work.

**How it was proved.** Every step compares the native code with the original program running on the Z80 reference:
- *Lockstep* (`tests/lockstep.h`, N08): after every frame the work RAM, screen, sprites, sound registers, latches, I/O writes and the cycle count of the frame must be identical.
- *Coverage players* (N02 to N04): scripted games that reach every part of the game (attract mode, play, deaths, all ghosts, fruit, level changes, intermissions, level 21 and 256, service tests).
- *Native machine against Z80 machine* (`tests/test_native_machine.sh`): frame fingerprints identical over 171,273 frames.
- *Every instruction of the list against the Z80 on random states* (`tests/test_native_ops.sh`).
- *Zero ROM instructions* (`tests/test_no_rom_code.sh`, N48): over 3.1 million frames no ROM instruction runs outside the native code.
- *No Z80 in the program* (`tests/test_no_z80_symbols.sh`, N49), *replay and Purist hashes* (`tests/test_replay.sh`, `tests/test_purist.sh`), and *names equal the memory* (`tests/test_names.sh`, `tests/test_names_output.sh`).
Each test also checks that a deliberately wrong version (one cycle too slow, a shifted view) is caught.

## What the ROM is still needed for

The program ROM's **instructions** are no longer used. You still need your own `pacman.zip` because the game reads these as data at run time (`src/data/rom_data.*`, list in `docs/notes/rom_data_tables.md`): the maze map and dot positions, the text strings, the level records and speed tables, the fruit and points tables, direction tables, the sound tunes and effect tables, and the self-test tables. The picture side needs the graphics ROMs (tiles and sprites), the colour PROMs, and the sound-wave PROM. Nothing from the ROM is copied into this repository; `tools/export_github.sh` scans for it and refuses to finish if it finds any.

### `src/data/` the table reader
| File | What it is |
|---|---|
| `rom_data.h/.cpp` | The only code that reads the player's ROM image as data: addresses and sizes of the tables, no ROM bytes. |

### `src/game/` the native game
One file per part of the table above, plus `attract_prog.h` (generated), `state.h` (generated names for work RAM) and `output.h` (names for screen, sprites, sound registers, latches). Every file starts with a comment naming the ROM routines it replaces. Nothing here contains ROM bytes.

### `tests/oracle/` the reference processor (tests only)
| File | What it is |
|---|---|
| `z80.h` | Z80 emulator by Ivan Kosarev (MIT). Third party, unmodified. |
| `LICENSE-z80`, `CREDITS.md`, `README.md` | Its licence, credit and a note that it is a test tool. Must stay with the header. |
| `z80_test.cpp`, `boot_test.cpp` | Run a tiny known Z80 program, and boot the ROM on the core (proves the reference works on your compiler). |

### `src/board/` the arcade board
| File | What it is |
|---|---|
| `romset.h/.cpp` | Finds, opens and verifies the player's ROM zip (uses miniz). Friendly error text comes from `app/firstrun`. |
| `bus.h/.cpp` | Memory map, mirrors and I/O ports of the board. |
| `inputs.h/.cpp` | Input ports and DIP switches (all bits read 0 when active, like the real hardware). |
| `cpuregs.h` | The small register file the native game keeps (no processor emulator). |
| `machine.h/.cpp` | The native machine + bus + frame loop with the vertical-blank interrupt. Also saves and restores the whole machine state. Built with `-DPAC_NO_Z80` for the program; the tests build the version with the reference Z80 next to it. |

### `src/video/`
| File | What it is |
|---|---|
| `gfx.h/.cpp` | Decodes the graphics ROMs and colour PROMs into plain pixel arrays. |
| `tilemap.cpp` | Draws the tile layer (maze, text, dots) into a 224 x 288 picture. |
| `sprites.cpp` | Draws the 8 hardware sprites (Pac-Man, ghosts, fruit, scores) on top. |

### `src/audio/`
| File | What it is |
|---|---|
| `wsg.h/.cpp` | The Namco WSG sound chip: registers in, samples out (96 kHz, once per frame). |
| `audio_out.h/.cpp` | Ring buffer and resampler between the chip and the sound card callback. |

### `src/app/` the window and everything around the game
| File | What it is |
|---|---|
| `main.cpp` | The program: command-line options, the SDL window, the main loop, drawing, wiring of every feature. Includes `menu.cpp` and `replay.cpp` (they are compiled as part of it). |
| `menu.h/.cpp` | The Esc menu, key-help panel, settings file (`settings.cfg`) and the rules of Purist mode. It only returns commands; `main.cpp` carries them out. |
| `controls.h/.cpp` | Key and pad bindings, `controls.cfg`, remap dialogue, one-button start. |
| `display.h/.cpp` | Where the picture goes in the window (whole-number or fill scaling), smoothed pacing. No emulation here. |
| `overlay.h/.cpp` | The F3 debug overlay (targets of the ghosts etc.). Reads memory only. |
| `state.h/.cpp` | Save states (8 slots), rewind, the saved high score. |
| `replay.h/.cpp` | Records and replays inputs; the per-frame state hash used by the tests. |
| `firstrun.h/.cpp` | Finds the ROM on the first run and shows friendly messages and the "Choose file" dialog. |
| `events.h` | The game event reader: looks at a few RAM values after each frame and turns changes into events (dot eaten, ghost eaten, level cleared…). |
| `assist.h` | The one shared rule for "assisted" games (save states, rewind, etc.) that do not count for records. |
| `stats.h`, `records.h`, `achievements.h` | Per-run statistics, personal bests per level, achievements. All built on `events.h`. |
| `crt.h` | CRT look: shadow mask, glow, curvature, presets. |
| `surround.h` | The picture around the game: black, cabinet look, ambient glow, your own `surround.png`. |
| `haptics.h`, `lightbar.h`, `lastdevice.h` | Pad vibration, light bar colours, and "which device was used last" (to show keyboard or pad hints). |

### `src/logic/` the readable game rules ("Track B")
The game's rules rewritten as small readable C++ pieces, each proved identical to the ROM routine it copies. **Used by the game only with RULES READABLE** (Esc > GAME OPTIONS > RULES, or `--rules readable`; a `rules.cfg` with changed values switches it on): the adapters in `src/rules/` run these functions in place of 24 native routines. With RULES ORIGINAL (the standard) and in Purist mode the native game runs with the original numbers. See `src/logic/README.md`, [MODDING.md](MODDING.md) and `src/rules/` below.

| File | Content |
|---|---|
| `random_number.h` | The random number generator. |
| `scoring.h`, `work_ram.h` | Scoring, extra life, high score; a view of work RAM using the original's addresses. |
| `dots_and_fruit.h` | Eating dots and energizers, ghost-house counters, fruit. |
| `ghost_targeting.h` | Each ghost's target tile and direction choice. |
| `movement.h` | Pac-Man and ghost movement, speeds, tunnel, Cruise Elroy. |
| `mode_timers.h` | Scatter/chase and frightened timers, per-level settings. |
| `ghost_house.h` | When each ghost leaves the ghost house (dot counters, the common counter after a death), the eaten ghost's wait, the inactivity timer. |

### `src/rules/` the bridge between the machine and `src/logic/`
| File | Content |
|---|---|
| `rules.h` | The adapters: one hook per replaced arcade routine (24). A hook runs the readable function, then leaves memory, registers, flags and the cycle count exactly as the arcade routine would. `table()` is the list `Machine::rules` points to (empty with RULES ORIGINAL). |
| `z80sim.h`, `z80ops.h` | The timing and register model used by the adapters (a transcription of the instruction path of each replaced routine, with the original's flags and cycle counts). |
| `rules_file.h` | The `rules.cfg` reader/writer (24 keys), and the code that puts the file's numbers into the level set-up and the scoring. |

The example mods are in `mods/`; their tests in `tests/test_mods.sh`.

### `src/third_party/` and `src/hello/`
`miniz/` is the zip reader (MIT, compiled into the program). `hello/main.cpp` is the very first toolchain test (a window that proves a Windows exe can be built).

### `tests/` the safety net
About 100 files. Most come as a `.sh` script (builds and runs one test) plus a `.cpp` (the check itself). Groups:
- **Machine proofs** (need your ROM): `test_cpu_parity`, `test_integration`, `test_replay`, `test_purist`, `test_scenarios`, `test_behaviour_*` and the `test_logic_*` proofs. Replay-based tests compare the state of the whole machine frame by frame with recorded hashes in `tests/data/`.
- **Front-end tests** (mostly no ROM needed): menus, bindings, mouse, CRT, glow, curve, surround, ultrawide, statistics, achievements, vibration, lightbar.
- **Documentation checks**: `tools/check_docs.py`, `tests/check_*.py`.
- `tests/run_all.sh` runs everything (needs the ROM, about 45 minutes). `tests/run_rom_free.sh` runs only what works without a ROM (see [BUILDING.md](BUILDING.md)).
- Shared helpers: `logic_probe.h` (the side-by-side proof method), `scenario.h` (set-ups that put the machine in a given game situation).

### `tools/`
| File | What it is |
|---|---|
| `build_app.sh` | Builds the program on Linux (`build/app/pacman-port`). |
| `build_windows.sh`, `package.sh` | Cross-compile the Windows exe and build the release zip (on Linux with MinGW). |
| `build_native.sh` | Builds and runs the reference Z80 test program. |
| `gen_attract.py`, `gen_names.py` | Make `src/game/attract_prog.h` and `src/game/state.h` (the first needs the private disassembly, so it is not run from the GitHub copy). |
| `export_github.sh` | Makes the clean copy for GitHub and refuses to finish if any ROM bytes are in it. |
| `check_docs.py`, `check_rom.py` | Documentation check; ROM identification (CRC32). |
| `plan_data.py`, `make_plan.py`, `plan_template.html`, `checkpoint.sh` | The project's plan machinery (status table, plan page, backup zip). |
| `analysis/` | The Python reference prototype used to cross-check the C++ (needs the pip package `z80`). |
| `trace_disasm.py`, `frametime_test.bat` | Disassembly tracing helper; a Windows frame-time test. |

### `docs/`
| Folder | Content |
|---|---|
| `docs/notes/` | What we learned: hardware facts, boot flow, RAM map, routines, table comparison, settings table. |
| `docs/steps/` | One file per project step (objective, what may be touched, how to verify). |
| `docs/evidence/` | The test results of each step (text). |
| `docs/disasm/` | Labels and code/data block lists of the game program. The disassembly itself is **not** published (it is made from the ROM). |
| `docs/images/` | Screenshots used in the README (our own menus only). |
| `docs/PLAN.md`, `Pac-Man_PC_Port_Plan.html` | The plan, as text and as a page. |

### Top level
`README.md` (front page), `BUILDING.md`, `MODDING.md`, `ARCHITECTURE.md` (this file), `CONTRIBUTING.md`, `CHANGELOG.md`, `MACHINE_NOTES.md` (how the arcade machine works), `PROJECT_STATUS.md` (status of every step), `LICENSE`, `THIRD-PARTY-LICENSES.txt`, `.clang-format` (code layout), `.gitignore` (keeps ROMs and build output out of the repository).

## Run-time files
The program keeps `settings.cfg`, `controls.cfg`, records, achievements, save states and the high score in `%APPDATA%\PacmanPort` on Windows. It never writes to the ROM file.

## Where to read next
- How the machine works: [MACHINE_NOTES.md](MACHINE_NOTES.md), then `docs/notes/`.
- How a feature was proved: `docs/evidence/<step>_*.txt` (the step names are in [PROJECT_STATUS.md](PROJECT_STATUS.md)).
