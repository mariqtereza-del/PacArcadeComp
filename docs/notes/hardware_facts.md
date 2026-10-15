# Hardware facts, cross-checked (step S07, 2026-10-07)

Status words: CONFIRMED (second source agrees), CORRECTED (the Lomont wording was wrong or loose, corrected text given), NEW (not in Lomont; added from the second source), UNRESOLVED (needs a prototype experiment, written down at the end).

Sources:
- [L] Lomont guide, as recorded in docs/notes/hardware_facts_lomont.md (kept, not deleted).
- [M] MAME driver, Midway "pacman" set: `pacman.cpp` and `pacman_v.cpp` (from GitHub master, read 2026-10-07; the local copies are in the scratchpad `/tmp/mame_src`, and the quoted lines are listed under each fact).
- [P] Python prototype in tools/analysis, run on the user's ROM. Raw output: docs/evidence/S07_prototype_experiments.txt (experiments E1 to E14, scripts `tools/analysis/oracle_s07*.py`).
- [R] The ROM itself (disassembly docs/disasm/pacman_raw.asm).

## 1. Clock, screen, timing
| Fact | Status | Evidence |
|---|---|---|
| Z80A at 3.072 MHz (18.432 MHz / 6) | CONFIRMED | [M] clock 18.432 MHz / 6 |
| Interrupt mode 2 | CONFIRMED | [P] E7: I register = 3F, vector table in ROM |
| 28x36 tiles, 8x8, 224x288 picture | CONFIRMED | [M] tile scan 36x28; see the scan note below |
| Native (unrotated) picture is 288 wide x 224 tall, shown rotated 90 degrees | CONFIRMED | [M] `set_raw(18.432MHz/3, 384, 0, 288, 264, 0, 224)` with ROT90 |
| 60.61 frames per second | CONFIRMED | pixel clock 6.144 MHz / (384 x 264) = 60.606 Hz |
| 50,688 CPU cycles per frame | CONFIRMED | 192 cycles per line x 264 lines; the prototype uses this number |
| VBLANK starts at line 224, i.e. 43,008 cycles into the frame | NEW | [M] visible lines 0 to 223 of 264. The prototype delivers the interrupt after the whole frame (see UNRESOLVED U1) |
| Interrupt vector: the program runs `OUT (0),A`; on VBLANK the CPU jumps through the word at I*256 + that byte | CONFIRMED | [M] port 0 write sets the vector byte. [P] E7 and E8: I = 3F; byte FA at boot, FC after boot; ROM words 3FFA = 3000 and 3FFC = 008D. The "CD" in Lomont's text is only an example |
| The interrupt is level-sensitive and only raised when the enable bit (5000 bit 0) is set | NEW | [M] `vblank_irq`, `irq_mask_w(0)` clears the line. Effect on the port: U1 |

Tile scan note: [M] `row += 2; col -= 2; if (col & 0x20) return row + ((col & 0x1f) << 5); else return col + (row << 5);`. This is identical to the prototype's `tile_offset`.

## 2. Memory map (addresses as seen by the CPU)
| Range | Meaning | Status | Evidence |
|---|---|---|---|
| 0000-3FFF | ROM, repeated at 8000-BFFF (mirror 0x8000) | CONFIRMED, mirror NEW | [M] |
| 4000-43FF | video RAM: tile codes, repeated at A000 (mirror 0xA000) | CONFIRMED, mirror NEW | [M] |
| 4400-47FF | colour RAM: tile palette numbers, mirror 0xA000 | CONFIRMED | [M] |
| 4800-4BFF | not RAM. Reads return 0xBF, writes are ignored | CORRECTED (Lomont says RAM starts at 4800) | [M] `pacman_read_nop` returns 0xBF. [P] E2: the ROM never writes there (0 writes) |
| 4C00-4FEF | work RAM | CORRECTED (4C00, not 4800) | [M] |
| 4FF0-4FFF | sprite RAM: 8 pairs of bytes (number and flip, then palette), mirror 0xA000 | CONFIRMED | [M]. Note [M]: the real Midway board only has the middle 6 sprites; the program still writes 8 |
| 5000-5007 | write: 8-bit latch (LS259). Details per address are in section 3 | CONFIRMED with corrections | [M] mirror 0xAF38 |
| 5040-505F | write: sound registers | CONFIRMED | [M] |
| 5060-506F | write: sprite coordinates (see section 4) | CONFIRMED | [M] |
| 5070-507F, 5080 | writes ignored | NEW | [M] nopw |
| 50C0 | write: watchdog reset (16 VBLANKs without a write resets the machine) | CONFIRMED | [M]. [P] E3: the ROM writes 50C0 every frame |
| 5000 | read: IN0 | CONFIRMED | [M], [P] E1 |
| 5040-507F | read: IN1 (every address in the range gives IN1) | CONFIRMED | [M] mirror 0xAF3F, [P] E1 |
| 5080-50BF | read: DIP switch 1 (every address gives DSW1) | CONFIRMED | [M], [P] E1 |
| 50C0-50FF | read: DIP switch 2 (all bits unused) | NEW | [M]. [P] E1: the ROM never reads it, so its value cannot matter |

