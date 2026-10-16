# Example mods (no programming)

Each `.cfg` file here is a complete rules file. To try one: copy it next to `PacArcadeComp.exe`, rename the copy to `rules.cfg`, start the program. The window title says MODDED while a game runs, and a modded game never counts for the saved high score, records or achievements. Purist mode ignores the file. Remove or rename `rules.cfg` to go back to the arcade rules.

| File | What it does |
|---|---|
| `slow_ghosts.cfg` | ghosts at 60 percent speed |
| `long_blue_time.cfg` | the ghosts stay blue twice as long |
| `double_points.cfg` | dots, energizers and ghosts worth double; extra life at 20000 |

Every value is explained in [MODDING.md](../MODDING.md). Each of these files is loaded and checked by `tests/test_mods.sh`.
