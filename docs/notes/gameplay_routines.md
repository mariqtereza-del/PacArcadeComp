# Gameplay routines of the Pac-Man program ROM (step S11)

Sources: `docs/disasm/pacman.asm` (S08), `docs/notes/ram_map.md` (S10), the labels in `docs/disasm/labels.txt` (the eight S11 labels sit below the line `# ==== S11 gameplay labels`), and the prototype traces in `docs/evidence/S11_gameplay_trace.txt` (made by `python3 tools/analysis/oracle_s11.py`; the "trace section N" references below are its numbered sections). Names are our working names. Frame numbers are the prototype's (see boot_flow.md).

Program counters quoted from write traces are the values the prototype reports right after the writing instruction (the address of the next instruction).

Method (as the step requires): for each kind of routine I set a write trace on the RAM variables from the S10 map (every write in 4C00-4EFF with the program counter that did it, scenario A: scripted one-player game of 4400 frames), looked for the writing PC, read the code around it, and confirmed with execution counts and relation checks in three prototype runs (A: seed 7; B: seed 1, which reaches 70 dots so the fruit appears; C: attract mode only). The step allows at most eight named routines, so eight are labelled; the others found on the way are listed in section 3 without labels.

## 1. The eight labelled routines
| # | Kind | Address and name | Confidence | Evidence (the trace that found it) |
|---|---|---|---|---|
| 1 | Ghost target choice | 2966 `ghost_pick_direction_toward_target` | HIGH | Write trace: the ghost direction bytes 4D2C-4D2F and the vectors 4D1E-4D25 are written right after calls to 2966 from six places (return addresses 2751, 2765, 2787, 27A2, 27C4, 27EA: the four target tasks). Trace section 10: 559 calls; the new direction was never the reverse of the old one; Blinky's chase calls passed Pac-Man's tile exactly 82 of 82 times, its scatter calls passed 221D 194 of 194 times. |
| 2 | Ghost movement | 1B36 `ghost_blinky_move` (copies for Pinky 1C4B, Inky 1D22, Clyde 1DF9) | HIGH | Write trace: 4D00/4D01 (Blinky position) are written only at pc 1C44 (plus boot code), and 1C44 lies inside 1B36-1C4A. Trace section 1: executed 5012 times, once per update. The result is the sprite position checked in S10 (staging bytes at 1490 add 6 and complement the position). |
| 3 | Mode timer | 0E36 `ghost_mode_timer` | HIGH | Write trace: 4DC2/4DC3 (counter) written at pc 0E49 every frame; 4DC1 (phase) at pc 0E62; flags 4DB1-4DB4 at pcs 0E68 and 0E6B. Trace section 3: phase 0 to 1 at frame 1954 with the counter at 419 (table word 420 = 7 s); table 420, 1620, 2040, 3240, 3540 frames = 7, 27, 34, 54, 59 s. |
| 4 | Dot eating | 19E0 `eat_dot_at_pacman_tile` | HIGH | Write trace: 4E0E written only at pc 19E7. Trace section 4: executed 101 times and 4E0E rose by exactly 1 in 101 frames; each execution also blanks the tile (S10 screen check) and calls the score task. |
| 5 | Fruit | 0EAD `fruit_spawn_check` | HIGH | Scenario B (seed 1) reached it: trace section 8: at frame 2144, 4E0E = 70, fruit position 4DD2 = 0x..94 (word 8094), sprite code 00, colour 14 (cherry). The ROM table at 0EFD starts (00,14,06), (01,0F,07), (02,15,08) (section 13). Not reached in scenario A (only 69 dots). |
| 6 | Scoring | 2A5A `add_score` | HIGH | Write trace: 4E80/81/82 written only at pcs 2A6C, 2A71, 2A78; high score 4E88 at 2AA6. Trace section 6: 101 calls, 101 score changes (0 to 10 to 20 ... BCD). Points table at 2B17 = 10, 50, 200, 400, 800, 1600, 100, 300, 500, 700 (section 13). |
| 7 | Ghost-house release | 2069 `ghost_house_release_check_pinky` (copies 208C Inky, 20AF Clyde) | MED | Trace section 7: Pinky released at frame 676 (limit 0), Inky at frame 1092 with its counter 4E10 = 30 and limit 4DB9 = 30. Clyde left at frame 2302 with counter 4E11 = 26 against limit 50 on level 2, so Clyde (or the level 2 case) uses a rule I have not found: that is why MED. |
| 8 | Sound request | 0E6C `siren_sound_select` | MED | Write trace: 4EAC written from pcs 0E76 and 0EAC (inside it) every update (trace section 9: 3168 times). The bits it sets are the request bits read by sound_update_group_A (S09). What each bit sounds like is for the audio steps, so MED. |

