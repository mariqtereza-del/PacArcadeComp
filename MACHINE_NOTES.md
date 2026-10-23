# MACHINE_NOTES - how the Midway Pac-Man arcade machine works (as established by this project)

Merged on 2026-10-08 (step S13) from the study notes; no new research was added. Every claim names its source file; `python3 tools/check_docs.py` checks that each cited path exists. The ROM itself is never part of this repository's releases.
Confidence words: HIGH = code and prototype agree; MED = clear from the code, role inferred; LOW = few observations. "Prototype" = the Python reference in `tools/analysis` running the user's ROM; all game-logic evidence comes from it only.

**Native game (steps N01 to N53).** These notes describe the arcade *machine*, and they still hold. The program in this repository no longer runs the original instructions: the game is native C++ proved identical to them. Where to look: the routine map `docs/notes/rom_routines.md`, the data the game still reads from the ROM `docs/notes/rom_data_tables.md`, the timing of the frame and interrupt `docs/notes/native_timing.md`, the names of the work RAM `src/game/state.h`, the names of the screen, sprite, sound and latch memory `src/game/output.h`, and the overview in `ARCHITECTURE.md`.

## 1. Memory map
Source: `docs/notes/hardware_facts.md` section 2 (MAME driver + prototype experiments in `docs/evidence/S07_prototype_experiments.txt`).
- 0000-3FFF program ROM (mirrored at 8000-BFFF). 4000-43FF video RAM (tile codes), 4400-47FF colour RAM (palette & 0x1F), both mirrored at A000.
- 4800-4BFF is NOT RAM: reads give 0xBF, writes are ignored (corrects the Lomont guide; no effect on the game, experiment E14). Work RAM is 4C00-4FEF; sprite RAM 4FF0-4FFF (8 byte pairs).
- 5000-5007 write latch (only bit 0 counts): 5000 interrupt enable, 5001 sound enable (NOT 50C0), 5003 flip screen, 5007 coin counter; 5002, 5004-5006 do nothing on real hardware. 5040-505F sound registers, 5060-506F sprite coordinates, 50C0 write = watchdog (reset after 16 frames without a write).
- Reads: 5000 IN0, 5040-507F IN1, 5080-50BF DIP switch 1, 50C0-50FF DIP 2 (unused).
- RAM variables: 56 named and checked variables in `docs/notes/ram_map.md` (evidence `docs/evidence/S10_ram_values.txt`, test `tests/check_ram_map.py`). Correction found in S12: 4E0A/4E73 is not a "difficulty pointer" only; it is the pointer into the level-order byte list (see section 6).

## 2. Video
Source: `docs/notes/hardware_facts.md` section 4.
- Picture 224x288 (28x36 tiles of 8x8); the native picture is 288x224 turned 90 degrees. Tiles 2 bits per pixel, 16 bytes each. Sprites 16x16, 2 bits per pixel, 64 in pacman.5f; colour 0 transparent; lower sprite numbers are drawn on top.
- Sprite position: x = 272 - byte(5061+2n), y = byte(5060+2n) - 31 (native picture). Sprite palette is 5 bits; tile palette is colour RAM & 0x1F. Sprites are clipped to native x 16..271, y 0..223 and also plotted at x-256; sprites 0-2 are one pixel further along in MAME (open: U2).
- Colours: 3-bit red and green, 2-bit blue from PROM 82s123.7f (resistor weights 0x21/0x47/0x97 and 0x51/0xAE); look-up PROM 82s126.4a, 4 bits per entry.
- Tile scan formula identical in MAME and the prototype. Golden frame hashes: `docs/notes/prototype_findings.md` (guard test `tests/test_prototype.py`).

## 3. Sound
Source: `docs/notes/hardware_facts.md` section 5.
- 3 voices, 4-bit samples, sound clock = CPU clock / 32 = 96 kHz. Only PROM 82s126.1m is used for waveforms (8 waveforms of 32 samples); 3m is not used.
- Registers: voice 1 accumulator 5040-5044, waveform 5045, frequency 5050-5054, volume 5055; voice 2 5046-5049, 504A, 5056-5059, 505A; voice 3 504B-504E, 504F, 505B-505E, 505F. Sound runs only when 5001 = 1.
- The interrupt handler copies the sound shadow 4E8C-4E9B to 5050-505F each frame and runs sound_update_group_A (2D0C) and group B (2CC1) (`docs/notes/boot_flow.md` section 3). Mixing details are UNRESOLVED (U3).

