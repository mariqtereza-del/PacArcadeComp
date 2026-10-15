# Changing the game: a guide for beginners

You do not need to be a programmer to try things here, but you do need patience: a change is "edit a text file, build, run, look". This page tells you honestly **what you can change today**, **how**, and **how to check that you did not break anything**.

## 1. First, get the program building
Follow [BUILDING.md](BUILDING.md). Once `sh tools/build_app.sh` works and the game starts from `build/app/pacman-port`, you are ready. Use your own `pacman.zip` in the `roms/` folder.

## 2. The honest picture: what is changeable and what is not

The game's rules (ghost behaviour, speeds, scoring) live inside the original game program (the ROM) that the emulator runs. **That program is not part of this repository and cannot be edited as source code.** So there are three layers:

| Layer | Where | Can you change it? |
|---|---|---|
| **The front end**: menus, controls, screen effects, surround, statistics, achievements, pad features, settings | `src/app/` | **Yes, today.** This is where to start. |
| **The machine model**: memory map, inputs, DIP switches, video, sound | `src/board/`, `src/video/`, `src/audio/` | Yes, but be careful: the tests compare it with the real machine. Change it only if you want to alter how the hardware itself behaves. |
| **The game rules** | inside the ROM | Not as source. We have rewritten the rules as readable C++ in `src/logic/` and **proved each piece identical** to the ROM routine it copies (random numbers, scoring, dots and fruit, ghost targeting, movement, mode timers). **The game does not use that code yet**, so editing `src/logic/` does not change how the game plays. It is there to read, to learn from, and as the base for a future step that switches the game to it. |

So "easy to modify the game" today means: **the player experience around the game** (looks, menus, controls, features) and **the arcade's own options** (lives, bonus life, difficulty, ghost names: Esc > GAME OPTIONS). Changing the *rules* needs the future step "use the readable code" (see section 6).

## 3. Safe first experiments (front end)

Each one: edit, run `sh tools/build_app.sh`, start `build/app/pacman-port`, look.

1. **Change a menu word.** Open `src/app/menu.cpp`, find `{"QUIT GAME", Cmd::Quit, -1}` and change the text inside the quotes (keep capital letters; the game's font has no lower case). Build and open the menu with Esc.
2. **Change an achievement's name or text.** `src/app/achievements.h` has a table like `{"first_fruit", "FRUIT SALAD", "EAT A BONUS FRUIT."}`. Change the second and third texts only; the first word is the key under which the unlock is stored.
3. **Make the CRT glow stronger.** In `src/app/crt.h`, `glow_gain` returns `0.3` (light) and `0.6` (strong). Change a number and look at Esc > DISPLAY > GLOW. (The test `tests/test_glow.sh` checks those numbers, so it will tell you the value changed; that is expected, update the test's expected number if you keep your change.)
4. **Default settings.** The list of settings and their standard values is in `docs/notes/settings_table.md`; the defaults are at the top of the `Settings` struct in `src/app/menu.h`.
5. **Default keys.** `src/app/controls.cpp` holds the standard key bindings and pad buttons.
6. **Your own surround.** No code needed: put a PNG named `surround.png` next to the program and choose Esc > DISPLAY > SURROUND > CABINET.

Tip: use your editor's "search in all files" to find any on-screen text you want to change.

## 4. Adding a feature that watches the game

`src/app/events.h` reads a few memory values after every frame and turns changes into events (dot eaten, ghost eaten, level cleared…). Statistics, achievements, vibration and the lightbar are all built on it, and none of them can change the game. A new feature of this kind (for example a new achievement or a new light colour) is the safest kind of modification: it can only *read*. The map of memory values is `docs/notes/ram_map.md`.

## 5. Checking your work

1. **No ROM needed:** `sh tests/run_rom_free.sh`. It builds and runs the tests that do not need the game (see [BUILDING.md](BUILDING.md)).
2. **With your ROM** (put `pacman.zip` in `roms/`):
   - `sh tests/test_purist.sh` and `sh tests/test_replay.sh`: prove the machine still behaves exactly like the original (the "hash" of the whole machine state, frame by frame, must stay identical).
   - `sh tests/run_all.sh`: everything (about 90 minutes). Run it before you share a change.
3. A change to the front end must **never** change those hashes. If it does, you changed the machine, not just the look.

## 6. The future: making the readable rules the real rules

`src/logic/README.md` explains the method: each piece (`random_number.h`, `scoring.h`, `dots_and_fruit.h`, `ghost_targeting.h`, `movement.h`, `mode_timers.h`) was written next to the ROM routine and run **side by side** with it (via `tests/logic_probe.h`) for millions of frames and random cases until there was no difference at all. The next stage is to let the machine call these functions *instead of* the ROM routines, again proving that nothing changes, and only then to start changing rules on purpose (for example "ghosts that never leave the house" or different speeds). That stage is not done; if you want to take it on, see [CONTRIBUTING.md](CONTRIBUTING.md).

## 7. Rules of the road
- **Never put the ROM, or anything made from it (disassembly listings, dumps of its bytes, screenshots of the game's graphics), in the repository.** `tools/export_github.sh` scans for this and refuses to finish if it finds any.
- Keep the layout tidy: `clang-format -i yourfile.cpp` (the settings are in `.clang-format`).
- Give every new file a one-line comment at the top saying what it is.
