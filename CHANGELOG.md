# Changelog

Version numbers follow `major.minor.patch`. The exe's version is set in `dist/res/app.rc` and `tools/package.sh`.

## 2.0.0 (steps N01 to N55, 2026-10-23)
- **The program no longer contains a Z80 emulator.** Everything below is why that is safe: the game is proved identical to the original, frame for frame.
- The game is now native C++: the original program's routines were rewritten one by one (`src/game/`) and each step was proved identical, frame by frame and cycle by cycle, against the original running on a Z80 emulator. Zero ROM instructions are executed any more (3.1 million frames checked).
- The Z80 emulator is no longer part of the program; it lives in `tests/oracle/` as a test tool. The ROM is read only for data tables (`src/data/`).
- The game state has names: `src/game/state.h` (work RAM) and `src/game/output.h` (screen, sprites, sound registers, latches).
- Documentation rewritten for the native engine (README, ARCHITECTURE, BUILDING, MODDING, src/logic/README.md, MACHINE_NOTES pointers).
- INVINCIBLE has its own CHEATS page in the menu (not in Game Options); a game played with it counts for no records, statistics or achievements.
- The plan (docs/PLAN.md and the plan page) now records the whole project, including the removed steps.
- Fixed: the game could crash when eating a ghost with the INVINCIBLE test option (N49c).

## 1.1.0 (steps T01a to T02k, 2026-10)
- The project is now called PacArcadeComp (program PacArcadeComp.exe; the GitHub repository and the downloads use the same name). Your settings folder %APPDATA%\PacmanPort is unchanged, so your settings, records and high score carry over.
- Readable game rules written and proved identical to the ROM routines (`src/logic/`), and the game can run them: Esc > GAME OPTIONS > RULES > READABLE replaces 24 arcade routines, frame for frame identical to the arcade run (3,000,000-frame proofs). RULES ORIGINAL (the standard) and Purist mode run the untouched arcade program.
- Modding: the `rules.cfg` file (speeds, times, Cruise Elroy and ghost house limits, points, extra-life score), Esc > GAME OPTIONS > WRITE RULES FILE, three example mods in `mods/`, MODDING.md with a worked C++ example. Modded games show MODDED in the window title and never count for the saved high score, records or achievements; Purist mode ignores the file.
- Repository prepared for GitHub: documentation (README, BUILDING, MODDING, ARCHITECTURE, CONTRIBUTING), MIT licence for our own code, third-party licences, `tools/export_github.sh` (clean export with a scan for ROM bytes).
- The Windows zip now also holds the `mods` folder and MODDING.txt. Still no ROM inside.

## 1.0.1 (step S36, release testing)
- First public Windows release of the program: arcade-accurate play of the player's own ROM, menus, save states and rewind, eight save slots, game options (the arcade DIP settings), controls remapping, Xbox / PlayStation 4 / PlayStation 5 pad support with vibration and light bar, statistics, personal bests, achievements, CRT effects (mask, glow, curve, presets), surround and cabinet look, ultrawide screens, Purist mode, first-run ROM finder with friendly messages, REPORT-A-PROBLEM.bat.

## Before 1.0 (summary; details in PROJECT_STATUS.md)
- S01 to S13: study of the arcade machine, ROM identification, disassembly and labelling, memory map, checks against the Pac-Man Dossier.
- S14 to S24: Z80 core, board, video, inputs, first playable build.
- S25 to S30A: sound, replay recorder, behaviour tests, level 256.
- S31 to S34 and X steps: display options, controls, pause/save/rewind, Purist mode, mouse, lightbar, statistics, achievements, CRT effects, cabinet look, ultrawide.
- S35: packaging of the Windows exe.