### How the pieces fit (what the code does, in order)
`gameplay_frame_update` (1017, not labelled) runs **twice per frame** (trace section 2: 2 executions on every playing frame) from the interrupt handler's mode dispatch (calls at 052C/052F in attract demo and 08EB/08EE in play). Each run: 1291, then (unless Pac-Man is dying, 4DA5) 1066 (ghost state changes), 1094-10B4 (the four ghost state handlers), the collision scan 171D and 1789, Pac-Man's speed tick 1806, the four ghost movers 1B36, 1C4B, 1D22, 1DF9, and, only in state 4E04 = 3, 1376 (power pill timer), 2069, 208C, 20AF (ghost-house release). Directions are chosen by tasks: a ghost mover that stands in the middle of a tile (pixel position & 7 = 4) queues task 08 to 0B (target choice, section 2 of the table) or 0C to 0F (frightened or eyes), which call 2966 (toward a target) or 291E (random, using the ROM bytes as a random table through the pointer 4DC9).

Target rules read from the code (confirmed only where the trace section says so):
- Blinky (task 08 at 2730): scatter target 221D when the phase 4DC1 is even and Blinky is not in "Cruise Elroy" (4DB6 = 0); otherwise Pac-Man's tile 4D39. Confirmed (trace section 10).
- Pinky (task 09 at 276C): scatter target 391D; otherwise Pac-Man's tile plus four times Pac-Man's direction vector (4D1C). Executed 211 times; target values not separately compared.
- Inky (task 0A at 27A9): scatter target 2040; otherwise reflects Blinky's tile around the point two tiles ahead of Pac-Man. Executed in B and C only a few times.
- Clyde (task 0B at 27F1): scatter target 3B40; chases Pac-Man's tile unless the squared distance is below 0x40 (8 tiles), in which case it goes to its scatter corner. Executed only in the attract run (C), because Clyde did not leave the house early in A and B.

## 2. Other findings
- Demo game: task 17 (28E3) is Pac-Man's direction chooser when nobody controls him (queued at 1A3F when the call to 1ED0 at 1A3A returns with no carry; what 1ED0 tests has not been examined). Executed 258 times in the attract-only run (C), never with a coin inserted (A, B). The joystick path writes 4D3C and 4D26 at 1AD1-1B03.
- The phase table at 4D86 is per level (copied by the routine at 0740-0795 from the tables at 0843-0873), and the phase restarts at 0 whenever a life is lost (trace section 3).
- Ghost reversal on a phase change: flags 4DB1-4DB4 (set by 0E36 and by a power pill at 1A70) are consumed by 1EFE, 1F25, 1F4C, 1F73, which turn the ghost round (direction xor 2).
- Power pill (1A70, not labelled): copies the level's frightened time 4DBD to the counter 4DCB, sets 4DA6 and the four edible flags 4DA7-4DAA, turns the ghosts blue (sprite code 1C, colour 11) and requests the sound. Executed once in A at frame 999 (trace section 5).
- Extra life: when the score passes the 4E71 limit, 2B33 adds one to 4E14/4E15 once per game and sets bit 0 of the sound request 4E9C.

## 3. Found but not labelled (the step limits labels to eight)
| Address | What it is | Evidence | Confidence |
|---|---|---|---|
| 1017 | gameplay_frame_update: runs twice per frame | trace sections 1 and 2 | HIGH |
| 171D / 1789 | ghost versus Pac-Man collision scan and its outcome (kills: 4DD0 counter, points index 4DD0+1, sound bit 3 of 4EBC; otherwise Pac-Man dies, 4DA5) | executed every update; no ghost was eaten in A or B, so the kill branch was not seen running | MED (death branch seen: 3 lives lost in A at frames 2447, 3291, 4318; kill branch code only) |
| 1806 | Pac-Man speed tick (delay 4D9D, speed pattern 4D46-4D4D shifted each frame) | write trace of 4D46-4D49 at 1836/183E | HIGH |
| 1A6A / 1A70 | energizer detection and power pill start | trace section 5 | HIGH |
| 1EFE, 1F25, 1F4C, 1F73 | per-ghost forced reversal | executed 311 times for Blinky in A | HIGH |
| 2000, 2018, 205A | add vector to position; pixel to tile (divide by 8, add 32 and 30: matches S10's 4D39/4D3A rule); tunnel test | executed 15774 and 7014 times | HIGH |
| 291E | random direction for frightened ghosts | executed 57 times in A at power pill time | MED |
| 28E3 | Pac-Man direction in the demo | attract run C | HIGH |
| 1B08 | counts a dot for the ghost-house counters | 101 calls, one per dot | HIGH |
| 0E23 | toggles ghost animation flag 4DC0 every 8 frames | write trace | MED |

## 4. Open items and honest limits
- Clyde's early release (frame 2302 in scenario A) is not explained by the personal-counter rule; look at 20D7 and 4D97 (inactivity timer, written at 13ED/13F5) in a later step.
- The ghost-kill branch, the frightened direction tasks 0D, 0E, 0F and Pinky's and Inky's chase targets were seen executing only a few times; their numbers have not been compared with the Dossier (the tables are read in S12).
- Evidence comes only from the Python prototype; the C++ port will have to reproduce the same writes (S18 compares RAM every frame).
