#!/bin/sh
# S36: soak test. The real window runs the game for SECONDS (default 480) with every extra on while the window is resized, moved, the menu opened and closed, full screen toggled and focus taken away and given back,
# every few seconds, and a coin, start and arrow keys are pressed (the game itself plays only by those keys; the attract mode runs in between). Checks: the program is still alive and exits normally, memory does not keep growing (second half against first half), the machine tick rate stays near 60.6, no crash message.
cd "$(dirname "$0")/.." || exit 2
SECS=${1:-480}; D=build/s36; rm -rf $D; mkdir -p $D; fail=0
chk() { if [ "$2" = "$3" ]; then echo "  ok: $1 ($2)"; else echo "  FAIL: $1 (got \"$2\", wanted \"$3\")"; fail=1; fi; }
xd() { timeout 5 xdotool "$@"; }
export DISPLAY=${DISPLAY:-:99}; xdpyinfo >/dev/null 2>&1 || { rm -f /tmp/.X99-lock /tmp/.X11-unix/X99; ( Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & ); sleep 2; }
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
build/app/pacman-port --no-audio --crt-mask strong --glow strong --curve strong --surround ambient --scanlines --fps 144 --measure $SECS --report $D/r.txt --config $D/c.cfg --no-hiscore >/dev/null 2>&1 &
P=$!; sleep 4; W=$(xdotool search --pid $P | head -1); xd windowfocus $W; xd key 5; sleep 0.3; xd key 1; : > $D/rss.txt; T=0; N=0; T0=$(date +%s)
while kill -0 $P 2>/dev/null && [ $T -lt $((SECS - 5)) ]; do
  RSS=$(awk '/VmRSS/ {print $2}' /proc/$P/status 2>/dev/null); [ -n "$RSS" ] && echo "$T $RSS" >> $D/rss.txt
  case $((N % 6)) in
    0) xd windowsize $W 700 900 >/dev/null 2>&1;; 1) xd windowsize $W 1200 700 >/dev/null 2>&1;;
    2) xd windowfocus $W; xd key Escape; sleep 0.5; xd key Escape;;
    3) xd windowmove $W 50 40 >/dev/null 2>&1; xd windowsize $W 1000 1000 >/dev/null 2>&1;;
    4) xd windowfocus $W; xd key F11; sleep 1; xd key F11;;
    5) xd windowsize $W 448 576 >/dev/null 2>&1;;
  esac
  for k in Left Up Right Down; do xd keydown $k; sleep 0.4; xd keyup $k; done      # some real play
  N=$((N + 1)); sleep 8; T=$(( $(date +%s) - T0 ))
done
wait $P; CODE=$?
chk "the program ran to the end and exited normally" "$CODE" 0
python3 - $D/rss.txt <<'P' || fail=1
import sys
v=[tuple(map(int,l.split())) for l in open(sys.argv[1]) if l.strip()]
if len(v)<6: print("  FAIL: too few memory samples", len(v)); sys.exit(1)
h=len(v)//2; a=sum(x[1] for x in v[1:h])/max(1,len(v[1:h])); b=sum(x[1] for x in v[h:])/len(v[h:])
print(f"  memory (RSS, MB): first half average {a/1024:.1f}, second half average {b/1024:.1f}, last {v[-1][1]/1024:.1f}, samples {len(v)}")
ok = b < a*1.10 + 5*1024
print("  ok: memory does not keep growing" if ok else "  FAIL: memory grows by more than 10 %"); sys.exit(0 if ok else 1)
P
RATE=$(grep -a "Measured tick rate" $D/r.txt | sed 's/.*rate: \([0-9.]*\).*/\1/'); echo "  tick rate $RATE"; chk "tick rate at least 55 per second (the menu pauses and the toggling of full screen cost some time)" "$(python3 -c "print(1 if float('${RATE:-0}') >= 55 and float('${RATE:-0}') <= 61.2 else 0)")" 1
chk "no crash or error lines in the report" "$(grep -a -c -i 'crash\|exception\|segmentation\|assert' $D/r.txt)" 0
[ $fail = 0 ] && echo "RESULT: PASS" || { echo "RESULT: FAIL"; exit 1; }
