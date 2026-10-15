# What the Python prototype (tools/analysis) established
Written during audit step S05, 2026-10-06, from reading harness.py/render.py and re-running them. The prototype was built by an earlier working session; it is an analysis tool and the intended oracle for the C++ port. It is NOT the port.
- Runs the 16 KB program ROM on the third-party Python package `z80` 1.2.0 (Ivan Kosarev, MIT, github.com/kosarev/z80; the Python module wraps a C++ core). Install: `pip install z80 pillow numpy --break-system-packages`.
- Board model in harness.py: ROM 0000-3FFF; RAM and video RAM from 4000; I/O at 5000-50FF (reads: IN0=5000, IN1=5040, DIP=5080 default 0xC9, 50C0); interrupt enable = bit 0 of 5000; IM2 vector byte from OUT (0),A; 50688 cycles per frame; interrupt delivered by hand at end of each frame if enabled.
- Observed: first interrupt handler address is 0x3000 (PC after frame 0); vector byte is 0xFA at boot and 0xFC by frame 1500 (the ROM rewrites it).
- Observed: attract mode reaches the "CHARACTER / NICKNAME" screen around frame 700 and the "10 PTS / 50 PTS / (c) 1980 MIDWAY" screen by frame 1500; the demo maze with GAME OVER appears later (see docs/evidence/prototype/contact2.png).
- render.py implements: palette decode from 82s123.7f + 82s126.4a (resistor weights 0x21/0x47/0x97 and 0x51/0xAE), 8x8 tile decode, 16x16 sprite decode with MAME-style bit offsets, tilemap addressing (tile_offset), sprite placement with a `variant` switch, then 90 degree rotation to 224x288.
- Evidence level: the rendered screens look like genuine Pac-Man screens (text, ghosts, colours, maze). That is visual evidence only; no pixel comparison against an external reference yet.
- Not established: input bit assignments (coin_lowactive.png / coinstart.png / start.png suggest coin and start were tried, but no log says what the game did), sprite flip bit order and coordinate variant, sound, anything about game logic.
- Hooks available for later steps: Board.watch_writes(addr,val,pc) for RAM write tracing; Board.io_writes list of (frame, addr, value); helper hold() for pressing active-low input bits; screen_text() dumps the tile text screen.
- Golden frame hashes (SHA-1 of RGB bytes of the 224x288 render): frame 700 b63cb26056e76c05f084894d7cb3f86c78f2e813; frame 1500 3d0eb894d703684b468bfe8013ebdf3dfe5c46de. Guard test: tests/test_prototype.py.
## Leads found by search (not examined, not verified)
- RAM map: http://www.euro-arcade.de/files/pacman_mm/pacman_mm.htm ("Pac-Man Memory Map").
- Possible C/other re-implementations to compare against in later steps: github.com/mburkley/pacman-c, github.com/Justin-Credible/pac-man-emulator, github.com/frisnit/pacman-emulator.
