#!/usr/bin/env python3
"""Writes tests/data/level_expect.txt (and level_modes.txt: n then the seven cumulative mode-phase frame counts of that level's speed record) from tests/data/rom_tables.json (the ROM-decoded per-level table made in S12).
One line per level 1..21: n pac pac_fright elroy2 elroy1 ghost ghost_fright tunnel fright_ticks house0 house1 house2 elroy1_dots elroy2_dots
-1 means 'not applicable on this level' (frightened speeds when there is no frightened time). Levels above 21 use line 21."""
import json, os
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
d = json.load(open(os.path.join(root, "tests/data/rom_tables.json")))["rom"]["levels"]
out = []
for n in range(1, 22):
    L = d[str(n)]
    fr = int(round(L["fright_sec"] * 120))
    pf = L["pac_fright"] if fr else -1
    gf = L["ghost_fright"] if fr else -1
    h = L["house_limits"]
    out.append([n, L["pac"], pf, L["elroy2"], L["elroy1"], L["ghost"], gf, L["tunnel"], fr, h[0], h[1], h[2], L["elroy1_dots"], L["elroy2_dots"]])
rec = json.load(open(os.path.join(root, "tests/data/rom_tables.json")))["rom"]["speed_records"]
modes = []
for n in range(1, 22):
    modes.append([n] + rec[str(d[str(n)]["speed_record"])]["mode_frames"])
open(os.path.join(root, "tests/data/level_modes.txt"), "w").write("".join(" ".join(map(str, r)) + "\n" for r in modes))
open(os.path.join(root, "tests/data/level_expect.txt"), "w").write("".join(" ".join(map(str, r)) + "\n" for r in out))
print("wrote tests/data/level_expect.txt and level_modes.txt,", len(out), "levels")
