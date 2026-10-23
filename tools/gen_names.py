#!/usr/bin/env python3
"""N51/N52: writes src/game/state.h (the named layout of the work RAM blocks) and replaces the raw addresses in the native code by those names.
Usage: python3 tools/gen_names.py            (writes src/game/state.h only)
       python3 tools/gen_names.py --replace  (also rewrites the 0x4Dxx literals in src/game/*.h and src/rules/*.h code, comments untouched)
The names come from docs/notes/ram_map.md and the comments of src/logic/*.h (T01b-T02g); an address with no verified meaning is called byte_4Dxx."""
import re, sys, glob, os
os.chdir(os.path.join(os.path.dirname(__file__), '..'))
F = []   # (address, size, name)
def add(a, n, name): F.append((a, n, name))
G = ['blinky', 'pinky', 'inky', 'clyde']
for i, g in enumerate(G): add(0x4D00 + 2*i, 2, g + '_pixel')
add(0x4D08, 2, 'pacman_pixel')
for i, g in enumerate(G): add(0x4D0A + 2*i, 2, g + '_tile')
add(0x4D12, 2, 'pacman_demo_tile')
for i, g in enumerate(G): add(0x4D14 + 2*i, 2, g + '_step')
add(0x4D1C, 2, 'pacman_step')
for i, g in enumerate(G): add(0x4D1E + 2*i, 2, g + '_vector')
add(0x4D26, 2, 'pacman_wanted_step')
for i, g in enumerate(G): add(0x4D28 + i, 1, g + '_current_direction')
for i, g in enumerate(G): add(0x4D2C + i, 1, g + '_direction')
add(0x4D30, 1, 'pacman_direction')
for i, g in enumerate(G): add(0x4D31 + 2*i, 2, g + '_move_tile')
add(0x4D39, 1, 'pacman_tile_a'); add(0x4D3A, 1, 'pacman_tile_b')
add(0x4D3B, 1, 'choice_direction'); add(0x4D3C, 1, 'pacman_wanted_direction'); add(0x4D3D, 1, 'choice_reverse')
add(0x4D3E, 2, 'choice_ghost_tile'); add(0x4D40, 2, 'choice_target_tile'); add(0x4D42, 2, 'choice_next_tile'); add(0x4D44, 2, 'choice_distance')
add(0x4D46, 4, 'pacman_speed_normal'); add(0x4D4A, 4, 'pacman_speed_blue'); add(0x4D4E, 4, 'speed_elroy2'); add(0x4D52, 4, 'speed_elroy1')
for i, g in enumerate(G):
    b = 0x4D56 + 12*i
    add(b, 4, g + '_speed_normal'); add(b + 4, 4, g + '_speed_blue'); add(b + 8, 4, g + '_speed_tunnel')
