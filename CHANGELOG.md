# Changelog

Version numbers follow `major.minor.patch`. The exe's version is set in `dist/res/app.rc` and `tools/package.sh`.

## Unreleased
- (nothing yet)

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
