#!/bin/sh
# S36: no memory growth in the game logic. The scripted game runs 3000 seconds of game time (182,000 frames, no window, as fast as possible); memory is sampled while it runs.
# Before the S36 fix the log of I/O writes grew by about 300 bytes per frame (55 MB in this run); now memory must stay flat (growth under 3 MB after the first two seconds).
cd "$(dirname "$0")/.." || exit 2
D=build/s36l; rm -rf $D; mkdir -p $D; fail=0
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
build/app/pacman-port --no-audio --autoplay 3000 --report $D/r.txt --config $D/c.cfg --no-hiscore >/dev/null 2>&1 &
P=$!; : > $D/rss.txt; while kill -0 $P 2>/dev/null; do awk '/VmRSS/ {print $2}' /proc/$P/status 2>/dev/null >> $D/rss.txt; sleep 0.5; done; wait $P
python3 - $D/rss.txt <<'P' || fail=1
import sys
v=[int(l) for l in open(sys.argv[1]) if l.strip()]
if len(v)<6: print("  FAIL: too few samples", len(v)); sys.exit(1)
a=v[4]; b=max(v[4:]); print(f"  memory samples {len(v)}: after 2 s {a/1024:.1f} MB, highest afterwards {b/1024:.1f} MB, growth {(b-a)/1024:.1f} MB")
ok=(b-a) < 3*1024; print("  ok: memory stays flat" if ok else "  FAIL: memory keeps growing"); sys.exit(0 if ok else 1)
P
grep -a -q "182242 frames run" $D/r.txt && echo "  ok: the same 182242 frames were played" || { echo "  FAIL: frame count differs"; fail=1; }
grep -a -q "^PASS autoplay" $D/r.txt && echo "  ok: autoplay PASS line" || { echo "  FAIL: no autoplay PASS"; fail=1; }
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
