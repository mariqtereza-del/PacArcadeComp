# src/logic: the game's rules as readable C++ (Track B, optional step T01)

The program ROM of the arcade machine is a Z80 program. The port runs that program, so the game is exactly right by construction. This folder
holds the same rules written the way a person would write them, one small piece at a time, so the code can be read, studied and edited.

Rules of this folder:
- A piece is added only after a test has shown that it does exactly what the ROM routine does (see "Proof" below). Nothing here is used by
  the game yet: the game still runs the ROM. A piece becomes "in use" only in a later, separate step.
- Every piece names the ROM address it copies, and the RAM addresses it reads or writes (docs/notes/ram_map.md, docs/notes/gameplay_routines.md).
- No piece reads the player's ROM except through the argument the caller passes in (the ROM is never part of this repository).

Proof (the same for every piece): tests/logic_probe.h runs the real machine one instruction at a time. When the machine enters the ROM
routine, the probe gives the readable version the same inputs; when the ROM routine returns, the probe compares the result and every RAM
byte the routine writes. Any difference stops the run and prints the frame, the instruction and both answers.

| Piece | File | Copies ROM routine | Proof |
|---|---|---|---|
| Random numbers (T01a) | random_number.h | 2A23 `get_random_byte` | tests/test_logic_random.sh, docs/evidence/T01a_random.txt |
| Scoring, extra life, high score (T01b) | scoring.h, work_ram.h | 2A5A `add_score` (with 2B33 `give_extra_life_once`) | tests/test_logic_scoring.sh, docs/evidence/T01b_scoring.txt |
| Dots, energizers, ghost house counters, power pill start, fruit appearance (T01c) | dots_and_fruit.h | 19CD (eating), 1B08, 1A70, 0EAD | tests/test_logic_dots.sh, docs/evidence/T01c_dots_fruit.txt |
| Ghost targeting: the four target rules and the direction chooser (T01d) | ghost_targeting.h | 2966 (+29EA, 2A12), 2730, 276C, 27A9, 27F1 | tests/test_logic_ghosts.sh, docs/evidence/T01d_ghost_targeting.txt |
| Movement: Pac-Man and the four ghosts, speed patterns, tunnel, Cruise Elroy, fruit eating (T01e) | movement.h | 1806, 1B36, 1C4B, 1D22, 1DF9 (+2000, 2018, 205A, 1ED0, 1EFE-1F73, 20D7) | tests/test_logic_movement.sh, docs/evidence/T01e_movement.txt |
| Mode timers: scatter/chase timer, frightened timer, turn-round flags, level settings (T01f) | mode_timers.h | 0E36, 1376, 070E (tables 0796, 0843-0873, 330F) | tests/test_logic_timers.sh, docs/evidence/T01f_mode_timers.txt |

Not copied (left to the ROM): redrawing the score, the high score and the little Pac-Man pictures in video RAM (T01b); Pac-Man eating the fruit, ROM 19A1-19C4 (T01c); the drawing of the level's fruits, ROM 2BEA (T01f). The proof of T01b compares all work RAM (4C00-4FEF) after every call; the proof of T01c also compares the video RAM (the maze, where the dots are) and the colour RAM.
