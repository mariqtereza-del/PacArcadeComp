#!/usr/bin/env python3
"""Step S08 check: re-run the tracer, re-assemble docs/disasm/pacman.asm and require a byte-identical ROM, no executed byte missing from the
traced code, no overlapping instructions, every byte classified."""
import sys, os, subprocess
root = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
r = subprocess.run([sys.executable, os.path.join(root, 'tools/trace_disasm.py')], capture_output=True, text=True, cwd=root)
t = r.stdout
ok = r.returncode == 0 and 'IDENTICAL, 16384 bytes' in t and 'missing = 0; overlapping instructions = 0' in t and 'unclassified:                              0 bytes' in t
print('PASS S08: pacman.asm re-assembles to the identical 16384-byte ROM; every executed byte is traced code; 0 unclassified' if ok else 'FAIL S08\n' + t + r.stderr)
sys.exit(0 if ok else 1)
