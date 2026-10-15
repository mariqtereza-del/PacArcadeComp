# Settings table (X48, 2026-10-15)

`settings.cfg` sits next to `controls.cfg` (the program's settings folder). The menu writes it; it can also be edited with Notepad. One comment line, then one `key value` line per setting (21 settings); a wrong value or unknown key is reported in the log and the other lines are still used. Checked by tests/test_integration.sh (round trip is byte-identical).

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

(Menu names are as shown on screen; the CRT LOOK row sets mask, glow, curve and scanlines together from four presets and has no key of its own.)

## Keys (every key the program handles is listed in the help panel)
ESC menu, F1 pause, F2 save state, F4 load state, Backspace rewind, F3 overlay, F5 scanlines, F6 pacing, F7 window frame, F8 scaling, F9 keys page, F10 pad page, F11 full screen, F12 reset controls. The new extras after S34 (CRT mask, glow, curve, presets, surround, cabinet picture, Ambient) have no keys of their own: they live in the menu (Esc > DISPLAY) so that no new key could clash with the game's controls. Every pad command has a picture in the pad help panel.
