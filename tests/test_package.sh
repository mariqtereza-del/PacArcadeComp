#!/bin/sh
# S35: the release package. (1) listing: only the program, README and licences; (2) NO ROM BYTES: 32-byte pieces taken every 64 bytes from every file of the ROM set are searched for in every file of the package (the exe too) and inside the zip as a whole;
# (3) the exe carries the icon and version information and the program's own name, not the original game's; (4) the README names MazeChase.exe and pacman.zip.
cd "$(dirname "$0")/.." || exit 2
fail=0; chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/package.sh >/dev/null 2>&1 || { echo "FAIL: package build"; exit 1; }
ZIP=$(ls dist/MazeChase-*-win64.zip | head -1); echo "package: $ZIP"
LIST=$(unzip -Z1 $ZIP | sort | tr '\n' ' '); echo "  listing: $LIST"
chk "listing is exactly the folder, program, README, problem-report helper, licences" "$LIST" "MazeChase/ MazeChase/MazeChase.exe MazeChase/README.txt MazeChase/REPORT-A-PROBLEM.bat MazeChase/THIRD-PARTY-LICENSES.txt "
chk "no zip, ROM or rom-like file inside" "$(unzip -Z1 $ZIP | grep -i -c '\.zip$\|\.rom$\|\.bin$\|pacman\.\|\.6[a-z]$\|\.5[a-z]$')" 0
python3 - $ZIP roms/pacman.zip <<'P'
import sys, zipfile, io
pkg, rom = sys.argv[1], sys.argv[2]
blobs = {"(the zip file itself)": open(pkg, "rb").read()}
with zipfile.ZipFile(pkg) as z:
    for n in z.namelist():
        if not n.endswith("/"): blobs[n] = z.read(n)
pieces = []
with zipfile.ZipFile(rom) as r:
    for n in r.namelist():
        d = r.read(n)
        for off in range(0, len(d) - 32 + 1, 64):
            p = d[off:off + 32]
            if len(set(p)) > 3: pieces.append((n, off, p))      # skip blank / constant stretches, they would match anything
print(f"  searching {len(pieces)} ROM pieces of 32 bytes in {len(blobs)} blobs")
bad = 0
for bn, b in blobs.items():
    hits = [(n, off) for n, off, p in pieces if p in b]
    if hits: bad += 1; print(f"  FOUND ROM bytes in {bn}: {hits[:3]}")
print("  ok: no ROM bytes found anywhere in the package" if not bad else "  FAIL: ROM bytes found")
sys.exit(1 if bad else 0)
P
[ $? = 0 ] || fail=1
chk "exe: version information present (Maze Chase)" "$(strings -el dist/MazeChase/MazeChase.exe | grep -c 'Maze Chase - an arcade maze game player')" 1
chk "exe: resource section (icon and version) present" "$([ $(x86_64-w64-mingw32-objdump -x dist/MazeChase/MazeChase.exe 2>/dev/null | grep -c '\.rsrc') -ge 1 ] && echo yes || echo no)" yes
chk "exe: window title uses our name" "$([ $(strings dist/MazeChase/MazeChase.exe | grep -c 'Maze Chase') -ge 3 ] && echo yes || echo no)" yes
chk "exe: the original game's name is not in the window titles" "$(strings dist/MazeChase/MazeChase.exe | grep -c '^Pac-Man - \(MENU\|BINDINGS\|PAUSED\)')" 0
chk "README names MazeChase.exe and pacman.zip" "$(grep -c 'MazeChase.exe' dist/MazeChase/README.txt)$(grep -c 'pacman.zip' dist/MazeChase/README.txt | sed 's/[1-9][0-9]*/+/')" "$(grep -c 'MazeChase.exe' dist/MazeChase/README.txt)+"
chk "licences file lists SDL and miniz" "$(grep -c '^1. SDL\|^2. miniz' dist/MazeChase/THIRD-PARTY-LICENSES.txt)" 2
# run the real Windows exe under Wine (if Wine works here): without a ROM it must stop politely, with the ROM it must play
if command -v wine >/dev/null 2>&1; then
  export DISPLAY=${DISPLAY:-:99}; xdpyinfo >/dev/null 2>&1 || { rm -f /tmp/.X99-lock /tmp/.X11-unix/X99; ( Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & ); sleep 2; }
  W=build/s35/wine; rm -rf $W; mkdir -p $W; cp dist/MazeChase/MazeChase.exe $W/
  ( cd $W && WINEDEBUG=-all timeout 120 wine MazeChase.exe --no-audio --measure 2 --report r_none.txt >/dev/null 2>&1; echo $? > code_none )
  if [ -f $W/r_none.txt ]; then
    chk "wine, no ROM: friendly message in the report" "$(grep -a -c 'does not contain the game itself' $W/r_none.txt)" 1
    cp roms/pacman.zip $W/; ( cd $W && WINEDEBUG=-all timeout 120 wine MazeChase.exe --no-audio --measure 4 --report r_ok.txt >/dev/null 2>&1 )
    chk "wine, ROM next to the exe: it runs and reports its pictures" "$(grep -a -c 'Measured tick rate' $W/r_ok.txt)" 1
    chk "wine: the ROM was found next to the exe" "$(grep -a -c '^ROM: using .*pacman.zip' $W/r_ok.txt)" 1
  else echo "  note: Wine could not run the program here (skipped; the Windows PC test decides)"; fi
else echo "  note: no Wine here (skipped)"; fi
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
