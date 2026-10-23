# ROM data tables (step N10)

Which bytes of the program ROM (0000-3FFF) the Z80 reads as DATA (not as instruction bytes) in the four coverage runs. Address ranges, sizes and read counts only: no ROM bytes are written here. Source: `tests/rom_data_map.cpp`, run by `sh tests/test_rom_data_map.sh` (full run: players A, B, C of 300,000 frames and D of 54,000 frames); the complete per-address report is `docs/evidence/N10_data_map.txt`.

Method: a test-only copy of the bus (made by the test script from src/board/bus.cpp; the game's file is not changed) reports every ROM read. For each Z80 step the first reads are the instruction's own bytes (checked against the instruction length table); every further ROM read of that step is a data read. Three special kinds are kept apart because they would make every table look read: the power-on checksum, the random-number routine and the interrupt vector lookup (section S).

Totals of the full run: **28,824,865 data reads** from 85 reading instructions, 14,545 distinct ROM addresses. The self-checks of the tool (instruction lengths, fetch bytes, no data read in jump/call/return steps, no short step) were all 0.

## Game tables

| id | range | size | what it is, who reads it | data reads |
|---|---|---|---|---|
| T01 | 0068-007F | 24 | Small tables read by the routines 06BE and 0A7C | 1,350 |
| T02 | 0219-021F | 7 | Small table read by 01DC (called from the interrupt handler 008D, so read every frame) | 1,101,961 |
| T03 | 0796-0813 | 126 | Level records (126 bytes), read by 06BE | 8,394 |
| T04 | 0843-0878 | 54 | Speed and timing tables read by 06BE and 083A | 12,591 |
| T05 | 0EFD-0F3B | 63 | Fruit / bonus table read by 0EAD (and by RST 10 / 18) | 1,092 |
| T06 | 2728-272F | 8 | Bytes read through RST 10 / 18 (inline lists in the 2700 area) | 153 |
| T07 | 2B17-2B32 | 28 | Points table (score values), read through RST 10 / 18 | 29,358 |
| T08 | 2B73-2B77 | 5 | Five bytes inside the code at 2B73, read as data by 2BCD (the only code bytes read as data) | 6,030 |
| T09 | 3154-316B | 24 | Self-test table read by 2FAD | 37,944 |
| T10 | 32F9-32FE | 6 | Bytes read by 2FAD (via 3213, 3218) | 138,526 |
| T11 | 32FF-330C | 14 | Direction and offset tables (steps of Pac-Man and ghosts), read by 1806, 1A70, 2000, 291E, 2966 | 2,087,964 |
| T12 | 338D-3434 | 168 | Table read by 0814 (168 bytes) | 109,122 |
| T13 | 3435-35B1 | 381 | Maze map (381 bytes), read by 2419 | 365,760 |
| T14 | 35B5-36A4 | 240 | Dot positions (240 bytes), read by 2448 and 2487 | 361,200 |
| T15 | 36A5-3710 | 108 | Lists read through RST 10 / 18 (108 bytes) | 944,534 |
| T16 | 3713-39D0 | 702 | Text strings, read by the text printer 2C5E | 5,479,601 |
| T17 | 3A4F-3AE1 | 147 | Table read by 3AF4 (147 bytes) | 294 |
| T18 | 3AE2-3AF3 | 18 | Table read by 2FAD (18 bytes) | 4,182 |
| T19 | 3B08-3B2F | 40 | Sound table read by 2BEA | 10,780 |
| T20 | 3B30-3BAF | 128 | Sound tables read by 2BEA and 2DEE | 158,192 |
| T21 | 3BB0-3BD3 | 36 | Sound lists read through RST 10 / 18 and 2BEA | 79,734 |
| T22 | 3BD4-3CFF | 300 | Sound tables read by 2D44, 2F55 to 2F9B and 2BEA | 50,789 |
| T23 | 3D00-3E59 | 346 | Text strings read by 2C5E (and a few bytes by 2BEA) | 853,969 |

## Jump tables and inline data

| id | range | what it is | blocks | data reads |
|---|---|---|---|---|
| J01 | (see docs/disasm/blocks.txt) | all blocks named jumptableNNN there: word tables read through RST 10 / RST 20 (task and routine tables) | 21 blocks | 15,987,854 |
| J02 | (see docs/disasm/blocks.txt) | all blocks named inlineNNN there: bytes that follow a call to RST 28 / RST 42 and are read by that routine | 98 blocks | 357,152 |

(The rows J01 and J02 count the blocks of docs/disasm/blocks.txt; the test checks that the numbers quoted here are right. Their ranges are not repeated here because every jump-table word and inline byte is listed in that file.)

## S: special reads (kept apart)

| id | range | what it is | data reads |
|---|---|---|---|
| S01 | 0000-2FFF | Power-on ROM checksum loop (reading instruction 300A) reads every byte of the program ROM once per start | 626,688 |
| S02 | 0000-1FFF | ROM bytes used as random numbers (reading instruction 2A30; the routine 2A23 indexes the ROM with its seed, so any address below 2000 can be read) | 9,651 |
| S03 | 3FFA-3FFD | Interrupt vector lookup when the interrupt is delivered (made by the CPU, not by a program step) | 1,859,730 |

## What the native game must deliver

- Fixed tables (T01 to T23): levels, speeds, fruit and points, directions, maze, dots, text, sound. They come from the player's own ROM at run time (src/data/), never from the source.
- The random-number routine (S02) reads ROM bytes chosen by the seed: a native version must read the same bytes of the player's ROM, or the numbers differ.
- The power-on checksum (S01) reads the whole program ROM once; it is part of the start-up and can be reproduced by one pass over the ROM.
- T08 is code at 2B73-2B77 read as data: a native copy must take those five bytes from the ROM as well.
- Demo move lists: this run did not identify a table of its own for them; if they are in the ROM they are among the lists read through RST 10 / 18 (T15) or the inline data (J02). N11 onwards must confirm this before replacing the demo.