Behaviour of reads of 4800-4BFF (RAM start question): [P] E10b: the ROM reads there 193 times in 4,500 frames, all from the routine at 205A (it looks at the tile under a moving object; positions beyond the board edge land in that range). The prototype returns 0, MAME's hardware returns 0xBF. The code only compares the byte with 0x1B, and 0 and 0xBF both fail that test. Experiment E14: with 0xBF returned, the picture and all of 4000-4FFF are identical at frames 700, 1500 and 4500. So the difference has no effect. In the port, return 0xBF to match the hardware.

## 3. The 8-bit output latch at 5000-5007 (only bit 0 of the data counts)
| Address | Meaning | Status | Evidence |
|---|---|---|---|
| 5000 | interrupt enable. 1 = VBLANK interrupt allowed | CONFIRMED | [M]; [P] E3: the ROM writes 0 then 1 once per frame |
| 5001 | sound enable | CORRECTED (Lomont's page 16 text says 50C0, which is the watchdog) | [M] latch bit 1; [P] E3 writes to 5001, none to 50C0 that look like sound control |
| 5002 | no connection | NEW | [M]. [P] E3: the ROM does write it; no effect |
| 5003 | flip screen | CONFIRMED | [M] |
| 5004, 5005 | start lamps: no such hardware | CORRECTED (Lomont lists lamps and lockout as present) | [M] "no boards are known to contain the output transistors". [P] the ROM writes them (E3) harmlessly |
| 5006 | coin lockout: no such hardware | CORRECTED | [M] |
| 5007 | coin counter | CONFIRMED | [M] |

## 4. Video
| Fact | Status | Evidence |
|---|---|---|
| Tile RAM layout (two bottom rows first, main area, two top rows) | CONFIRMED | same as the scan formula in section 1 |
| Tiles: 8x8, 2 bits per pixel, 16 bytes each, bit planes {0, 4}, x offsets {8*8+0..3, 0..3} | NEW | [M] `tilelayout` |
| Sprites: 16x16, 2 bits per pixel, 64 bytes each, 64 in pacman.5f | CONFIRMED | [M] `spritelayout` (Lomont's strip order 5,1,6,2,7,3,4,0 is what that layout expresses; compared formally in S19) |
| Sprite byte 0: bits 2-7 sprite number, bit 1 = flip X, bit 0 = flip Y | CONFIRMED as stated for the native (unrotated) picture | [M] `fx = byte & 1`, `fy = byte & 2` in MAME's native coordinates. The names X and Y swap when the picture is turned 90 degrees, which is why the sources look different. Decided in S21 against pictures |
| Sprite byte 1: palette number. Width is 5 bits (0 to 31) | CORRECTED (Lomont says 0-15; the prototype masks with 0x3F) | [M] `spriteram[offs+1] & 0x1f`. [P] E6: the ROM uses palette numbers 0x10 to 0x19 for sprites (0x14, 0x18, 0x19 seen), so 4 bits is not enough. No active sprite in 4,500 frames had bit 5 set (E12); the larger byte values E6 lists come from unused sprite slots and the start-up test |
| Tile palette: colour RAM & 0x1F | NEW | [M] `colorram & 0x1f`. [P] E11b: the colour RAM holds bit-5 values (20, 30, 60, 70, A0, B0, E0, F0) only in frames 212 to 252, the start-up memory test pattern; never during attract mode or the game. In MAME the 4A look-up PROM is indexed by 5 colour bits plus 2 pixel bits (128 entries); its upper half is not the same as the lower half, so masking with 0x3F instead of 0x1F would colour those test-pattern frames differently. Use & 0x1F in the port |
| Sprite colour 0 is transparent | CONFIRMED | [M] `transpen_mask(..., 0)` |
| Sprite order: lower numbers drawn on top | CONFIRMED | [M] draws sprite RAM offsets 14 down to 6, then sprites 2, 1, 0 |
| Sprite position: x = 272 - byte(5061+2n), y = byte(5060+2n) - 31 (native picture) | CONFIRMED (this settles inconsistency 3 for the prototype's variant A) | [M]. Lomont's numbers (visible range (31,16) to (239,256)) are in the rotated frame |
| Sprites are clipped to native x 16..271, y 0..223 | NEW | [M] `spriteclip`. The prototype does not clip. Needed in S21 |
| Sprites also plotted at x-256 to give wrap-around | NEW | [M] second draw call. Needed in S21 |
| Sprites 0, 1, 2 are drawn one pixel further along (xoffsethack = 1 for Pac-Man) | NEW | [M] `m_xoffsethack = 1`. The prototype lacks it. Needed in S21; see U2 |
| Colours: 3-bit red and green from PROM bits 0-2 and 3-5 (resistors 1000, 470, 220 ohm, weights 0x21, 0x47, 0x97), 2-bit blue from bits 6-7 (470, 220 ohm, weights 0x51, 0xAE) | NEW | [M] colour decode from 82s123.7f |
| Look-up PROM 82s126.4a: 4 bits per entry, `prom[i] & 0x0F`, 64 palettes of 4 entries | NEW | [M]; [P] E13: first palettes are 00 00 00 00 / 00 0f 0b 01 / ... |

## 5. Sound
| Fact | Status | Evidence |
|---|---|---|
| 3 voices, 4-bit samples | CONFIRMED | [M] |
| Sound clock = CPU clock / 32 = 96 kHz | CONFIRMED | [M] 18.432 MHz / 6 / 32 |
| Waveforms: Lomont says 16 waveforms in two PROMs | CORRECTED | [M] only 82s126.1m is used (3-bit waveform select = 8 waveforms of 32 samples); 82s126.3m is marked "Timing - not used". The port needs only 1m; keep 3m checked in the ROM check |
| Register layout: voice 1 accumulator 5040-5044, waveform 5045, frequency 5050-5054, volume 5055; voice 2 accumulator 5046-5049, waveform 504A, frequency 5056-5059, volume 505A; voice 3 accumulator 504B-504E, waveform 504F, frequency 505B-505E, volume 505F | CONFIRMED | [M] address comment block, same layout (voice 1 has 5 frequency nibbles, the others 4) |
| Sound is enabled only when 5001 is 1 | CONFIRMED | section 3 |
| Mixing details (accumulator bits used, volume multiply, output scaling) | UNRESOLVED | U3 |

## 6. Input ports (bit meanings, [M]; the ROM reads 5000, 5040 and 5080 only: [P] E1)
IN0, read at 5000. A bit reads 0 when the control is active (active low); the idle value is 0xFF. Bit 4 is the Rack Test switch (a cheat, off = 1).
| Bit | Meaning |
|---|---|
| 0 | joystick 1 up |
| 1 | joystick 1 left |
| 2 | joystick 1 right |
| 3 | joystick 1 down |
| 4 | Rack Test (advance level; 1 = off) |
| 5 | coin slot 1 |
| 6 | coin slot 2 |
| 7 | service coin |

IN1, read at 5040 (active low):
| Bit | Meaning |
|---|---|
| 0 to 3 | joystick 2 up, left, right, down (cocktail table) |
| 4 | service mode switch |
| 5 | start 1 player |
| 6 | start 2 players |
| 7 | cabinet type (1 = upright, 0 = cocktail) |

DIP switch 1, read at 5080 (the factory default is 0xC9, which the prototype already uses):
| Bits | Meaning | Default |
|---|---|---|
| 0-1 | coinage: 00 free play, 01 one coin one credit, 10 one coin two credits, 11 two coins one credit | 01 |
| 2-3 | lives: 00 one, 01 two, 10 three, 11 five | 10 (3 lives) |
| 4-5 | bonus life at: 00 10,000, 01 15,000, 10 20,000, 11 none | 00 |
| 6 | difficulty: 1 normal, 0 hard | 1 |
| 7 | ghost names: 1 normal, 0 alternate | 1 |

Note on the 0xC9 default: 0xC9 = 1100 1001 binary, which gives coinage 01, lives 10, bonus 00, normal difficulty, normal names, matching the table. The value tables above were read from MAME's input definition (coinage 11 = two coins one credit, lives 11 = five); the S23 test confirms them by running the ROM.

## 7. The three known inconsistencies, settled
1. Sound enable: 5001 (latch bit 1). 50C0 is the watchdog. Lomont's p16 sentence is a slip. CORRECTED.
2. RAM start: work RAM begins at 4C00. 4800-4BFF is empty space that reads 0xBF. Tested: no effect on the game (E14). CORRECTED.
3. Sprite X and Y: byte at 5061+2n is X, byte at 5060+2n is Y, formula in section 4. Flip bits are an orientation naming difference. CONFIRMED (prototype variant A).

## 8. UNRESOLVED facts (3), each with the experiment that settles it
- U1. How a VBLANK that arrives while interrupts are disabled is handled. MAME holds the request until interrupts are enabled again; the prototype drops it, and also delivers it after the whole frame rather than at line 224. Experiment (S18): in the prototype, count how many frames end with the enable bit at 0 (E3 shows the ROM clears and sets it each frame, so check the order within the frame); then change the delivery to cycle 43,008 with a held request and compare RAM after 4,500 frames with the current prototype. If they differ, the port follows MAME; if they match, both are equivalent for this ROM.
- U2. The one-pixel offset for sprites 0, 1, 2 (`xoffsethack`). It is in MAME for Pac-Man; the prototype does not have it. Experiment (S21): render the same frames with and without the offset next to the real arcade look (the Dossier screenshots and the user's reference images) and keep the variant that matches. Until then the port follows MAME.
- U3. Sound mixing details (which accumulator bits select the sample, how volume scales, final output level). Experiment (S25): build the sound chip from MAME's formulas and compare its output, sample by sample, against a second independent description (Lomont p15 and the MAME sound code), then listen with the user (S26).