## 4. Timing
Sources: `docs/notes/hardware_facts.md` section 1, `docs/notes/boot_flow.md`, `docs/notes/gameplay_routines.md`.
- Z80A at 3.072 MHz, interrupt mode 2, 60.606 frames per second (pixel clock 6.144 MHz / 384 x 264), 50,688 CPU cycles per frame. VBLANK starts at line 224 (cycle 43,008). The interrupt vector byte is set with OUT (0),A: FA at boot, FC after boot; the words at 3FFA = 3000 and 3FFC = 008D.
- Boot: reset 0000 -> boot_init 230B -> self-test 3000 (frames 1-252, about 5 s of delays) -> main_init 234B (frame 296) -> task_loop 238D -> first real interrupt 008D (frame 299) -> first screen text frame 302 (`docs/notes/boot_flow.md` section 1, evidence `docs/evidence/S09_prototype_trace.txt`).
- Interrupt handler 008D: watchdog, sound copies, sprite staging copies, update_frame_counters, run_timed_events, game_mode_dispatch, input reading, sound updates (`docs/notes/boot_flow.md` section 3). Outside the interrupt the CPU polls a task queue (4CC0-4CFF, pointers 4C80/4C82); tasks are queued with RST 28h and timed events with RST 30h.
- The gameplay update 1017 runs twice per frame; the mode timer 0E36 counts frames; the power-pill counter 4DCB counts half-frames (`docs/notes/gameplay_routines.md`, evidence `docs/evidence/S11_gameplay_trace.txt`).
- Disassembly: `docs/disasm/pacman.asm` (re-assembles to the identical ROM, `docs/evidence/S08_roundtrip.txt`); labels `docs/disasm/labels.txt` (35).

## 5. Inputs
Source: `docs/notes/hardware_facts.md` section 6 (MAME input definitions; DIP meanings to be confirmed by running the ROM in S23). All inputs are active low.
- IN0 (5000): bits 0-3 joystick 1 up/left/right/down, bit 4 rack test (1 = off), bits 5-7 coin 1, coin 2, service coin.
- IN1 (5040): bits 0-3 joystick 2, bit 4 service switch, bits 5-6 start 1 / start 2, bit 7 cabinet (1 = upright).
- DIP 1 (5080, default 0xC9): bits 0-1 coinage, 2-3 lives (00 one, 01 two, 10 three, 11 five), 4-5 bonus life (10,000 / 15,000 / 20,000 / none), bit 6 difficulty (1 normal), bit 7 ghost names (1 normal). The ROM reads 5000, 5040 and 5080 only (experiment E1).

## 6. Tables (decoded from the ROM, compared with the Pac-Man Dossier: no disagreements)
Sources: `docs/notes/table_comparison.md`, `tests/data/rom_tables.json`, `tests/test_rom_tables.py` (21 levels x 15 fields).
- Level records at 0796 (6 bytes x 21; levels past 21 reuse the last). Level order list at 0068 (hard DIP: 007D) read through the pointer 4E0A. Record bytes: speed record, (byte 1: only the self-test reads it, purpose unknown), ghost-house limit index, Elroy index, frightened-time index, inactivity index.
- Speed records at 330F (42 bytes: 7 patterns, 7 mode words). Speed % = ones in the 32-bit pattern x 5; 100% = 1.25 pixels per frame. Level 1: Pac-Man 80, ghosts 75, tunnel 40, frightened Pac-Man 90 / ghosts 50, Elroy 80 and 85; levels 2-4: 90/85/45, 95/55, 90 and 95; levels 5-20: 100/95/50, 100/60, 100 and 105; level 21+: 90/95/50, Elroy 100 and 105.
- Scatter/chase timer (frames): level 1 420, 1620, 2040, 3240, 3540, 4740, 5040 (7/20/7/20/5/20/5 s); levels 2-4 as level 1 until 3540, then chase for 65534 frames; level 5+ 300, 1500, 1800, 3000, 3300, then chase.
- Frightened time (table 0861, units of 1/120 s): 6,5,4,3,2,5,2,2,1,5,2,1,1,3,1,1,0,1,0,0,0 s for levels 1-21. Flashes (derived from the blink rule at 0AE3: below 0x100 on 4DCB, 14-frame toggle): 5, or 3 for 1-second levels.
- Elroy dots left (084F): 20/10, 30/15, 40/20, 50/25, 60/30, 80/40, 100/50, 120/60. Ghost-house personal limits (0843): level 1 0/30/60, level 2 0/0/50, later 0/0/0; global counter values 7, 17, 32. Inactivity timer 4 s (levels 1-4) then 3 s.
- Points (2B17): 10, 50, 200, 400, 800, 1600, 100, 300, 500, 700, 1000, 2000, 3000, 5000. Fruit (0EFD): cherry, strawberry, peach x2, apple x2, grapes x2, galaxian x2, bell x2, key from level 13; appears at 70 and 170 dots. Dot pause 1 frame, energizer 3 frames.

