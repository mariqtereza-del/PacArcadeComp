#!/bin/sh
# S35: first-run ROM finder. (1) unit test with a pretend player (missing, wrong, damaged, picked, remembered), (2) the real program with no ROM and with wrong ROMs in scripted mode: friendly text in the
# log and a clean exit code 2, (3) the program starts normally with the ROM next to it, (4) the ROM is not touched.
cd "$(dirname "$0")/.." || exit 2
export DISPLAY=${DISPLAY:-:99}; xdpyinfo >/dev/null 2>&1 || { rm -f /tmp/.X99-lock /tmp/.X11-unix/X99; ( Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & ); sleep 2; }
D=build/s35; rm -rf $D; mkdir -p $D/files $D/app; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
python3 - $D/files <<'P'
import sys, zipfile, os, shutil
d = sys.argv[1]; rom = "roms/pacman.zip"
open(d + "/text.zip", "w").write("this is only text\n" * 20)
with zipfile.ZipFile(d + "/empty.zip", "w") as z: z.writestr("readme.txt", "no game here")
with zipfile.ZipFile(rom) as src, zipfile.ZipFile(d + "/badcrc.zip", "w") as z:
    first = True
    for i in src.infolist():
        b = src.read(i.filename)
        if first: b = bytes([b[0] ^ 0xFF]) + b[1:]; first = False
        z.writestr(i.filename, b)
data = open(rom, "rb").read(); open(d + "/cut.zip", "wb").write(data[:len(data) // 2])
P
g++ -std=c++17 -O1 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME -I src/third_party/miniz -o $D/t_fr tests/test_firstrun.cpp src/app/firstrun.cpp src/board/romset.cpp build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o $(sdl2-config --cflags --libs) || { echo "FAIL: build"; exit 1; }
SUM0=$(sha1sum roms/pacman.zip | cut -d' ' -f1)
$D/t_fr $D/files roms/pacman.zip > $D/unit.txt; tail -3 $D/unit.txt; grep -q "RESULT: PASS" $D/unit.txt || fail=1
# the real program, scripted mode (no dialogs), in a folder that has no ROM
cp build/app/pacman-port $D/app/; P=$D/app/pacman-port
( cd $D/app && timeout 30 ./pacman-port --no-audio --measure 1 --report r_none.txt --config cfg/c.cfg >/dev/null 2>&1; echo $? > code_none )
chk "no ROM: exit code 2 (clean stop)" "$(cat $D/app/code_none)" 2
chk "no ROM: friendly message in the log" "$(grep -a -c 'does not contain the game itself' $D/app/r_none.txt)" 1
for w in text empty badcrc cut; do
  ( cd $D/app && timeout 30 ./pacman-port --no-audio --measure 1 --report r_$w.txt --config cfg/c.cfg --rom ../files/$w.zip >/dev/null 2>&1; echo $? > code_$w )
  chk "$w.zip: exit code 2" "$(cat $D/app/code_$w)" 2
  chk "$w.zip: friendly message in the log" "$(grep -a -c 'This file cannot be used' $D/app/r_$w.txt)" 1
done
cp roms/pacman.zip $D/app/; ( cd $D/app && timeout 30 ./pacman-port --no-audio --measure 3 --report r_ok.txt --config cfg/c.cfg >/dev/null 2>&1; echo $? > code_ok )
chk "ROM next to the program: starts and exits normally" "$(cat $D/app/code_ok)" 0
chk "ROM next to the program: log says which ROM is used" "$(grep -a -c '^ROM: using .*pacman.zip' $D/app/r_ok.txt)" 1
chk "the ROM file is unchanged" "$(sha1sum roms/pacman.zip | cut -d' ' -f1)" "$SUM0"
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
