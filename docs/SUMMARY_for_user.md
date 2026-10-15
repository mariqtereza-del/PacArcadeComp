# Pac-Man port: what we have learned so far (one page, plain words)

**Where we are:** the "study" part of the project is finished (steps S01 to S13). We now understand how the original arcade machine works well enough to start rebuilding it on Windows. The full technical version is `MACHINE_NOTES.md`; this page is the short one.

**What the machine is.** A small computer chip (a Z80, running 3 million steps a second) that draws a 224 x 288 picture 60.6 times a second, plays three sound voices, and reads a joystick, coin slots and two start buttons. Everything the game does lives in your 16 KB game program file; the other ROM files hold pictures, colours and sound shapes.

**What we have proved, with evidence you can open:**
- We can run your ROM in our reference program and get real Pac-Man screens, and we labelled the important parts of the game's code (`docs/disasm/labels.txt`, `docs/notes/boot_flow.md`, `docs/notes/gameplay_routines.md`).
- We have a verified map of 56 game memory values, such as score, lives, level and where each ghost is (`docs/notes/ram_map.md`).
- The game's own tables (ghost and Pac-Man speeds for every level, how long ghosts chase and scatter, how long they stay blue, bonus fruit and points) were read from your ROM and compared with the Pac-Man Dossier for levels 1 to 21. **Everything matched** (`docs/notes/table_comparison.md`).
- Every one of these checks runs automatically (`sh tests/run_all.sh`) and all pass.

**Things where sources disagreed, and what we decided:** an older guide put the game memory in the wrong place and named the sound switch wrongly; we follow the arcade emulator MAME and our own tests. The Dossier says "60 frames a second", the real machine is 60.6; the numbers still match.

**What is not known yet (honest list):**
- Exactly how the sound chip mixes its three voices (to be settled in the sound steps, with you listening).
- One-pixel details of where ghosts and Pac-Man are drawn (settled later by comparing pictures).
- One odd case where a ghost (Clyde) leaves the ghost house earlier than the rule we found explains.
- How long the bonus fruit stays on screen (9-10 seconds in the Dossier; not yet read from the ROM).
- All game-behaviour evidence so far comes from our reference program, not from a second independent emulator.

**Your part:** nothing right now. Next comes building the Windows program step by step, each step tested before the next.
