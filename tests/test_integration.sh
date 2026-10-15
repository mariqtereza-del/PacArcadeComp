#!/bin/sh
# X48: integration checks without a window: settings file tidy and round trip, Purist table, and every key the program reacts to is listed in the help panel.
cd "$(dirname "$0")/.." || exit 2
D=build/x48; mkdir -p $D; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
g++ -std=c++17 -O2 -Wall -Wextra -DMINIZ_NO_STDIO -DMINIZ_NO_TIME $(sdl2-config --cflags) tests/test_integration.cpp src/board/romset.cpp src/video/gfx.cpp \
  build/app/miniz.o build/app/miniz_tdef.o build/app/miniz_tinfl.o build/app/miniz_zip.o -o $D/t_int $(sdl2-config --libs) || { echo "FAIL: build"; exit 1; }
$D/t_int $D | tee $D/int_out.txt | tail -3; grep -q "RESULT: PASS" $D/int_out.txt || fail=1
# every function key and Backspace that main.cpp handles has a line in the help panel (menu.cpp draw_help)
KEYS=$(grep -o "SDLK_F[0-9]*\|SDLK_BACKSPACE" src/app/main.cpp | sort -u | sed 's/SDLK_BACKSPACE/BKSP/; s/SDLK_//')
for k in $KEYS ESC; do n=$(grep -c "{\"$k\", \"" src/app/menu.cpp); chk "help panel lists $k" "$([ "$n" -ge 1 ] && echo yes || echo no)" yes; done
chk "number of keys handled (F1 to F12 and Backspace)" "$(echo $KEYS | wc -w)" 13
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
