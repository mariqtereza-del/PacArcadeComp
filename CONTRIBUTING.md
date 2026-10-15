# Contributing

Thank you for looking at this project. You do not have to be a programmer to help.

## Reporting a problem (no programming needed)
1. Open the **Issues** tab of this repository and click **New issue**.
2. Say what you were doing, what you expected, and what happened (the questions in [docs/BUG_REPORT_TEMPLATE.md](docs/BUG_REPORT_TEMPLATE.md) help; a screenshot is useful, but see the rule below).
3. If the game misbehaved, start it with `REPORT-A-PROBLEM.bat` (in the release folder), reproduce the problem, close the game with Esc > QUIT GAME, and attach the `problem_report.txt` that appears next to it.

## The one rule that cannot be bent
**Do not upload the game's ROM, files made from it, or screenshots/recordings of the game's own graphics, and do not tell people where to find the ROM.** This keeps the project legal and open. Screenshots of this program's own menus are fine.

## Sending a change
1. Read [BUILDING.md](BUILDING.md), [ARCHITECTURE.md](ARCHITECTURE.md) and [MODDING.md](MODDING.md).
2. Make a small change that does one thing.
3. Run `sh tests/run_rom_free.sh`; if you have the ROM also `sh tests/test_purist.sh` and `sh tests/test_replay.sh`.
4. Format with `clang-format -i` (settings in `.clang-format`), keep the one-line header comment on every file.
5. Open a Pull Request describing what changed and which tests you ran.

## How this project works (so your change fits)
Everything is done in small steps, each with a test and a short evidence note in `docs/evidence/`. "It works" means "a test shows it". Claims in the documentation should point to such evidence.

## Ideas that need help
- Testing the Windows build with a game pad that is not Xbox or PlayStation.
- Building on Windows without WSL (see the untested notes in BUILDING.md) and reporting what worked.
- The next stage of the readable rewrite: letting the game use the proven code in `src/logic/` (see MODDING.md section 6).
