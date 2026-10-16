# Settings table (X48, 2026-10-15; T02i added `rules`)

`settings.cfg` sits next to `controls.cfg` (the program's settings folder). The menu writes it; it can also be edited with Notepad. One comment line, then one `key value` line per setting (22 settings); a wrong value or unknown key is reported in the log and the other lines are still used. Checked by tests/test_integration.sh (round trip is byte-identical).

| Key | Values (first = standard) | Menu place | Locked in Purist? |
|---|---|---|---|
| purist | off, on | PURIST | (is Purist) |
| overlay | off, on | DISPLAY > OVERLAY F3 | yes |
| scanlines | off, on | DISPLAY > SCANLINES F5 | yes |
| fill | off, on | DISPLAY > SCALING F8 | yes |
| smooth | off, on | DISPLAY > SMOOTH PACING F6 | yes |
| window_frame | on, off | DISPLAY > WINDOW FRAME F7 | no |
| help_panel | off, on | DISPLAY > HELP PANEL | no |
| lives | 1, 2, 3, 5 (standard 3) | GAME OPTIONS > LIVES | no (the arcade DIP setting) |
| bonus | 10000, 15000, 20000, none | GAME OPTIONS > BONUS LIFE | no (the arcade DIP setting) |
| difficulty | normal, hard | GAME OPTIONS > DIFFICULTY | no (the arcade DIP setting) |
| ghost_names | normal, alternate | GAME OPTIONS > GHOST NAMES | no (the arcade DIP setting) |
| vibration | off, light, full | CONTROLS > VIBRATION | yes |
| pad_symbols | auto, xbox, ps4, ps5 | CONTROLS > PAD SYMBOLS | no |
| pause_on_focus_loss | on, off | CONTROLS > FOCUS LOSS PAUSE | yes |
| hints | auto, keyboard, pad | CONTROLS > HINTS | yes |
| lightbar | on, off | CONTROLS > LIGHTBAR | yes |
| run_summary | on, off | DISPLAY > RUN SUMMARY | yes |
| surround | black, cabinet, ambient | DISPLAY > SURROUND | yes |
| curve | off, light, strong | DISPLAY > CURVE | yes |
| glow | off, light, strong | DISPLAY > GLOW | yes |
| crt_mask | off, light, strong | DISPLAY > CRT MASK | yes |
| rules | original, readable | GAME OPTIONS > RULES | yes (Purist always runs the original ROM routines) |

(Menu names are as shown on screen; the CRT LOOK row sets mask, glow, curve and scanlines together from four presets and has no key of its own.)

## Keys (every key the program handles is listed in the help panel)
ESC menu, F1 pause, F2 save state, F4 load state, Backspace rewind, F3 overlay, F5 scanlines, F6 pacing, F7 window frame, F8 scaling, F9 keys page, F10 pad page, F11 full screen, F12 reset controls. The new extras after S34 (CRT mask, glow, curve, presets, surround, cabinet picture, Ambient) have no keys of their own: they live in the menu (Esc > DISPLAY) so that no new key could clash with the game's controls. Every pad command has a picture in the pad help panel.

## Rules (T02i)
`rules original` (the standard) runs the original ROM routines. `rules readable` runs the proved readable rules (src/logic, wired in src/rules): the game plays exactly the same, frame for frame (tests/test_rules_all.sh).
**rules.cfg** (next to PacArcadeComp.exe, else in the settings folder; `--rules-file PATH` for tests) changes the numbers of the readable rules without programming: 24 keys, one comment line before each (the menu item GAME OPTIONS > WRITE RULES FILE or `--write-rules-file PATH` writes the standard file; it never overwrites). Per-level keys may end in `.N` (level 1 to 21). Wrong lines are reported in the log and ignored.
* A file whose values all equal the standard ones changes nothing (not called MODDED). As soon as one value differs the game uses the readable rules (RULES shows FILE), the window title says `ASSISTED: MODDED`, and the game does not count for the saved high score, records, statistics-for-records or achievements (the assisted-game rule, assist.h reason MODDED).
* Purist mode ignores the file and always runs the original ROM routines.
* Keys: pacman_speed_percent, ghost_speed_percent, phase_time_percent, frightened_time_percent, elroy1_dots_left, elroy2_dots_left, pinky_house_dots, inky_house_dots, clyde_house_dots (all per level), dot_points, energizer_points, ghost1..4_points, cherry/strawberry/orange/apple/melon/galaxian/bell/key_points, extra_life_score (0 = the BONUS LIFE menu setting).
* Decisions (safest choices): speeds are given in percent of the original pixel steps (not raw bit patterns); the plain line of a key sets every level and a `.N` line wins whatever its place in the file; points are multiples of 10 (the arcade score ends in 0).
* Limits: the numbers shown by the ROM's own picture of a ghost score (the little 200/400/800/1600 sprites) stay the original pictures even when the points are changed; the ROM's time model is the original, so a mod that makes the readable rules do different work does not keep the arcade cycle counts.
