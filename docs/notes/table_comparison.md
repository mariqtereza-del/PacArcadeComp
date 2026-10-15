# S12 - ROM tables versus the Pac-Man Dossier

Source of ROM values: `build/rom/prog.bin`, decoded by `tests/test_rom_tables.py` (stored in `tests/data/rom_tables.json`, key `rom`).
Source of Dossier values: https://pacman.holenet.info/ (Appendix A table A.1 and the text sections), read with short targeted fetches; stored in the same JSON under `dossier`. Fetch results are summaries, so every Dossier number was cross-checked against the ROM rather than trusted alone.

## Result
All 21 levels x 15 fields match. **DISAGREEMENTS: none.** (Test prints "0 disagreements".)

## Per table
| Table (ROM address) | Result |
|---|---|
| Level records 0796 (6 bytes x 21; levels 21+ reuse record 21) | decoded; used to index every table below |
| Speeds, records at 330F (42 bytes: 7 patterns + 7 mode words) | MATCH. Speed % = ones in the 32-bit pattern x 5. L1 80/90/85/80/75/50/40, L2-4 90/95/95/90/85/55/45, L5-20 100/100/105/100/95/60/50, L21+ 90/90/105/100/95/45/50 (pac, pac-fright, Elroy 2, Elroy 1, ghost, ghost-fright, tunnel). |
| Mode timings (mode words, frames) | MATCH. L1 420,1620,2040,3240,3540,4740,5040 = 7/20/7/20/5/20/5 s. L2-4 ends 65534/65535 (chase 1033 s then 1 frame scatter). L5+ 300,1500,1800,3000,3300,65534,65535 = 5/20/5/20/5/1037/1 frame. |
| Frightened times 0861 (units of 1/120 s) | MATCH, levels 1-21: 6,5,4,3,2,5,2,2,1,5,2,1,1,3,1,1,0,1,0,0,0 s. |
| Elroy dots-left 084F | MATCH (20/10 ... 120/60). |
| Ghost-house personal limits 0843 | MATCH (L1 0/30/60, L2 0/0/50, L3+ 0/0/0). |
| Ghost-house global counter values (code at 2079, 209C, 20BF) | MATCH: 7, 17, 32. Nuance: the Dossier describes 32 as the check that switches the global counter off; the ROM compares against 0x20 at 20BF the same way. |
| Inactivity limit 0873 | MATCH (4 s levels 1-4, 3 s level 5+; ROM 240/240/180 frames). |
| Points table 2B17 (BCD words) | MATCH: 10, 50, 200, 400, 800, 1600, 100, 300, 500, 700, 1000, 2000, 3000, 5000. |
| Fruit table 0EFD (3 bytes per level, level capped at 21 entries) | MATCH: cherry 100, strawberry 300, peach 500 x2, apple 700 x2, grapes 1000 x2, galaxian 2000 x2, bell 3000 x2, key 5000 from level 13. |
| Fruit triggers (code at 0EBB/0EBF) | MATCH: 70 and 170 dots eaten. |
| Dot / energizer pause | MATCH (S11): 1 and 3 frames. |

## Derived or open items (not disagreements)
- **Number of flashes** is not a ROM table. The ROM blinks while the frightened counter 4DCB is below 0x100 (code 0AE3) and toggles every 14 frames; this gives 5 flashes for times of 2 s or more and 3 for 1 s, equal to the Dossier. The test checks this derived value.
- **Fright columns for levels with no frightened time** (17, 19, 21+): the Dossier leaves them blank; the ROM still holds speed patterns but they are never used. Not compared.
- **"Dots speed" columns** in the Dossier are derived from the speed, not ROM tables; not compared.
- **Level record byte 1** (copied to 4DB0) is only read by the self-test; role unknown, no Dossier counterpart.
- **Fruit on-screen time (9-10 s)** was not decoded here; left for a later step.
- **Frame rate**: the Dossier says 60 fps, the hardware runs 60.606 Hz. Mode times in frames match exactly; the Dossier's seconds are rounded accordingly.
