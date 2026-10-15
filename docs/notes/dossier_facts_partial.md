# Facts read so far from The Pac-Man Dossier (UNVERIFIED; partial)
Source: https://pacman.holenet.info/ (Jamey Pittman, v1.0.27, 2015-08-11). Page text is 132,739 characters.
Read only through the web-fetch tool (shell blocked, proxy 403). The tool stops at character 100,000: **the last 32,739 characters have not been read** (continue with offset 100000). The tool also refused to reproduce whole tables, so every table value must be obtained by short, targeted questions or read from the ROM itself (step S12).
Chapters: 1 Welcome To The Machine, 2 Gameplay Details, 3 Maze Logic 101, 4 Meet The Ghosts, 5 Deciphering The Split Screen, Appendices (reference tables, easter eggs, hardware info, vintage guides, glossary, FAQ). Note: one fetch reported no dedicated "Hardware Info" appendix with specifications; treat Lomont as the hardware source.
## Facts captured
- Maze is 28x36 tiles; tile is 8x8 pixels. 244 dots in total: 240 small (10 points) + 4 energizers (50 points). Clearing the maze is worth 2,600 points (= 240x10 + 4x50, consistent).
- "100% speed = 75.75757625 pixels/sec" (= 1.25 pixels per frame at 60.606 frames per second).
- Eating a dot pauses Pac-Man for 1 frame; an energizer for 3 frames.
- Cornering: during a turn Pac-Man moves one pixel in the new direction for each pixel in the old, so he moves diagonally at effectively double speed.
- A summary line said Pac-Man speed goes from 80% on level 1 to 100% by level 5 then 90% from level 21 (Table A.1). Exact table not captured.
## Disagreement noted
- The Dossier often says "1/60th of a second" per frame; the hardware is 60.606 Hz (Lomont p2) and the Dossier's own 75.7575 px/s figure only works at 60.606 Hz. Treat "60" as rounding in the prose.
