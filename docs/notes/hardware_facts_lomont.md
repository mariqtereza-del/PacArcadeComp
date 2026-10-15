# Hardware facts from Lomont's Pac-Man Emulation Guide (UNVERIFIED: step S06 output, to be cross-checked in S07)
Source: https://www.lomont.org/software/games/pacman/PacmanEmulation.pdf ("Pac-Man Emulation Guide v0.1, Oct 2008").
Obtained 2026-10-06 through the web-fetch tool (the shell cannot reach lomont.org: proxy 403). The tool summarises, so numbers may be mis-transcribed. Page numbers are as reported by the tool.
Status of every line below: UNVERIFIED until S07.

## CPU, clock, screen, timing
- Z80A at 3.072 MHz [p2]. Interrupt mode 2 (IM 2) [p2, p17].
- Visible screen 28x36 tiles, 8x8 pixels, 4 colours per tile: 224x288 [p2].
- 60.61 frames per second, vblank every 16.67 ms [p2, p17]. (Prototype uses 50688 cycles per frame = 3,072,000 / 60.606.)
- Interrupt vector: the code writes a byte to port 0 (OUT (0),A); on VBLANK the CPU jumps via the address at I*256 + that byte [p17]. Example given: LD A,#CD; OUT (0),A.

## Memory map [Table 1, p3]
| Range | Meaning |
|---|---|
| 0000-3FFF | game ROM (four 4K chips) |
| 4000-43FF | video RAM: tile codes |
| 4400-47FF | video RAM: tile palette numbers |
| 4800-4FEF | RAM (as written in the guide) |
| 4FF0-4FFF | sprite number/flip + palette, 8 sprites x 2 bytes (write only) |
| 5000 | read IN0 (joystick 1, coin); write: interrupt enable (bit 0) |
| 5001 | write: sound enable |
| 5003 | write: flip screen (bit 0) |
| 5004-5007 | write: start lamps, coin lockout, coin counter |
| 5040-507F | read IN1 (joystick 2, start buttons); 5040-504F write: sound voice registers |
| 5050-505F | write: voice frequency and volume |
| 5060-506F | write: sprite x,y coordinates |
| 5080-50BF | read: DIP switches |
| 50C0-50FF | write: watchdog reset (counts each VBLANK on a 4-bit counter; resets when it reaches 16) [p18] |

## Video [p10-13]
- Tile RAM layout: first 64 bytes = two rows along the bottom of the screen (right to left, top to bottom, 32 tiles wide with 4 tiles off-screen at each end); next 0x380 bytes (offsets 40h-3BFh) = the 28x32 main area, top to bottom, right to left; last 64 bytes = two rows along the top. Palette RAM uses the same layout. [Fig 7, p10-11]
- Picture rotation: not stated in the text captured from Lomont. [source: prototype render.py rotates the native 288x224 picture 90 degrees to get 224x288; confirm in S07]
- Sprites: 16x16, 2 bits per pixel, 64 sprites in the 4096-byte sprite ROM (pacman.5f), 64 bytes each [p7]. Eight-pixel strips are arranged in the order 5,1,6,2,7,3,4,0 [p8].
- Sprite register byte 0: bits 2-7 sprite number, bit 1 X flip, bit 0 Y flip; byte 1: palette entry 0-15. Colour 0 is transparent. Lower-numbered sprites draw on top. X/Y give the upper-left corner; coordinates start at the lower right of the screen; visible range (31,16) to (239,256) [p13-14, Fig 10].

## Sound [p14-16]
- 3-voice, 4-bit Namco WSG. Waveforms: two 256-byte PROMs (82s126.1m, 82s126.3m) = 16 waveforms of 32 samples, values 0-15 [p9, p14].
- Clock = CPU clock / 32 = 96 kHz [p14].
- Voice 1: accumulator 5040-5044, waveform 5045, frequency 5050-5054 (20 bit), volume 5055. [Table 7, p14]
- Voice 2: accumulator 5046-5049, waveform 504A, frequency 5056-5059 (16 bit), volume 505A. [Table 7, p14]
- Voice 3: accumulator 504B-504E, waveform 504F, frequency 505B-505E (16 bit), volume 505F. [Table 7, p14]
- Per clock per voice: add frequency to accumulator; use the top 5 bits of the accumulator to index a sample in the selected waveform; multiply by volume; mix. [p15]
- Frequency formula for voice 1: V = (f x 32)/96000 x 2^15 = (4096/375) f. A440 is register value 12C6h. Note table: Table 8, p16.

## Input ports [p19]
- IN0 and IN1 are single read-only bytes; DIP switches are 8 switches memory mapped at 5080-50BF. (Bit meanings not captured yet; take them from MAME in S07.)

## Points that look inconsistent inside this single source (resolve in S07)
1. Table 1 puts sound enable at 5001, but the text on p16 says "register 50C0h is the sound enable register". 50C0 is also listed as the watchdog. Likely a text slip (or a transcription slip by the fetch tool).
2. RAM is given as 4800-4FEF; MAME (to be checked) maps real RAM at 4C00-4FEF with 4800-4BFF behaving differently (mirror/unused).
3. Sprite palette register address "4FF1h-4FFFh" is consistent with "odd bytes of 4FF0-4FFF", fine; sprite X/Y registers 5060-506F: confirm which byte is X and which Y (the prototype uses 5060+2n and 5061+2n and rotates; confirm in S21).