add(0x4D86, 14, 'phase_limits')
add(0x4D95, 2, 'inactivity_limit'); add(0x4D97, 2, 'inactivity_counter')
for i, g in enumerate(G): add(0x4D99 + i, 1, g + '_tunnel_flag')
add(0x4D9D, 1, 'pacman_pause'); add(0x4D9E, 1, 'dots_at_last_move'); add(0x4D9F, 1, 'global_dot_counter')
add(0x4DA0, 1, 'blinky_active'); add(0x4DA1, 1, 'pinky_out'); add(0x4DA2, 1, 'inky_out'); add(0x4DA3, 1, 'clyde_out')
add(0x4DA4, 1, 'ghost_touched'); add(0x4DA5, 1, 'pacman_dying'); add(0x4DA6, 1, 'frightened')
for i, g in enumerate(G): add(0x4DA7 + i, 1, g + '_edible')
add(0x4DAB, 1, 'eaten_ghost_waiting')
for i, g in enumerate(G): add(0x4DAC + i, 1, g + '_dead')
add(0x4DB0, 1, 'level_setting')
for i, g in enumerate(G): add(0x4DB1 + i, 1, g + '_reverse')
add(0x4DB5, 1, 'extra_reverse')
add(0x4DB6, 1, 'elroy1'); add(0x4DB7, 1, 'elroy2')
add(0x4DB8, 1, 'pinky_dot_limit'); add(0x4DB9, 1, 'inky_dot_limit'); add(0x4DBA, 1, 'clyde_dot_limit')
add(0x4DBB, 1, 'elroy_dots_left1'); add(0x4DBC, 1, 'elroy_dots_left2')
add(0x4DBD, 2, 'frightened_time'); add(0x4DBF, 1, 'pacman_in_tunnel_row')
add(0x4DC1, 1, 'phase_number'); add(0x4DC2, 1, 'phase_counter'); add(0x4DC7, 1, 'choice_scratch'); add(0x4DC8, 1, 'siren')
add(0x4DC9, 2, 'random_seed'); add(0x4DCB, 2, 'frightened_counter'); add(0x4DCE, 1, 'blink_timer_1up')
add(0x4DD0, 1, 'ghost_chain'); add(0x4DD2, 2, 'fruit_position'); add(0x4DD4, 1, 'fruit_points')
for a in [0x4DC0, 0x4DD1, 0x4DD6]: add(a, 1, 'byte_%04X' % a)   # used by the code, meaning not established
ACTORS = F
# ---- N52: flow, scores, settings, dot arrays, sound requests (4E00-4EEF), the system block (4C80-4CC0) and the sprite staging bytes (4C02-4C3D)
FLOW = []
def fa(a, n, name): FLOW.append((a, n, name))
fa(0x4E00, 1, 'main_state'); fa(0x4E01, 1, 'reset_state'); fa(0x4E02, 1, 'intro_state'); fa(0x4E03, 1, 'credit_ready_state'); fa(0x4E04, 1, 'level_state')
fa(0x4E06, 1, 'byte_4E06'); fa(0x4E07, 1, 'byte_4E07'); fa(0x4E08, 1, 'byte_4E08'); fa(0x4E09, 1, 'current_player'); fa(0x4E0A, 2, 'difficulty_pointer')
fa(0x4E0C, 1, 'first_fruit_shown'); fa(0x4E0D, 1, 'second_fruit_shown'); fa(0x4E0E, 1, 'dots_eaten')
fa(0x4E0F, 1, 'pinky_dot_counter'); fa(0x4E10, 1, 'inky_dot_counter'); fa(0x4E11, 1, 'clyde_dot_counter')
fa(0x4E12, 1, 'died_this_level'); fa(0x4E13, 1, 'level'); fa(0x4E14, 1, 'lives'); fa(0x4E15, 1, 'lives_displayed')
fa(0x4E16, 30, 'pill_array_p1'); fa(0x4E34, 4, 'energizer_array_p1'); fa(0x4E38, 1, 'byte_4E38'); fa(0x4E42, 1, 'byte_4E42')
for a in (0x4E66, 0x4E67, 0x4E68, 0x4E69, 0x4E6A): fa(a, 1, 'byte_%04X' % a)
fa(0x4E6B, 1, 'coins_per_credit'); fa(0x4E6C, 1, 'partial_credit_coins'); fa(0x4E6D, 1, 'credits_per_coin'); fa(0x4E6E, 1, 'credits')
fa(0x4E6F, 1, 'lives_per_game'); fa(0x4E70, 1, 'two_player_game'); fa(0x4E71, 1, 'bonus_life_score'); fa(0x4E72, 1, 'cocktail_mode')
fa(0x4E73, 1, 'difficulty_pointer_lo'); fa(0x4E75, 1, 'ghost_names_mode')
fa(0x4E80, 3, 'score_p1'); fa(0x4E84, 3, 'score_p2'); fa(0x4E88, 3, 'high_score')
fa(0x4E8C, 16, 'sound_registers_shadow')
for i, n in enumerate(('voice1_effect_a', 'voice2_effect_a', 'voice3_effect_a')): fa(0x4E9C + 16*i, 16, n)
for i, n in enumerate(('voice1_effect_b', 'voice2_effect_b', 'voice3_effect_b')): fa(0x4ECC + 16*i, 16, n)
SYS = []
def sa(a, n, name): SYS.append((a, n, name))
sa(0x4C80, 2, 'task_queue_write_ptr'); sa(0x4C82, 2, 'task_queue_read_ptr'); sa(0x4C84, 1, 'frame_counter_up'); sa(0x4C85, 1, 'frame_counter_down')
sa(0x4C86, 1, 'timer_sixtieths'); sa(0x4C87, 1, 'timer_seconds'); sa(0x4C88, 1, 'timer_minutes'); sa(0x4C8A, 1, 'byte_4C8A')
sa(0x4C8B, 1, 'rng_a'); sa(0x4C8C, 1, 'rng_b'); sa(0x4C90, 48, 'timed_events'); sa(0x4CC0, 1, 'task_queue_start')
SPR = []
def pa(a, n, name): SPR.append((a, n, name))
pa(0x4C02, 8, 'sprite_data'); pa(0x4C0A, 1, 'byte_4C0A'); pa(0x4C0B, 1, 'pacman_colour'); pa(0x4C0C, 1, 'fruit_sprite_code'); pa(0x4C0D, 1, 'fruit_sprite_colour')
pa(0x4C20, 1, 'byte_4C20'); pa(0x4C22, 12, 'sprite_codes_copy'); pa(0x4C32, 12, 'sprite_coords_copy')
BLOCKS = [('Actors', 0x4D00, 0x4DD7, ACTORS), ('Flow', 0x4E00, 0x4EEC + 16, FLOW), ('System', 0x4C80, 0x4CC1, SYS), ('SpriteBytes', 0x4C02, 0x4C3E, SPR)]
seen = {}
for bn, lo, hi, fl in BLOCKS:
    fl.sort()
    for (a, n, _), (b, _, _) in zip(fl, fl[1:]): assert a + n <= b, hex(a)
    for a, n, name in fl:
        assert lo <= a and a + n <= hi, (bn, hex(a)); assert name not in seen, name; seen[name] = bn
