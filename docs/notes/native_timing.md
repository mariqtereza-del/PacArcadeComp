# Native timing: where the interrupt lands inside a running task (step N06)

Measured by `tests/timing_study.cpp` (run `sh tests/test_timing_study.sh`; raw output in `docs/evidence/N06_timing.txt`). It runs the four coverage players A, B, C and D of `tests/coverage_player.h` on the player's ROM, 954,000 frames in all (A, B and C 300,000 frames each, D 54,000), and looks at every frame at the moment the vertical-blank interrupt arrives. Only addresses, counts and cycle numbers are written here; no ROM bytes. The routine names are the entry addresses of `docs/notes/rom_routines.md` (step N01).

## 1. The numbers

| What | Number |
|---|---|
| Frames measured | 954,000 |
| Interrupt not delivered (interrupts are off: the power-on tests and the start-up) | 24,135 |
| Interrupt delivered | 929,865 |
| ... while the program waits in the task-queue polling loop (238D-2392) | 913,693 (98.26 percent) |
| ... while a task is running (any other ROM address) | **16,121 (1.73 percent of the delivered, 1.69 percent of all frames)** |
| ... while a task is being taken off the queue (2395-23A7) | 0 |
| ... in the start-up wait (the HALT in the start code, address 234B after it) | 51 |
| The same, only the three play players A, B, C (900,000 frames) | **5,959 frames = 0.66 percent** (A 1,677 = 0.56, B 1,580 = 0.53, C 2,702 = 0.90 percent); the planning measurement was 471 of 60,000 = 0.79 percent |
| The same, player D (the service-switch test screens, 54,000 frames) | 10,162 frames = 18.82 percent. These frames are in the test-mode screens only, not in the game |
| Places (addresses) where the interrupt arrived inside a task | 201 in 11 routines. In play (A, B, C): 68 places in 7 routines. Test screens only: 133 places in 4 routines |
| Length of a frame | 50,688 to 50,704 cycles, mean 50,693.2: a frame ends at the first instruction boundary at or after 50,688 cycles; the excess (0 to 16 cycles) is thrown away, it is not carried into the next frame |
| Cost of the interrupt handler (008D to its RET) | cheapest 4,604, dearest 34,420, mean 12,910.6 cycles = 25.47 percent of a frame (929,763 frames) |
| Handler not finished at the end of the frame | 6 frames, all at the start of the reset code: the handler jumps to the reset when the service switch is pressed during play (address 01D1 area), so the machine restarts |
| Task queue (32 places) | deepest at an interrupt 11, deepest seen at the top of the polling loop 13; empty at 923,796 of the 929,865 interrupts |
| Timed-event slots (16) | at most 4 busy at an interrupt |
| Polling loop | one turn costs 37 cycles (four instructions at 238D, 2390, 2391, 2392); the interrupt lands on 238D in 260,297 frames, on 2390 in 394,392, on 2391 in 173,127, on 2392 in 85,877 |
| Where the cycles go (whole run, all frames) | polling loop and idle (routine 22FE): 69.87 percent; interrupt handler and what it calls: 24.82 percent; the tasks and the start-up code together about 5.3 percent |
| Interrupt stack pointer | between 4FB6 and 4FC0 |
| Distinct register sets (AF BC DE HL IX IY) saved by the interrupt | 9,601 in all, 1,444 among the frames with the interrupt inside a task |
| Runs of consecutive frames with the interrupt inside a task | 1 frame: 421 times, 2: 74, 3: 599, 4: 48, 5-8: 1,087, 9-16: 260, 17-64: 182; longest 61 frames |

## 2. What the numbers mean

1. **Why only about 1 frame in 60 is "inside a task".** The interrupt handler runs first in every frame (about 12,900 cycles) and the tasks of the queue run after it, in the same frame; they finish long before the end of the frame and the program then waits in the 37-cycle polling loop. A frame ends inside a task only when the tasks queued for that frame need more than the roughly 37,800 cycles that are left. That happens with long screen work: fills and clears (RST 08h loop), drawing the maze, putting the dots on the maze and the video-RAM scan.
2. **Every frame has a cycle-exact end, not only these.** The frame ends at an instruction boundary, so the exact cycles of the last instruction decide where the interrupt lands (0 to 16 cycles beyond 50,688). In the polling-loop frames the landing address is one of four and follows from the cycle on which the last task finished (37-cycle turns).
3. **The saved registers are part of the proof.** The interrupt pushes AF, BC, DE, HL, IX, IY and the return address on the stack (4FB6 to 4FC0). The stack lies inside the work RAM that `state_hash` (src/app/replay.cpp) hashes. So every frame, also the polling-loop frames, shows the register values at the landing place; BC, DE, IX and IY there are leftovers of the last task that ran. 9,601 different register sets were seen.
4. **The handler is a fixed quarter of every frame**, and its cost depends on the game state (4,604 to 34,420 cycles). The frames in which the handler costs more than about 37,000 cycles leave almost no time for tasks.
5. **Queue and timed-event table stay small** (at most 13 of 32 and 4 of 16), so the paths for "queue full" and "no free slot" (the one unexecuted instruction 005A, step N05) do not occur in play.

