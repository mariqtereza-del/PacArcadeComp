# Boot flow and interrupt cycle of the Pac-Man program ROM (step S09)

Sources: docs/disasm/pacman.asm (S08), docs/disasm/labels.txt (the labels and their evidence), docs/evidence/S09_prototype_trace.txt (Python prototype run, script tools/analysis/oracle_s09.py), docs/notes/hardware_facts.md (S07: ports and latch bits).
All frame numbers are the prototype's: frame N is the N-th slice of 50,688 CPU cycles, and the prototype delivers the VBLANK interrupt at the end of the slice (see S07, U1), so real hardware may differ by a part of a frame.
Confidence words: HIGH = code and prototype agree; MED = clear from the code, role inferred; LOW = guess from few instructions. Names are our working names; the ROM has none.

## 1. Frame 0 to the first screen draw
| Frame | What happens | Where | Evidence |
|---|---|---|---|
| 0 | Reset: interrupts off, interrupt page register I = 3F, jump to boot_init. | reset_entry 0000 -> 230B (HIGH) | prototype: 0000 reached once at frame 0 |
| 0 | boot_init: zero the eight output-latch bytes at 5000-5007 (interrupts off, sound off, no flip), fill video RAM with tile 40 (blank) and colour RAM with 0F, kicking the watchdog (write to 50C0) between pages; select interrupt mode 2, send vector byte FA to port 0, set 5000 = 1 (interrupts allowed), EI, HALT. | boot_init 230B-234A (HIGH) | prototype: first writes to 5000 are 0 at 2312 then 1 at 2349, both in frame 0; vector byte FA at frame 0 |
| 0 -> 1 | The first interrupt (end of frame 0) goes through the table at I*256 + FA: the word at 3FFA is 3000. | power_on_self_test 3000 (MED) | prototype: PC = 3000 at the end of frame 0; first fetch at 3000 in frame 1 |
| 1-252 | Power-on self-test: ROM checksum loop (3000-3041), RAM test with patterns (3042-30BC; the prototype shows 4E00 changing to test values while it runs), clean-up (30BD-3173). | 3000-30FF | prototype: PC stays inside 3000-30FF for these frames |
| 253 | Test-mode screen routine (3174-3246): sets the latches 5001-5006 to 1, flip screen 5003 to 0, sets the vector byte to FC (SUB 4 gives FC from 0), enables interrupts, reads IN0, IN1 and the DIP switches (5000, 5040, 5080) and prints status messages through the text task 2C5E; with the service switch (IN1 bit 4) not pressed it falls through and ends at 3246 with 5000 = 0 and DI. | 3173-3246 | prototype: vector byte FC set in frame 253; 5000 = 1 at 3196 and 5000 = 0 at 324A, both frame 253 |
| 253-295 | Writes tile data taken from a table at 3AE2 into video memory (3253-327A), then calls the delay routine 32ED eight times (each a 0x2800-iteration countdown, about 5 frames). | 3253-3285 | prototype: PC sits at 32F3-32F6 (the delay loop) for frames 254-295 |
| 296 | The code waits while the service switch is held (3286-328E, IN1 bit 4 reads 0 when held), then tests IN1 bits 5 and 6 (start buttons, active low: 1 = not pressed). Neither start button pressed, as in this run: jump to main_init at 234B (3290-3295). A start button pressed would instead continue into the longer test routines at 3298-32EA (more delays, an input test, the screen at 32DC) and reach 234B from 32A4 or 32EA. | jump at 3295 to 234B | prototype: 234B first executed in frame 296 with no input pressed. The start-button branch was not run, so it is described from the code only |
| 296-297 | main_init: stack = 4FC0; zero the latches 5000-5007, RAM 4C00-4EF7 and 5040-507F (sound registers); clear colour RAM (240D) and video RAM (23ED, variant 0); set the task queue write pointer 4C80 and read pointer 4C82 to 4CC0 and fill 4CC0-4CFF with FF (= empty). | main_init 234B-2386 (HIGH) | prototype: the fill helper at 0008 first runs in frame 296; colour clear (240D) at 297; video clear (23ED) at 298 |
| 298 | Interrupts allowed: 5000 = 1, EI, fall into task_loop at 238D, which polls the queue. | 2387-238C | prototype: 5000 = 1 at 238C in frame 298 |
| 299 | First real VBLANK interrupt: vector byte is now FC, the word at 3FFC is 008D = vblank_interrupt_handler. The handler's first run schedules the first tasks (via 0028 and 0030, first used in frames 299 and 301). | 008D | prototype: 008D first fetched in frame 299, 1,201 times by frame 1499 |
| 301 | update_player_labels (02FD) writes the tile codes for "1UP" (43D8-43DA) and "2UP" (43C5-43C7). | 02FD | prototype: those writes at 036D-0382, frames 301-302 |
| 302 | First non-blank tiles in the playing-field area: 18 tiles, the screen text dump shows "1UP", "2UP", "CHARACTER / NICKNAME" and a score digit. This is the first screen draw after boot. | task_loop -> task table 23A8 -> text tasks | prototype: tiles counted in video RAM 4040-43BF; text via screen_text() |