files = [f for f in sorted(glob.glob('src/game/*.h') + glob.glob('src/rules/*.h')) if os.path.basename(f) not in ('attract_prog.h', 'state.h')]
LIT = re.compile(r'\b0x(4[CDE][0-9A-Fa-f]{2})\b')
def code_part(line):
    i = line.find('//'); return (line, '') if i < 0 else (line[:i], line[i:])
def owner(a):
    for bn, lo, hi, fl in BLOCKS:
        for s, n, name in fl:
            if s <= a < s + n: return name, a - s
    return None
o = ['// GENERATED by tools/gen_names.py (N51, N52): the named layout of the work RAM: the actors block (4D00-4DD6), the flow / scores / settings / dot arrays / sound requests block',
     '// (4E00-4EFF), the system block (4C80-4CC0: task queue, timers, random bytes, timed events) and the sprite staging bytes. The native code says `ram::pacman_pixel` instead of 0x4D08.',
     '// The structs are the same bytes as the machine (views for the tests): tests/test_names.sh proves every field sits at the address the ROM uses.',
     '// Names: docs/notes/ram_map.md, boot_flow.md and src/logic/*.h; byte_XXXX = used by the code, meaning not established. No ROM bytes here.',
     '#pragma once', '#include <cstddef>', '#include <cstdint>', 'namespace game {']
for bn, lo, hi, fl in BLOCKS:
    o.append('struct %s {   // packed bytes only: no padding, same layout as the machine, first byte = %04X' % (bn, lo))
    pos = lo
    for a, n, name in fl:
        if a > pos: o.append('    uint8_t gap_%04X[%d];' % (pos, a - pos))
        o.append('    uint8_t %s%s;   // %04X' % (name, '[%d]' % n if n > 1 else '', a)); pos = a + n
    o += ['};', 'static_assert(sizeof(%s) == %d, "size of %s");' % (bn, pos - lo, bn)]
o.append('namespace ram {   // addresses (constants; the same numbers the ROM uses)')
for bn, lo, hi, fl in BLOCKS:
    o.append('constexpr int k%sBase = 0x%04X;' % (bn, lo))
    for a, n, name in fl: o.append('constexpr int %s = k%sBase + (int)offsetof(%s, %s);' % (name, bn, bn, name))
o += ['}   // namespace ram', '// The blocks inside a copy or the real work RAM image (the first byte is the byte at 4C00).']
for bn, lo, hi, fl in BLOCKS:
    o.append('inline %s &%s_view(uint8_t *work_ram) { return *reinterpret_cast<%s *>(work_ram + (ram::k%sBase - 0x4C00)); }' % (bn, {'Actors': 'actors', 'Flow': 'flow', 'System': 'system', 'SpriteBytes': 'sprite_bytes'}[bn], bn, bn))
o.append('}   // namespace game')
open('src/game/state.h', 'w').write('\n'.join(o) + '\n')
print('fields', sum(len(b[3]) for b in BLOCKS))
if '--replace' in sys.argv:
    total = 0
    for f in files:
        out = []; ch = 0
        for l in open(f):
            code, com = code_part(l)
            def rep(m):
                a = int(m.group(1), 16); w = owner(a)
                if w is None: return m.group(0)
                pre = 'game::ram::' if f.startswith('src/rules/') else 'ram::'
                return pre + w[0] if w[1] == 0 else '(%s%s + %d)' % (pre, w[0], w[1])
            new = LIT.sub(rep, code)
            if new != code: ch += 1
            out.append(new + com)
        if ch:
            s2 = ''.join(out)
            if '#include "state.h"' not in s2 and 'game/state.h' not in s2:
                inc = '#include "../game/state.h"' if f.startswith('src/rules/') else '#include "state.h"'
                i = s2.find('#pragma once')
                s2 = s2[:i + 12] + '\n' + inc + s2[i + 12:] if i >= 0 else inc + '\n' + s2
            open(f, 'w').write(s2); total += ch; print(f, ch, 'lines')
    print('lines changed', total)
