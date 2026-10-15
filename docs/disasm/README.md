# docs/disasm
- `pacman.asm`: the 16 KB program ROM with code and data separated. Generated; re-assembles to the identical ROM. Do not edit by hand.
- `blocks.txt`: the classification (code, inline bytes after RST 28h/30h, RST 20h jump tables, data), one block per line. `end` is exclusive.
- `pacman_raw.asm`: the old straight-through listing (treats data as code). Kept only because tools/analysis/dis.py reads a copy; superseded by pacman.asm.
- Regenerate and verify: `python3 tools/trace_disasm.py` (needs z80dasm and z80asm: `apt-get install -y z80asm z80dasm`). Report: docs/evidence/S08_roundtrip.txt.
- Inputs: build/rom/prog.bin (from the user's ROM) and docs/evidence/S08_runtime_pcs.txt (every ROM byte the prototype fetched while playing, from tools/analysis/oracle_s08_exec.py).
- Block names: codeNNN = traced code; inlineNNN = data bytes after RST 28h (2 bytes) or RST 30h (3 bytes); jumptableNNN = RST 20h word table; dataNNN = bytes never reached as code.
- Note: pacman.asm is derived from the player's ROM, so like the ROM it is not part of anything distributed. (Keep it out of releases; the checkpoint zips are private.)
