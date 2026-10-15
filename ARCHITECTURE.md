# Architecture: how the program is put together

This page is a map of the whole repository: what each folder and file is for, and how the pieces fit. It is written for someone who has never seen the code. Every source file also starts with a one-line comment saying what it is.

## The big picture

```
 your pacman.zip ──► romset ──► Bus (memory map, I/O ports) ◄──► Z80 core
   (never copied)                   ▲   ▲                          │
                                    │   └── Inputs (keys, DIP switches)
                          Machine = Z80 + Bus + one frame at a time (60.6 per second)
                                    │
              ┌─────────────────────┼─────────────────────┐
              ▼                     ▼                     ▼
        video (tiles,          audio (sound chip       app (window, menu, pads,
        sprites → picture)      → sound card)           save states, effects …)
```

1. **The ROM** (`src/board/romset.*`): the player's own `pacman.zip` is opened, every file is checked (name, size, checksum), and the bytes are loaded into memory. Nothing is written back or copied.
2. **The board** (`src/board/`): `Bus` is the Z80's view of the machine (which address is ROM, work RAM, video RAM, colour RAM, sprite registers, I/O ports). `Inputs` holds the joystick, coin and start buttons and the DIP switches (lives, bonus life, difficulty). `Machine` ties the Z80 core, the bus and the frame loop together: it runs one frame's worth of CPU cycles, then raises the vertical-blank interrupt, exactly like the arcade hardware.
3. **The Z80 core** (`src/core/z80.h`): a third-party, unmodified, single-file Z80 emulator (MIT licence, credit in `src/core/CREDITS.md`).
4. **Video** (`src/video/`): `gfx` decodes the graphics ROMs and colour PROMs into pixel arrays; `tilemap` draws the 28x36 tile layer; `sprites` draws the 8 hardware sprites. They are pure functions of the machine's memory: they read, they never change the game.
5. **Audio** (`src/audio/`): `wsg` is the 3-voice Namco sound generator (it turns the sound registers into samples); `audio_out` is the ring buffer and resampler that hands them to the sound card.
6. **The app** (`src/app/`): everything a player sees around the machine: the window, menus, controls, pads, save states, effects, statistics. It only talks to the machine through the same inputs a player has and by *reading* its memory.

**The rule that keeps the game accurate:** features live outside the machine. Overlay, statistics, achievements, lightbar and vibration only read memory after each frame. Save states, rewind and replays copy or restore the machine as a whole. Purist mode switches all of them off, and a test proves the machine then runs bit-for-bit like the plain emulator.

## Folder and file map

### `src/core/` the processor
| File | What it is |
|---|---|
| `z80.h` | Z80 emulator by Ivan Kosarev (MIT). Third party, unmodified. |
| `LICENSE-z80`, `CREDITS.md` | Its licence and credit. Must stay with the header. |
| `z80_test.cpp` | Runs a tiny known Z80 program and prints the registers (proves the core works on your compiler). |

### `src/board/` the arcade board
| File | What it is |
|---|---|
| `romset.h/.cpp` | Finds, opens and verifies the player's ROM zip (uses miniz). Friendly error text comes from `app/firstrun`. |
| `bus.h/.cpp` | Memory map, mirrors and I/O ports of the board. |
| `inputs.h/.cpp` | Input ports and DIP switches (all bits read 0 when active, like the real hardware). |
| `machine.h/.cpp` | Z80 + bus + frame loop with the vertical-blank interrupt. Also saves and restores the whole machine state. |
| `boot_test.cpp` | Early test program: boots the ROM on the core with stubbed I/O. |

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

### `src/logic/` the readable game rules (optional "Track B")
The game's rules rewritten as small readable C++ pieces, each proved identical to the ROM routine it copies. **Not yet used by the game**: the game still runs the original program. See `src/logic/README.md` and [MODDING.md](MODDING.md).

| File | Content |
|---|---|
| `random_number.h` | The random number generator. |
| `scoring.h`, `work_ram.h` | Scoring, extra life, high score; a view of work RAM using the original's addresses. |
| `dots_and_fruit.h` | Eating dots and energizers, ghost-house counters, fruit. |
| `ghost_targeting.h` | Each ghost's target tile and direction choice. |
| `movement.h` | Pac-Man and ghost movement, speeds, tunnel, Cruise Elroy. |
| `mode_timers.h` | Scatter/chase and frightened timers, per-level settings. |

### `src/third_party/` and `src/hello/`
`miniz/` is the zip reader (MIT, compiled into the program). `hello/main.cpp` is the very first toolchain test (a window that proves a Windows exe can be built).

### `tests/` the safety net
About 100 files. Most come as a `.sh` script (builds and runs one test) plus a `.cpp` (the check itself). Groups:
- **Machine proofs** (need your ROM): `test_cpu_parity`, `test_integration`, `test_replay`, `test_purist`, `test_scenarios`, `test_behaviour_*` and the `test_logic_*` proofs. Replay-based tests compare the state of the whole machine frame by frame with recorded hashes in `tests/data/`.
- **Front-end tests** (mostly no ROM needed): menus, bindings, mouse, CRT, glow, curve, surround, ultrawide, statistics, achievements, vibration, lightbar.
- **Documentation checks**: `tools/check_docs.py`, `tests/check_*.py`.
- `tests/run_all.sh` runs everything (needs the ROM, about 90 minutes). `tests/run_rom_free.sh` runs only what works without a ROM (see [BUILDING.md](BUILDING.md)).
- Shared helpers: `logic_probe.h` (the side-by-side proof method), `scenario.h` (set-ups that put the machine in a given game situation).

### `tools/`
| File | What it is |
|---|---|
| `build_app.sh` | Builds the program on Linux (`build/app/pacman-port`). |
| `build_windows.sh`, `package.sh` | Cross-compile the Windows exe and build the release zip (on Linux with MinGW). |
| `build_native.sh` | Builds and runs the Z80 core test. |
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