## 3. Every place where the interrupt arrived inside a task, in play (players A, B, C)

Columns: address; routine (entry address of the N01 map); frames in A+B+C; task numbers (hexadecimal) that were running; the cycles since this address last ran inside the same frame, at the interrupt; the shortest repeat of this address inside one frame over the whole run (this is the cost of one loop turn when the address is in a loop); what it is.

| Address | Routine | Frames | Task (frames) | Gap at the interrupt | Shortest repeat | Kind |
|---|---|---|---|---|---|---|
| 0008 | 0008 | 1047 | 01(799) 06(276) | 26-26 | 26 | loop |
| 0009 | 0008 | 357 | 01(289) 06(88) | 26-26 | 26 | loop |
| 000A | 0008 | 237 | 01(164) 06(77) | 26-26 | 26 | loop |
| 241F | 2419 | 58 | 02(58) | 181-222 | 181 | loop |
| 2420 | 2419 | 33 | 02(33) | 181-222 | 181 | loop |
| 2422 | 2419 | 65 | 02(65) | 181-222 | 181 | loop |
| 2429 | 2419 | 1 | 02(1) | 584-584 | 403 | inside the loop body |
| 242A | 2419 | 2 | 02(2) | 946-946 | 403 | inside the loop body |
| 242C | 2419 | 36 | 02(36) | 181-181 | 181 | loop |
| 242D | 2419 | 40 | 02(40) | 181-181 | 181 | loop |
| 242E | 2419 | 22 | 02(22) | 181-181 | 181 | loop |
| 242F | 2419 | 129 | 02(139) | 181-181 | 181 | loop |
| 2430 | 2419 | 25 | 02(25) | 181-222 | 181 | loop |
| 2433 | 2419 | 69 | 02(75) | 181-222 | 181 | loop |
| 2434 | 2419 | 11 | 02(11) | 181-181 | 181 | loop |
| 2436 | 2419 | 17 | 02(23) | 181-222 | 181 | loop |
| 2437 | 2419 | 5 | 02(5) | 181-222 | 181 | loop |
| 2439 | 2419 | 18 | 02(18) | 181-222 | 181 | loop |
| 243A | 2419 | 55 | 02(55) | 181-181 | 181 | loop |
| 243B | 2419 | 50 | 02(50) | 181-222 | 181 | loop |
| 243C | 2419 | 12 | 02(12) | 181-222 | 181 | loop |
| 243D | 2419 | 40 | 02(40) | 181-181 | 181 | loop |
| 243F | 2419 | 34 | 02(34) | 181-222 | 181 | loop |
| 2440 | 2419 | 84 | 02(84) | 181-222 | 181 | loop |
| 2442 | 2419 | 22 | 02(22) | 181-181 | 181 | loop |
| 2443 | 2419 | 20 | 02(20) | 181-181 | 181 | loop |
| 2444 | 2419 | 31 | 02(31) | 181-181 | 181 | loop |
| 2445 | 2419 | 59 | 02(59) | 181-181 | 181 | loop |
| 2459 | 2448 | 2 | 03(2) | 663-663 | 623 | inside the loop body |
| 245C | 2448 | 38 | 03(38) | 77-124 | 72 | inside the loop body |
| 245F | 2448 | 23 | 03(27) | 77-124 | 72 | inside the loop body |
| 2460 | 2448 | 33 | 03(33) | 77-77 | 72 | inside the loop body |
| 2461 | 2448 | 3 | 03(3) | 77-77 | 72 | inside the loop body |
| 2463 | 2448 | 2 | 03(2) | 77-77 | 77 | loop |
| 2465 | 2448 | 12 | 03(14) | 77-77 | 72 | inside the loop body |
| 2467 | 2448 | 4 | 03(4) | 77-77 | 72 | inside the loop body |
| 2468 | 2448 | 2 | 03(2) | 77-77 | 72 | inside the loop body |
| 2A38 | 2A35 | 382 | 13(388) | 108-108 | 88 | inside the loop body |
| 2A3B | 2A35 | 149 | 13(149) | 88-108 | 88 | loop |
| 2A3C | 2A35 | 199 | 13(199) | 108-108 | 88 | inside the loop body |
| 2A3E | 2A35 | 458 | 13(464) | 108-108 | 88 | inside the loop body |
| 2A3F | 2A35 | 181 | 13(181) | 88-108 | 88 | loop |
| 2A40 | 2A35 | 293 | 13(293) | 108-108 | 88 | inside the loop body |
| 2A42 | 2A35 | 196 | 13(196) | 88-108 | 88 | loop |
| 2A45 | 2A35 | 268 | 13(268) | 108-108 | 108 | loop |
| 2A47 | 2A35 | 321 | 13(331) | 108-460 | 108 | loop |
| 2A4A | 2A35 | 308 | 13(314) | 108-108 | 108 | loop |
| 2A4C | 2A35 | 176 | 13(176) | 108-108 | 108 | loop |
| 2A4F | 2A35 | 178 | 13(198) | 108-460 | 108 | loop |
| 2A50 | 2A35 | 121 | 13(121) | 108-460 | 108 | loop |
| 2A53 | 2A35 | 1 | 13(1) | 304-304 | 88 | inside the loop body |
| 2B80 | 2B80 | 1 | 10(1) | 357-357 | 149 | inside the loop body |
| 2B83 | 2B80 | 1 | 10(1) | 357-357 | 149 | inside the loop body |
| 2B88 | 2B80 | 2 | 10(2) | 357-357 | 149 | inside the loop body |
| 2B8D | 2B80 | 3 | 10(3) | 357-357 | 149 | inside the loop body |
| 2B8F | 2B8F | 2 | 10(2) | 357-357 | 178 | inside the loop body |
| 2B90 | 2B8F | 4 | 10(4) | 357-357 | 178 | inside the loop body |
| 2B94 | 2B8F | 2 | 10(2) | 357-357 | 178 | inside the loop body |
| 2B95 | 2B8F | 1 | 10(1) | 357-357 | 178 | inside the loop body |
| 2B96 | 2B8F | 1 | 10(1) | 357-357 | 178 | inside the loop body |
| 2B99 | 2B8F | 1 | 10(1) | 357-357 | 178 | inside the loop body |
| 2B9A | 2B8F | 1 | 10(1) | 357-357 | 178 | inside the loop body |
| 2B9F | 2B8F | 1 | 10(1) | 357-357 | 178 | inside the loop body |
| 2BA0 | 2B8F | 3 | 10(3) | 357-357 | 178 | inside the loop body |
| 2C03 | 2BEA | 1 | 10(1) | 357-357 | 357 | loop |
| 2C08 | 2BEA | 1 | 10(1) | 357-357 | 357 | loop |
| 2C0A | 2BEA | 2 | 10(2) | 357-357 | 357 | loop |
| 2C0F | 2BEA | 3 | 10(3) | 357-357 | 357 | loop |

