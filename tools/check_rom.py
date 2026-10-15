#!/usr/bin/env python3
"""Step S02 regression check: does roms/pacman.zip contain the standard Midway 'pacman' set (MAME)?
Usage: python3 tools/check_rom.py [--write]   (--write saves docs/evidence/S02_rom_check.txt)
Exit code 0 = all 10 files present with expected size and CRC32."""
import sys, zipfile, zlib, os
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
EXPECT = {  # name: (size, crc32)  source: MAME 'pacman' set, checked via mdk.cab listing 2026-10-06
 'pacman.6e': (4096, 0xc1e6ab10), 'pacman.6f': (4096, 0x1a6fb2d4), 'pacman.6h': (4096, 0xbcdd1beb),
 'pacman.6j': (4096, 0x817d94e3), 'pacman.5e': (4096, 0x0c944964), 'pacman.5f': (4096, 0x958fedf9),
 '82s123.7f': (32, 0x2fc650bd), '82s126.4a': (256, 0x3eb3a8e4), '82s126.1m': (256, 0xa9cc86bf), '82s126.3m': (256, 0x77245b66)}
def main():
    path = os.path.join(ROOT, 'roms', 'pacman.zip'); lines = []; ok = True
    if not os.path.exists(path): print('MISSING roms/pacman.zip'); return 2
    z = zipfile.ZipFile(path)
    for n, (size, crc) in EXPECT.items():
        try: d = z.read(n)
        except KeyError: lines.append(f'{n:12s} MISSING'); ok = False; continue
        good = len(d) == size and (zlib.crc32(d) & 0xffffffff) == crc
        ok &= good
        lines.append(f'{n:12s} size={len(d):5d} crc32={zlib.crc32(d) & 0xffffffff:08x} {"OK" if good else "MISMATCH (expected %d / %08x)" % (size, crc)}')
    lines.append('RESULT: ' + ('PASS, standard Midway pacman set' if ok else 'FAIL'))
    print('\n'.join(lines))
    if '--write' in sys.argv:
        with open(os.path.join(ROOT, 'docs', 'evidence', 'S02_rom_check.txt'), 'w') as f: f.write('\n'.join(lines) + '\n')
    return 0 if ok else 1
if __name__ == '__main__': sys.exit(main())