## 2. The main loop between interrupts (task_loop at 238D)
Outside interrupts the CPU runs a polling loop (about 1.1 million passes in 1,500 frames):
1. Read the queue read pointer (word at 4C82, queue at 4CC0-4CFF).
2. If the byte there has its top bit set (FF = empty) go round again.
3. Otherwise this is a task id: mark it consumed (FF), read the parameter byte after it (also marked FF), advance the pointer by 2 (low byte wraps from FF to C0 = 4CC0), push 238D as the return address and jump through the 32-entry table at 23A8 using the id as the index (the parameter is in B).
Tasks are queued by RST 28h (queue_task_inline) which stores two inline bytes with task_queue_push (0042): task id then parameter, at the write pointer 4C80.
Timed events are queued by RST 30h (schedule_timed_event_inline), which fills a free three-byte slot at 4C90-4CBF; the interrupt handler counts them down (run_timed_events 0221) and dispatches through the 10-entry table at 0247 when a count reaches zero.
Prototype: RST 28h is first used in frame 299 (227 times in 1,500 frames), RST 30h in frame 301 (5 times).

## 3. One full interrupt cycle (vblank_interrupt_handler, 008D)
Order of operations (code at 008D-01DB; the prototype run of frame 600 enters the labelled routines in exactly this order, see docs/evidence/S09_prototype_trace.txt):
1. Push AF, write to 50C0 (kicks the watchdog), write 0 to 5000 (interrupts off), DI, push the other registers (008D-009B).
2. Copy 16 bytes from the RAM shadow 4E8C-4E9B to 5050-505F (sound frequency and volume registers) (009D-00A6).
3. Choose the waveform byte for each of the three voices (5045, 504A, 504F): from the group-B sound state (4ECF, 4EDF, 4EEF) if that group's flag byte (4ECC, 4EDC, 4EEC) is non-zero, else from group A (4E9F, 4EAF, 4EBF) (00A8-00D2).
4. Copy 28 bytes of sprite data from 4C02 to 4C22 and rotate the code/flip bytes of six sprites left by two bits (00D5-0111), optionally swap entries depending on 4DD1 and 4DA4/4DA6 (0114-0175; meaning decided in S10-S11).
5. Copy 12 bytes from 4C22 to 4FF2 (sprite number/colour bytes in sprite RAM) and 12 bytes from 4C32 to 5062 (sprite coordinates) (0176-018A). Prototype: frame 600 writes to 5040-50BF come from exactly these copy instructions (00A8, 00B7, 00C6, 00D5 are the sound/waveform copies, 018C region the sprite copies).
6. Calls in this order: update_frame_counters 01DC, run_timed_events 0221, game_mode_dispatch 03C8 (018C-0192).
7. If the game-mode byte 4E00 is non-zero: 039D, 1490, 141F, read_coin_inputs 0267, coin_counter_pulse 02AD, update_player_labels 02FD (0195-01AA). (4E00 is non-zero from frame 300 in the prototype, so these run every frame from then on.)
8. A small special case when 4E00 = 1 clears 4EAC and 4EBC (01AD-01B8).
9. sound_update_group_A 2D0C, then sound_update_group_B 2CC1 (01B9-01BC).
10. Restore registers; if 4E00 is non-zero and IN1 bit 4 (5040, the service switch) reads 0 jump to 0000 (restart into the test) (01BF-01D1).
11. Write 1 to 5000 (interrupts allowed again), EI, pop AF, RET (01D4-01DB).
Observed in frame 600: I/O writes 50C0 = FF at 0091, 5000 = 00 at 0095, 5006 = 01 at 0270 (inside read_coin_inputs), 5000 = 01 at 01D9; plus 31 writes to 5040-50BF from 00A8-00D5 and 018C.

## 4. Helper routines (RST vectors) as used by the code above
RST 08 fill_bytes, RST 10 read_table_byte, RST 18 read_table_word, RST 20 jump_table_dispatch (21 tables, used by task_loop, run_timed_events, game_mode_dispatch and others), RST 28 queue_task_inline (75 call sites), RST 30 schedule_timed_event_inline (23 call sites), RST 38 rst38_lockup (never reached; looks like a deliberate trap that stops kicking the watchdog).
Screen position helper: screen_pos_to_video_address (0065 -> 202D) and colour_address_of_tile (2052), first used at frame 664 in the prototype run.

## 5. Label table (all entries also in docs/disasm/labels.txt)
Names and confidence, in address order: reset_entry 0000 HIGH; fill_bytes 0008 HIGH; read_table_byte 0010 HIGH; read_table_word 0018 HIGH; jump_table_dispatch 0020 HIGH; queue_task_inline 0028 HIGH; schedule_timed_event_inline 0030 HIGH; rst38_lockup 0038 MED; task_queue_push 0042 HIGH; screen_pos_to_video_address 0065 MED; vblank_interrupt_handler 008D HIGH; update_frame_counters 01DC LOW; run_timed_events 0221 MED; game_mode_dispatch 03C8 MED; read_coin_inputs 0267 MED; coin_counter_pulse 02AD LOW; update_player_labels 02FD HIGH; colour_address_of_tile 2052 LOW; sound_update_group_A 2D0C MED; sound_update_group_B 2CC1 MED; boot_init 230B HIGH; main_init 234B HIGH; task_loop 238D HIGH; task_table 23A8 HIGH; clear_video_ram 23ED MED; clear_colour_ram 240D HIGH; power_on_self_test 3000 MED.
The evidence for each entry is in its line of labels.txt (code addresses read) plus the first-reached frames in docs/evidence/S09_prototype_trace.txt.

## 6. Things this step did not decide
- What each of the 32 tasks does (S11), what the RAM variables are (S10), the meaning of the counters in update_frame_counters, and whether the conditional sprite swap at 0114-0175 is for the power pellet or something else (S10-S11).
- The self-test is described from its inputs, outputs and flow; its inner loops are not analysed line by line.
- Real-hardware timing of the interrupt (S07 U1): the prototype delivers it at the end of the slice.
