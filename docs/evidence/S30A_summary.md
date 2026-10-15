# S30A evidence summary: level 256 and high-level tests

Run: `sh tests/test_level256_full.sh --pixels` (about 3 minutes; all PASS). Raw outputs: S30A_level256_full.txt (16 checks), S30A_high_levels.txt (37 checks), S30A_mame_compare.txt.
Pictures: S30A_level256_port.png (the port), S30A_level256_mame.png (MAME 0.264), S30A_level256_side_by_side.png.

## What was shown (and how strong each item is)
| Question | Result | Kind of proof |
|---|---|---|
| Does the ROM itself carry the game from level 1 to 256? | Yes: counter 0,1,...,255 one at a time; 256 level starts; 0 table mismatches against the ROM tables of S12 (levels 21+ use the level-21 entry) | NATURAL progression, with shortcuts listed below |
| Intermissions | Level state 32 happens exactly at counter values 1, 4, 8, 12, 16 (= after levels 2, 5, 9, 13, 17, as in the Dossier) and nowhere else | natural |
| Does level 256 behave like the Dossier says? | Counter stays 255 for 60,000 frames, ROM keeps running, level cannot be completed (best 143 of 244 dots), 123 dot/energizer tiles at the start (114 left + 9 right) | natural |
| Is the level-256 screen right? | Video RAM + colour RAM (2,048 bytes) identical to MAME; the 224 x 288 picture (tiles and sprites) identical to MAME's screenshot pixel for pixel | MAME 0.264 (Ubuntu package) running the same ROM set and the same input script |
| Level tables and events | The 256 level-start table lines, all level changes, intermissions, the level-256 counts, the rack-test timing: identical to MAME, including frame numbers | MAME |
| Rack-test switch on level 256 | Level ends, counter wraps 255 -> 0, the maze is a normal one again, speeds and times stay at level-21 values (the level-record pointer 4E0A stays at the end of its table), the next finished maze brings the counter to 1 and the first cutscene again. Same in MAME | natural + MAME. The Dossier's "ghost behaviour as at level 21" is confirmed; the earlier worry that the wrap would restore level-1 parameters is answered: it does not |
| Levels 5, 21 and 256: speeds, first scatter phase, power pill length, Elroy dot counts, ghost targets | 37/37 checks: Pac-Man and Blinky speeds within 0.1 % of the table, first scatter phase exactly 300 frames, pill 120 frames on level 5 and no frightened time on 21 and 256, Elroy at 40/20 (level 5) and 120/60 (levels 21, 256) dots left, 1,300-1,700 ghost target calls per level all following the Dossier rules including the up-facing bug | natural climb to the level, then measured on the running ROM |

## Harness shortcuts (test code only; the game program is untouched)
1. The dots-eaten counter 4E0E is set to 243 once per level while playing, so that one more dot ends the level (the ROM does the 244 check itself).
2. Lives are topped up when fewer than 2 are left.
3. A fixed pseudo-random joystick script (identical in C++ and in MAME).
4. High-level test only: ghosts released (4DA1-3), one time jump of the mode counter per scatter phase, energizers put under Pac-Man, dots-eaten counter held for the Elroy check, all marked in the output.
5. The rack-test switch is held for the first playing frame after 60,000 frames on level 256.

## Findings worth knowing
- **Poking the level counter alone is not a faithful level 256.** The speed and time tables follow a pointer (4E0A) that moves one record per finished level; a poked counter gives level-2 speeds on a level-256 maze. The maze/garbage depend only on 4E13 (so the S30 picture is right), but behaviour tests must climb naturally. test_level256_full.cpp part 2 records this (poked run = level-2 table, natural = level-21 table).
- Elroy flags stay on once set and are only evaluated while Blinky is out and moving normally; a dot eaten in the same frame can make the flag appear one dot "early" (seen once per level at left = threshold + 1: not a difference).
- MAME's rack-test field is a toggle: it needs a pulse (press edge) to flip, not a held value; MAME also saves switch positions between runs unless config reading is disabled (the script does that).
- The new Claude Doc 'Pac-Man Port: Level 256 Research' predates this step: its gaps D1, D3 and D5 are now closed by this evidence (D2 is closed for levels 5/21/256 only).

## Negative controls (the checks can fail)
- One byte of our video dump changed -> the MAME comparison prints DIFFERENT, 1 byte, and RESULT: FAIL.
- One table value changed in tests/data/level_expect.txt -> test_level256_full reports 1 mismatch and fails.
- One expected speed changed -> test_high_levels reports 2 failures.

## Not shown / still unknown
- MAME is a trusted emulator, not a real cabinet; both could share an error against hardware (the one place the Dossier and MAME could not arbitrate is not involved here: the garbage is determined by ROM bytes, the Z80 and the memory map).
- Sound is not compared. Sprites are compared only in the one level-256 frame (they were identical).
- Levels 2-4, 6-20 and 22-255 were checked for tables only, not for speeds and ghost targets (levels 5, 21 and 256 stand for the three table regimes; 21+ are identical by the table).
- The overlay (F3) is still not wired to the window (planned for S31).