Kinds: *loop* = the gap at the interrupt equals the shortest repeat, so one turn of the loop costs exactly that many cycles; *inside the loop body* = the address lies in a loop whose turns have different costs (the shortest repeat is the cheapest turn, the gap column shows what happened at the interrupt); *first pass* = the address had not run before in that frame.

The seven routines in play, with the plain meaning of the task and the cost of one loop turn:

| Routine | Task | Frames | What it does | Cost of one loop turn |
|---|---|---|---|---|
| 0008 (RST 08h fill) | 01 and 06 | 1,641 | fills a run of video or colour RAM with one value (clearing the screen); loop of three instructions 0008-000A | 26 cycles per byte |
| 2419 | 02 | 938 | draws the maze into video RAM from the compressed maze data (loop 241F-2445) | 181 cycles per step, 222 on the other path; two other places repeat after 403 |
| 2448 | 03 | 119 | puts the dots on the maze from the dot bits in RAM (loop 2457-2468) | 72 to 124 cycles per dot position |
| 2A35 | 13 | 3,231 | scans the video RAM 4040-43BF and blanks the dot tiles (loop 2A38-2A50) | 88 cycles per blank tile, 108 per other tile, 460 when a dot tile is blanked |
| 2B80, 2B8F, 2BEA | 10 | 30 | a drawing task seen only in player C (the rack and level change) | 149, 178 and 357 cycles per turn |

