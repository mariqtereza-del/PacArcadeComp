# Frame rate (2.1.0): what it is and what it is not

**Display only.** The machine ticks 60.606 times per second (3,072,000 Hz / 50,688 cycles) whatever the FRAME RATE says. The rules, the sound, the scores, the saved states, the replays and every hash come from the machine, so they cannot change. tests/test_fps_hdr.sh proves it: the per-frame hash log of the same 7,273-frame replay is byte-identical at 60, 30, 144, unlocked, and with HDR on.

## The rates
| Setting | What the program does |
|---|---|
| 60 (standard) | exactly version 2.0.0: a picture on every machine tick, shown on every screen refresh (vsync on). |
| 30 | pictures built 30 times a second, vsync stays on, the picture is shown on every refresh (tear-free). |
| 75, 100, 120, 144 | if the screen refresh fits the rate (the rate is within 3 % of the refresh divided by a whole number, or is at least the refresh): vsync on, as often as that allows (a 60 Hz screen cannot show more than 60). If it does not fit (for example 120 on a 144 Hz screen): vsync off and a timer holds the rate; tearing is possible, the log says so. |
| UNLOCKED | vsync off, no waiting: as many pictures as the computer can draw. Tearing is possible, the CPU is fully used. |

The log line `Frame rate: ...` says which of these applies on this screen. Purist mode is always 60.

## Smoother movement above 60
Only the sprites move between two ticks, the tile layer (maze, dots, text) is the latest tick's. Each sprite is drawn between its place one tick ago and its place now (`render_sprites_lerp` in src/video/sprites.cpp), spread over the neighbouring pixels in proportion (like a linear texture filter), so a sprite that moves 0.8 pixels per tick moves a little on every picture instead of a whole pixel every second or third picture. Because the newest tick's place is only reached at the end of the interval, the picture is at most one tick (about 16.5 ms) behind the machine. A sprite is not smoothed when its picture changed or it moved more than 8 pixels in a tick (appearing, the side tunnel). At 60 and 30 nothing is smoothed and the picture is exactly the old one.

## Smooth pacing (F6)
Smooth pacing makes the machine tick once every N screen refreshes (180 Hz: 3). It is built for the classic 60, so it is used only at FRAME RATE 60; at any other rate the setting stays saved but the program ticks by the clock ("exact"), and the menu shows EXACT.

## Cost
At 144 or unlocked the picture is rebuilt on every refresh (a 224 x 288 copy plus eight small sprites). On the test machine (a software-drawn display, no graphics card) unlocked ran at about 650 pictures per second with the defaults.
