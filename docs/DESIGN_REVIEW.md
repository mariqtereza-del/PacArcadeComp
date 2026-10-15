# Design review (S33A): the user's corrections, sorted

Source: the user's message of 2026-10-12 (before S34). Docs only; no code in this step.

| # | What the user asked | Box | Where it is done |
|---|---|---|---|
| 1 | All function-key settings move into a pause menu like a modern game, and the keys keep working | B (behaviour of existing features) | S34: every key goes through the same action as its menu item (src/app/main.cpp `do_cmd`, src/app/menu.*) |
| 2 | A help panel on the side that lists the keys, so a player can look them up | C (new feature, needs no switch) | S34: the 128-pixel panel beside the picture shows every key while the menu is open; menu item Display > Help panel keeps it on all the time |
| 3 | Esc pauses the game and opens a conventional pause menu: save-state controls, options, every function so far, Purist mode | B | S34: Esc = pause + menu (Resume, Save/Load state, Game options, Controls, Display, Purist mode, Restart, Quit); Esc inside a page goes back; Esc on the main page resumes |
| 4 | The rest as in the plan (lives, bonus life, difficulty, ghost names, Purist proof) | plan | S34 as written |

Decisions Claude recorded (no questions asked of the user):
- Esc used to close the program. It now opens the menu; "Quit game" is in the menu and the window's close button still quits. Unattended test runs (`--scripted`, `--replay`) keep Esc = quit.
- F1 stays a plain pause (no menu) because the user already tested it; F1 inside the menu resumes.
- The menu is drawn in the game's own font (capital letters, digits, dash only), so it looks like part of the game.
- The pad can work the menu (d-pad moves, A selects, B goes back) once the menu is open; opening the menu is by keyboard (Esc), because Start on the pad is the game's Start button.
- DIP options (lives, bonus life, difficulty, ghost names) are the ROM's own switches; the ROM reads them only at power-on, so they apply when the machine is restarted ("Apply - restart", "Restart game"). Choices are remembered in settings.cfg.
- Purist mode = every addition off: overlay, scanlines, fill scaling, smooth pacing, one-button start, save states, rewind, saved high score. It restarts the machine when switched on or off. The pause, window frame, full screen, key remapping and the ROM's own game options stay available (they do not change the emulation).
- Box D (anything changing emulated behaviour): none requested.

Open design questions noticed (not decided, listed for later): volume control; pad buttons for pause and save; pausing when the window loses focus; showing the remap prompts inside the picture instead of the title bar.

## Second list (user, 2026-10-12, after the S34 test)
The user confirmed S34 works (docs/evidence/S34_user_report.txt) and asked for two more features before moving on:

| # | What the user asked | Box | Where |
|---|---|---|---|
| 5 | Key re-binding needs a menu where each command (moving Pac-Man, credits, start...) is shown with the keyboard binding and the gamepad button that go with it | B (existing feature, S32) | new step S34A |
| 6 | Support for Xbox (XInput) controllers, DualShock 4 and DualSense, with that controller's button symbols shown, so the player sees at once what is what | C (new feature) | new step S34B (needs S34A's screen to show the symbols in) |

Timing decision: now is the right time. Both change the controls code (S32) and the menu (S34), and nothing after them needs them; S35 packages a finished program, so adding them after S35 would mean re-packaging, re-writing the README and repeating the release tests. Order: S34A, then S34B, then S35. S35 now depends on S34B. Notes recorded: SDL already maps Xbox, DualShock 4 and DualSense pads onto one standard layout, so the work is reading the controller type and drawing the right pictures (done in code, no image files); only the user's PS4 pad can be tested on real hardware, the other types are tested with simulated controllers and will be recorded as simulated; Steam Input or DS4Windows can make a PlayStation pad look like an Xbox pad, and then the Xbox pictures are correct for what Windows reports.

## Third list (user, 2026-10-12, after the S34A test)
The user confirmed S34A (docs/evidence/S34A_user_report.txt: remapping works, DualShock 4, DualSense and a DS4Windows (Xbox-type) pad all work) and asked for:

