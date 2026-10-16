# Changing the game: a guide

There are two ways to change how PacArcadeComp plays, and one way to change how it looks and feels.

| You want to… | You need | Go to |
|---|---|---|
| Change **numbers** of the game (speeds, times, points, extra-life score, ghost house) | a text editor (Notepad) | [Part 1](#part-1-change-the-rules-without-programming-rulescfg) |
| Change **what the rules do** (for example how Pinky aims) | a C++ compiler and patience | [Part 2](#part-2-change-the-rules-with-programming-srclogic) |
| Change the **menus, looks, controls, achievements** | a C++ compiler | [Part 3](#part-3-change-the-menus-and-the-look-srcapp) |

Everything is checked by tests, so you can always find out whether you broke something ([Part 4](#part-4-checking-your-work)).

---

## Part 1: change the rules without programming (`rules.cfg`)

### 1.1 Your first mod in five minutes
1. Unzip the program folder (the one with `PacArcadeComp.exe`). Your `pacman.zip` must be next to it, as always.
2. Open the folder `mods` of this project (or ask for the example files in the download). Copy the file `slow_ghosts.cfg` into the folder with `PacArcadeComp.exe`.
3. Click that copy once, press **F2** (rename), type `rules.cfg` and press **Enter**. (If you cannot see the ending `.cfg`: in the folder window click **View**, then tick **File name extensions**.)
4. Double-click `PacArcadeComp.exe`. Press **5** (coin) and **1** (start).
5. Look at the window's title bar: it says **PacArcadeComp - ASSISTED: MODDED**. The ghosts are clearly slower.
6. To go back to the real arcade game: rename `rules.cfg` to something else (or delete it) and start the program again.

What you just did: the game ran the **readable rules** (see Part 2) and put the numbers of your file into them. A game played with a changed file is a **modded game**: it never counts for the saved high score, the records or the achievements (the same "assisted game" rule that save states and rewind use), and Purist mode ignores the file completely.

### 1.2 Where the file can be
`rules.cfg` is looked for **next to `PacArcadeComp.exe`**, and if it is not there, in the program's settings folder (on Windows: `%APPDATA%\PacmanPort`, the folder with `settings.cfg`). The first one found is used.

### 1.3 Getting the standard file to start from
In the program: **Esc > GAME OPTIONS > WRITE RULES FILE**. It writes `rules.cfg` with every value set to the arcade's own value, with a comment line above each value, into the settings folder. (It never overwrites a `rules.cfg` that is already there.) Open it with Notepad. A file with only standard values changes nothing: the game is not called MODDED and plays exactly like the arcade.

From the command line the same file is written with `PacArcadeComp.exe --write-rules-file rules.cfg`.

### 1.4 The format
- One value per line: `key value`, for example `ghost_speed_percent 60`.
- A line that starts with `#` is a comment. Empty lines are fine.
- Keys marked **per level** may end in `.N` with N from 1 to 21 to change only that level (level 21 stands for 21 and every later level): `ghost_speed_percent.5 70`. A line without `.N` sets all levels; a `.N` line always wins over it, wherever it stands in the file.
- A line that is wrong (unknown key, not a number, out of range, points that are not a multiple of 10…) is **reported in the program's log and ignored**; all the other lines are still used.

### 1.5 Every value explained

**Speeds and times (per level)**

| Key | Standard | Allowed | What it does |
|---|---|---|---|
| `pacman_speed_percent` | 100 | 20 to 200 | Pac-Man's speed (normal and while the ghosts are blue) in percent of the arcade speed. 80 is slower, 120 is faster. |
| `ghost_speed_percent` | 100 | 20 to 200 | The ghosts' speed (normal, blue, in the tunnel, and Blinky's Cruise Elroy speeds) in percent. |
| `phase_time_percent` | 100 | 10 to 1000 | How long the scatter and chase phases last. 50 = half as long, so the ghosts attack sooner and longer. |
| `frightened_time_percent` | 100 | 0 to 1000 | How long the ghosts stay blue after an energizer. 0 = they are never frightened, 200 = twice as long. |

(A speed percent works by changing how many of the 32 steps of the speed pattern move one pixel. 100 leaves the arcade pattern untouched; in between values are spread evenly.)

**Blinky and the ghost house (per level; -1 means "the arcade value of that level")**

| Key | Standard | Allowed | What it does |
|---|---|---|---|
| `elroy1_dots_left` | -1 | -1, 0 to 244 | Blinky speeds up (Cruise Elroy 1) when this many dots are left. |
| `elroy2_dots_left` | -1 | -1, 0 to 244 | His second speed-up. |
| `pinky_house_dots` | -1 | -1, 0 to 244 | Dots Pac-Man must eat before Pinky leaves the ghost house (normally Pinky leaves at once). |
| `inky_house_dots` | -1 | -1, 0 to 244 | The same for Inky. |
| `clyde_house_dots` | -1 | -1, 0 to 244 | The same for Clyde. |

(The "common counter" used after Pac-Man loses a life and the inactivity timer keep their arcade values.)

**Points (the whole game; always a multiple of 10)**

| Key | Standard | What it is |
|---|---|---|
| `dot_points` | 10 | a dot |
| `energizer_points` | 50 | an energizer |
| `ghost1_points` … `ghost4_points` | 200, 400, 800, 1600 | the first to the fourth ghost eaten with one energizer |
| `cherry_points` | 100 | cherry |
| `strawberry_points` | 300 | strawberry |
| `orange_points` | 500 | orange |
| `apple_points` | 700 | apple |
| `melon_points` | 1000 | melon |
| `galaxian_points` | 2000 | Galaxian flagship |
| `bell_points` | 3000 | bell |
| `key_points` | 5000 | key |
| `extra_life_score` | 0 | The score that gives the extra life. 0 means "the BONUS LIFE setting of the menu" (10000, 15000, 20000 or none). Any other number (a multiple of 10 up to 990000) is exact. |

### 1.6 Three example mods
They are in the folder [mods/](mods/). Each one is a complete `rules.cfg`; copy one next to `PacArcadeComp.exe` and rename the copy to `rules.cfg`, as in 1.1.

1. [mods/slow_ghosts.cfg](mods/slow_ghosts.cfg): ghosts at 60 percent speed.
2. [mods/long_blue_time.cfg](mods/long_blue_time.cfg): the ghosts stay blue twice as long.
3. [mods/double_points.cfg](mods/double_points.cfg): dots, energizers and ghosts are worth double; the extra life comes at 20000.

Try your own: make a copy, change a number, start the program. Ideas: `pacman_speed_percent 130`; `phase_time_percent 50`; `pinky_house_dots.1 10` (Pinky waits for 10 dots on level 1 only); `frightened_time_percent.21 300` (a blue time on the late levels where the arcade gives almost none).

Each file in `mods/` is loaded and checked by `tests/test_mods.sh`: it must load without wrong lines and must change what its comment says (measured on the running game).

### 1.7 The menu setting RULES
**Esc > GAME OPTIONS > RULES** chooses between **ORIGINAL** (the standard: the arcade program's own routines) and **READABLE** (the same rules written as C++, see Part 2; the game plays exactly the same, frame for frame, which is proved by tests). While a `rules.cfg` with changed values is in use, the row says **FILE** and the readable rules are used automatically. The switch can be made at any moment.

### 1.8 Honest limits of Part 1
- The little pictures that show a ghost's score (200, 400, 800, 1600) when you eat one are drawn by the arcade program and keep their original numbers even if you change the ghost points; the score itself gets your points.
- The speeds are given as percent, not as the raw 32-bit patterns of the arcade. If you want exact patterns, use Part 2.
- Not changeable with the file: the maze, the ghosts' targeting rules (use Part 2), the order of the levels, the demo, the intermissions.

---

## Part 2: change the rules with programming (`src/logic/`)

### 2.1 How it fits together
- `src/logic/` holds the game's rules as readable C++: one file per kind of rule (`random_number.h`, `scoring.h`, `dots_and_fruit.h`, `ghost_targeting.h`, `movement.h`, `mode_timers.h`, `ghost_house.h`). Each piece was written next to the arcade routine it copies and **proved identical** to it, side by side, over millions of frames and random cases ([src/logic/README.md](src/logic/README.md)).
- `src/rules/` is the bridge. For each arcade routine that has a readable version, it holds an **adapter** (`rules.h`, with the timing model in `z80sim.h`): when the game reaches that routine, the adapter runs the readable C++ instead, and then leaves the machine exactly as the routine would have (memory, registers, the time it takes). 24 arcade routines are replaced this way. `rules_file.h` is the `rules.cfg` reader of Part 1.
- With **RULES ORIGINAL** (the standard) none of this runs: the arcade program runs untouched. With **RULES READABLE** the adapters run. Purist mode always uses the original routines.

So: **a change in `src/logic/` changes the game only with RULES READABLE** (menu, or `--rules readable`).

### 2.2 The loop
1. Build the program once: [BUILDING.md](BUILDING.md) (`sh tools/build_app.sh`).
2. Edit a file in `src/logic/`.
3. `sh tools/build_app.sh` again, start `build/app/pacman-port --rules readable`.
4. Play and look.

### 2.3 A worked example: Pinky aims 6 tiles ahead instead of 4
In the arcade game Pinky does not chase Pac-Man's tile but a tile **four steps ahead** of him. Let us make it six.

1. Open `src/logic/ghost_targeting.h`.
2. Find the function `pinky_target_task`. Inside it is this line:
   ```
   target = from_word((uint16_t)(as_word(tile_at(ram, kPacmanTileAddress)) + (uint16_t)(pac_step * 4)));
   ```
3. Change the **4** to **6**: `(uint16_t)(pac_step * 6)`. Save.
4. Build (`sh tools/build_app.sh`) and start with `--rules readable`. In the chase phase Pinky now ambushes from further ahead. (The F3 overlay draws its own copy of the ghost targets from the arcade rules and does not show your change.)

How we know this works: `tests/test_mods.sh` makes a **copy** of `src/logic/` in `build/mods/`, changes the 4 to a 6 in the copy exactly as above, and asks the copy for Pinky's target with Pac-Man at tile (100, 100) moving one step: the original answers (104, 100), the changed copy answers (106, 100), so the target moved by two tiles. The test also checks that the real `src/logic/` still says 4, so the repository stays the arcade game.

### 2.4 Checking that the original game is still identical
- **Rules off:** with RULES ORIGINAL the game is the arcade program: run `sh tests/test_replay.sh` and `sh tests/test_purist.sh`. Their frame-by-frame hashes must be identical to the stored ones whatever you did in `src/logic/`.
- **Rules on, unchanged logic:** `sh tests/test_rules_all.sh` plays millions of frames with every readable rule on and compares the whole machine with the arcade run: zero differences.
- **After you change `src/logic/`** these tests are *meant* to fail where your change matters (`tests/test_logic_*.sh` and `tests/test_rules_*.sh` compare with the arcade routine). That is how you see exactly what you changed. Keep a copy of the unchanged `src/logic/` folder to get back to the proved state.

### 2.5 The honest list of what is still original arcade code
Even with RULES READABLE these parts of the game are still the arcade program running on the emulator:
- The **game flow**: the attract mode and demo, the intro screens, the intermissions (cutscenes), the death sequence, the level-end flashing and the switch between states, the credit and start handling.
- **Drawing**: the maze, the score and high score text, the little Pac-Man life icons, the fruit pictures of the level (ROM 2BEA) and the sprite setup. The readable rules write only the game's data, not the pictures.
- **Sound**: all sound sequencing.
- A few small steps inside the replaced routines: Pac-Man eating the bonus fruit (ROM 19A1-19C4) and the redrawing after a score change.
- Rare cases: when one of the 24 replaced routines would run over the end of a video frame, the adapter lets the arcade routine run for that one call (less than one call in a hundred), which gives the same result.

---

## Part 3: change the menus and the look (`src/app/`)

Each one: edit, run `sh tools/build_app.sh`, start `build/app/pacman-port`, look.

1. **Change a menu word.** Open `src/app/menu.cpp`, find `{"QUIT GAME", Cmd::Quit, -1}` and change the text inside the quotes (capital letters; the game's font has no lower case). Open the menu with Esc.
2. **Change an achievement's name or text.** `src/app/achievements.h` has a table like `{"first_fruit", "FRUIT SALAD", "EAT A BONUS FRUIT."}`. Change the second and third texts only; the first word is the key under which the unlock is stored.
3. **Make the CRT glow stronger.** In `src/app/crt.h`, `glow_gain` returns `0.3` (light) and `0.6` (strong). (`tests/test_glow.sh` checks those numbers; update its expected number if you keep your change.)
4. **Default settings.** The list of settings and their standard values is in `docs/notes/settings_table.md`; the defaults are at the top of the `Settings` struct in `src/app/menu.h`.
5. **Default keys.** `src/app/controls.cpp` holds the standard key bindings and pad buttons.
6. **Your own surround.** No code: put a PNG named `surround.png` next to the program and choose Esc > DISPLAY > SURROUND > CABINET.

A new feature that only **watches** the game (a new achievement, a new light colour) is the safest kind: `src/app/events.h` reads a few memory values after every frame and turns changes into events; the map of memory values is `docs/notes/ram_map.md`.

---

## Part 4: checking your work

1. **No ROM needed:** `sh tests/run_rom_free.sh` builds and runs the tests that do not need the game.
2. **With your ROM** (`pacman.zip` in `roms/`):
   - `sh tests/test_mods.sh`: the example mods and the Pinky example.
   - `sh tests/test_rules_file.sh`: the rules file reader and the RULES setting.
   - `sh tests/test_purist.sh`, `sh tests/test_replay.sh`: the machine is still exactly the arcade machine.
   - `sh tests/run_all.sh`: everything (about 90 minutes). Run it before you share a change.
3. A change to the front end (Part 3) must **never** change the replay hashes. If it does, you changed the machine, not just the look.

## Rules of the road
- **Never put the ROM, or anything made from it (disassembly listings, dumps of its bytes, screenshots of the game's graphics), in the repository.** `tools/export_github.sh` scans for this and refuses to finish if it finds any.
- Keep the layout tidy: `clang-format -i yourfile.cpp` (the settings are in `.clang-format`).
- Give every new file a one-line comment at the top saying what it is.
- Modded games are marked MODDED and never count for records; please say so when you share a mod.