(2B80, 2B8F and 2BEA are three routines of one task; the table counts them together in the last row: 7 + 16 + 7 frames.)

## 4. The test screens (player D only)

The same study finds 133 more places, only in the service-switch test screens, in four routines: the text printer 2C5E (8,320 frames), the test-screen routine 2FAD (998), and the table readers 0010 and 0018 (304 and 412) that they call. In these the gap between two runs of an address is 889 to 2,548 cycles (the text printer prints a line character by character through RST 10h and RST 18h), and 7,349 or 7,364 cycles in the test-screen routine. They are listed in full in the evidence file. The test screens are part of the original program and part of the later power-on and service steps (N47); they do not occur in a normal game.

## 5. Decision gate: is there a frame that cannot be reproduced exactly?

**No frame is impossible to reproduce.** Every one of the 16,121 frames lies in one of 11 routines whose instruction costs are fixed by the Z80 timing and whose loop turn costs were measured above (section 3), so each can be stopped at an exact instruction boundary. This is a design claim; it is proved routine by routine in the later steps, each against the frame-by-frame run of the original ROM. **Nothing needs your decision now.** One question could come back later; it is written down in point D6.

## 6. Design for the native game

D1. **Exact cycle cost per routine.** Every native routine returns the number of Z80 cycles the original routine would have used for that call, computed from the same data (branches taken or not, loop counts, block copies). The costs come from the Z80 core's instruction timing, instruction by instruction; each routine's own step proves them against the ROM run (the cycle count of every call must be equal).

D2. **One frame = a cycle budget.** A native frame starts with the native interrupt handler (it returns its exact cost, 4,604 to 34,420 cycles), then runs the queued tasks one after the other, adding their costs; when the next task does not fit it is run only as far as the budget allows. The frame ends at the first instruction boundary at or after 50,688 cycles; the excess (0 to 16) is dropped exactly as the original does (section 1). The rest of the frame is the polling loop (37 cycles per turn, four landing addresses).

D3. **Long routines can stop in the middle and continue in the next frame.** The routines in section 3 (fill, maze drawing, dot placing, the video scan, the 2B80 family) are written as small state machines: a position (the address inside the routine) plus the loop variables, and a table of the cycle cost of each instruction of one loop turn. A routine given a budget runs whole turns while they fit, then runs the last turn instruction by instruction until the budget is used up; next frame it continues from the saved position. The shortest turn costs of section 3 (26, 181, 72, 88, 149, 178, 357) tell how many whole turns fit.

D4. **Registers at the landing place.** At a landing place the registers have values the original routine had at that instruction (they follow from the loop variables). The state machine of a long routine therefore also gives AF, BC, DE, HL, IX and IY for each stop position, and the native interrupt handler writes them to the stack exactly as the real interrupt does. In the polling-loop frames the stop position (one of four) and its registers follow from the cycle on which the last task ended; BC, DE, IX and IY are the exit values of that last task.

D5. **Queue and timed events.** The native game keeps the same 32-place task queue and 16-slot timed-event table with the same pointers and the same RAM addresses (they are read by the proof and by the later steps); they are small enough in play (13 and 4) that the full-queue path never runs.

D6. **The saved registers stay in the proof (the safest choice, recorded now).** The frame-by-frame proof keeps hashing the whole work RAM, including the stack bytes the interrupt writes (4FB6-4FC0). So every native task must leave BC, DE, IX and IY as the original routine leaves them. If this proves very costly in a later step, the only alternative is to leave those dead stack bytes out of the comparison; nothing in the game reads them (they lie below the stack pointer when the program is waiting). That would weaken the proof a little, so it would be brought to you as a decision first, and it is not done now.

D7. **Start-up and test mode.** The 24,135 frames without an interrupt (power-on RAM test, start-up) still last 50,688 cycles plus the excess; the 51 start-up frames end in the HALT; the six service-switch restarts leave the handler through the reset. These belong to the power-on step (N47).

## 7. Where the cycles go in the handler (whole run, exclusive cycles of the routine itself)

The mean per frame over the 929,763 handler runs: the handler body 008D 2,585; routine 1490 1,146; 0221 (timed events) 829; 2018 735; 2DEE 721; 1806 494; 0020 (jump-table dispatch) 420; 1017 355; 0010 346; 171D 316; 1B36 289; 2CC1 266 cycles. These are the routines whose cycle cost the native handler must reproduce first; each is ported and proved in its own step.