| # | What the user asked | Box | Where |
|---|---|---|---|
| 7 | The menu must be openable with each of the controllers | B (extends S34) | new step S34B |
| 8 | Save state and load state must be reachable from the controllers too | B (S33's known limitation: F2/F4 keyboard only) | S34B |
| 9 | An icon showing which type of controller is in use | C (new feature) | S34C (controller types, icon and pictures; this step used to be called S34B) |

Decisions recorded: the work is split in two because pad access to the menu and the states does not need pictures and is more urgent, while the icon and the pictures need the controller-type detection. Order: S34B, then S34C, then S35 (S35 now depends on S34C). Pad defaults: MENU = right stick click (R3), the one button every pad has and no pad driver steals; Coin and Start keep Back and Start; the other new commands start unset and the menu already reaches Save and Load by pad. The user has a DualShock 4, a DualSense and DS4Windows, so all three kinds can be tested on real hardware in S34C. A pad run through DS4Windows is seen by Windows as an Xbox pad, so the icon will say XBOX for it; that is what the program can know.

## Fourth list (user, 2026-10-13, after the S34B trigger fix)
| # | What the user asked | Box | Where |
|---|---|---|---|
| 10 | Vibration on each of the 3 controller types (Xbox, DualShock 4, DualSense), at moments that are fun | C (new feature) | new step S34V |
| 11 | A list of other modern gaming sensibilities that could be applied to Pac-Man (no code, a list only) | - | the list is given in the chat reply; nothing is added to the plan until the user picks items |

Decisions recorded (S34V): the step does not depend on S34B's pad buttons (it adds src/app/haptics.h and a Vibration item on the Controls page) so it can be tested in either order. Vibration is off in Purist mode (the arcade had none). SDL's one rumble call serves all three pad types; what differs per type is the motor strength (starting values, tuned by the user's test) and the trigger motors, which only Xbox One pads have. A DualShock behind DS4Windows is an Xbox pad to Windows, so it gets the Xbox profile and whatever DS4Windows relays. The engine reads memory only (score, lives, level, credits, death animation, power-up flag, positions) and cannot change the game.

## Fifth list (user, 2026-10-13, after the S34V test)
The user confirmed vibration works perfectly (docs/evidence/S34V_user_report.txt; S34V VERIFIED) and asked for these features, split into many small steps: lightbar colour following the game; button pictures that switch between keyboard and pad; mouse in the menus; auto-pause on focus loss; multiple save slots with thumbnails; resume where you left off; start at any level; ghost-chain and fruit trainer; frame advance and slow motion; local achievements (RetroAchievements set for Pac-Man (Arcade), game 12192, as reference); per-run statistics; personal bests per level; saved and shareable replays; racing a ghost of your best run; daily challenges; online leaderboards and two-player netplay with rollback; CRT effects, cabinet artwork, ultrawide and HDR.

| Request | Box | Steps (X ids in tools/plan_data_x.py order) |
|---|---|---|
| Lightbar | C | X09 to X11 |
| Button pictures switch keyboard/pad | C | X07, X08 (needs S34C pictures) |
| Mouse in menus | C | X04, X05 |
| Auto-pause on focus loss | C | X06 |
| Save slots, thumbnails, resume | C | X12 to X14 |
| Start at any level (assist) | C | X15 research, X16 |
| Ghost-chain and fruit trainer (assist) | C | X19 research, X20, X21 |
| Slow motion, frame advance (assist) | C | X17, X18 |
| Statistics, personal bests | C | X22 to X24 |
| Achievements | C | X25 to X29 |
| Replays (record, play, share, verify) | C | X30 to X33 |
| Race a ghost of the best run | C | X34, X35 |
| Daily challenges | C | X36 to X39 |
| CRT effects, cabinet art, ultrawide, HDR | C | X40 to X47 |
| Online leaderboards, two-player online | C | X49 to X56, after release 1.0 |
Foundations added by me because most of the above need them: X01 game event reader, X02 test scenarios, X03 one shared "assisted game" rule, X48 integration check (S35 now depends on it).

Decisions recorded:
- Every extra is outside the machine (reads memory only) or an assist. An assist (save-state load, rewind, start-at-level, trainers, slow motion, frame advance) stops the game counting for high score, achievements, records, replays and challenges. Purist mode switches all of it off.
- Achievements follow the conditions of the public RetroAchievements Pac-Man (Arcade) set (counted game, player 1, 3 lives; fruit, ghost chains, cleared rounds, no-ghost play) with our own names and wording; nothing is sent to RetroAchievements.
- Cabinet artwork is drawn in code (nothing from a real cabinet); the player may add their own surround.png.
- Netplay: the arcade's two-player game is turn-based and the machine is exactly repeatable from power-on and inputs, so true rollback is not needed. The plan uses: the active player's inputs stream to the other side (who watches a few frames behind), plus a "live race" mode where both play their own game and see each other's faint Pac-Man. Rollback would only be needed for a simultaneous shared simulation, which Pac-Man does not have.
- Online steps come after release 1.0 because they need hosting, cost and privacy decisions (asked for in step X49); everything else comes before S35.
- HDR: SDL2 may not offer HDR output on Windows; step X47 checks and records a decision instead of assuming.
- Start-at-level, the trainers and the daily challenge each begin with a research step that proves on the real ROM what is possible, before any app code.
