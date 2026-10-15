# Changelog

Version numbers follow `major.minor.patch`. The exe's version is set in `dist/res/app.rc` and `tools/package.sh`.

## Unreleased
- Repository prepared for GitHub (step S37): documentation (README, BUILDING, MODDING, ARCHITECTURE, CONTRIBUTING), licence (MIT for our own code), third-party licences (now also lists the z80 core), code layout formatted with clang-format (proved layout-only: every program file compiles to an identical object file), a one-line header comment on every file, `tools/export_github.sh` (clean export with a scan for ROM bytes).
- Readable game rules written and proved identical to the ROM routines (steps T01a to T01f in `src/logic/`); the game does not use them yet.

## 1.0.1 (step S36, release testing)
- First public Windows release of the program: arcade-accurate play of the player's own ROM, menus, save states and rewind, eight save slots, game options (the arcade DIP settings), controls remapping, Xbox / PlayStation 4 / PlayStation 5 pad support with vibration and light bar, statistics, personal bests, achievements, CRT effects (mask, glow, curve, presets), surround and cabinet look, ultrawide screens, Purist mode, first-run ROM finder with friendly messages, REPORT-A-PROBLEM.bat.

## Before 1.0 (summary; details in PROJECT_STATUS.md)
- S01 to S13: study of the arcade machine, ROM identification, disassembly and labelling, memory map, checks against the Pac-Man Dossier.
- S14 to S24: Z80 core, board, video, inputs, first playable build.
- S25 to S30A: sound, replay recorder, behaviour tests, level 256.
- S31 to S34 and X steps: display options, controls, pause/save/rewind, Purist mode, mouse, lightbar, statistics, achievements, CRT effects, cabinet look, ultrawide.
- S35: packaging of the Windows exe.