## 7. Routines (names are ours; the ROM has none)
Sources: `docs/notes/gameplay_routines.md`, `docs/notes/boot_flow.md`, `docs/disasm/labels.txt`.
- Eight gameplay routines labelled in S11: ghost direction choice 2966, Blinky movement 1B36 (copies 1C4B, 1D22, 1DF9), mode timer 0E36, dot eating 19E0, fruit 0EAD, scoring 2A5A, ghost-house release 2069 (208C, 20AF), siren request 0E6C. About ten more are listed unlabelled in the gameplay note (e.g. 1017 gameplay update, 171D/1789 collisions, 1806 Pac-Man speed tick, 1A70 power pill).
- Ghost targets: Blinky chases Pac-Man's tile (82 of 82 checked); Pinky four tiles ahead; Inky reflects Blinky's tile around the point two ahead; Clyde chases unless within 8 tiles. Only Blinky's rule was compared numerically.

## 8. Disagreements and open items
- Corrected from the Lomont guide (`docs/notes/hardware_facts.md` sections 2-4, 7): RAM starts at 4C00; sound enable is 5001; no lamp/lockout hardware; sprite palette is 5 bits; only one sound PROM is used.
- Prototype versus MAME: the prototype returns 0 for reads of 4800-4BFF (hardware 0xBF; no effect, port uses 0xBF); it delivers the interrupt after the whole frame (U1); it lacks the sprite offset hack and clipping (U2).
- Prototype/measurement versus pacman-c (`docs/notes/ram_map.md`, last sections): sprite staging sign/offset, names of 4DCE and 4C84, a typo for the player-2 array address; measured values are used.
- Dossier versus hardware: the Dossier says 1/60 s per frame, the hardware is 60.606 Hz (`docs/notes/dossier_facts_partial.md`). Dossier tables versus ROM: none (`docs/notes/table_comparison.md`).
- Open: U1, U2, U3 (above); Clyde's early release in scenario A (frame 2302, counter 26 against limit 50: rule not found, look at 20D7 and 4D97); ghost-kill branch and frightened direction tasks seen only rarely; fruit on-screen time (9-10 s) not decoded; level-record byte 1 (4DB0) unknown; flash count derived, not read from a table; two small leftovers of S09/S10 (conditional sprite swap 0114-0175; LOW-confidence variables 4E03, 4E01, 4DD2, 4E0C/4E0D).
- All game-logic evidence is from the Python prototype only; the C++ port must reproduce the same writes (S18).

## 9. Sources
- Lomont, Pac-Man Emulation Guide v0.1 (`docs/notes/hardware_facts_lomont.md`); MAME `pacman.cpp` and `pacman_v.cpp` (`docs/notes/hardware_facts.md`); pacman-c `memmap.h` (`docs/notes/ram_map.md`); The Pac-Man Dossier, pacman.holenet.info (`docs/notes/dossier_facts_partial.md`, `docs/notes/table_comparison.md`); the user's own ROM (check `docs/evidence/S02_rom_check.txt`); the Python prototype (`docs/notes/prototype_findings.md`).
- Project files: plan `docs/PLAN.md`, status `PROJECT_STATUS.md`.
