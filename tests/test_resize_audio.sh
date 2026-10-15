#!/bin/sh
# S36 bug test: sound after a window resize. A resize on Windows made the Direct3D screen present far too fast for a moment; the smoothed pacing then ran the game fast and the sound queue grew to 0.7 s
# (wrong pitch, crackling, delay). The test imitates it with a simulated 180 Hz screen that runs 4 times too fast for 0.4 s and for 1.5 s (--burst), and a 204 ms freeze, with silent video and sound drivers (the timing is simulated, so no screen or sound card is needed).
# Before the fix: queue up to 683 ms, 40+ underruns, 69,000 samples dropped. Now: queue at most 150 ms + the device buffer, no dropped samples, game speed 60.0 +/- 0.5 ticks per second.
cd "$(dirname "$0")/.." || exit 2
sh tools/build_app.sh >/dev/null || { echo "FAIL: build app"; exit 1; }
D=build/s36r; rm -rf $D; mkdir -p $D; fail=0
for sc in "none:" "burst400:--burst 500:400" "burst1500:--burst 500:1500" "freeze204:--stall 500:204"; do
  n=${sc%%:*}; a=${sc#*:}
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/app/pacman-port --rom roms/pacman.zip --scripted --ticks 1000 --sim-refresh 180 --pacing smooth $a --report $D/$n.txt --config $D/c.cfg --no-hiscore >/dev/null 2>&1
  python3 - "$D/$n.txt" "$n" <<'P' || fail=1
import re,sys
t=open(sys.argv[1],errors="replace").read(); n=sys.argv[2]
rate=float(re.search(r"Measured tick rate: ([\d.]+)",t).group(1))
m=re.search(r"(\d+) underruns \((\d+) samples missing\), (\d+) samples dropped",t); under,miss,drop=map(int,m.groups())
mx=float(re.search(r"queue \(average [\d.]+ ms, smallest [\d.]+, largest ([\d.]+)\)",t).group(1))
print(f"  {n}: tick rate {rate:.3f}, underruns {under} ({miss} samples), dropped {drop}, queue largest {mx:.1f} ms")
bad=[]
if mx>160: bad.append("queue longer than 160 ms (delay)")
if drop>0: bad.append("sound dropped because the queue was full")
if n!="freeze204" and miss>500: bad.append("sound underruns (crackling): more than 500 samples (10 ms) missing")
if n=="freeze204" and under>40: bad.append("too many underruns after a 204 ms freeze")
if abs(rate-(60.0 if n!="freeze204" else 59.4))>0.7: bad.append("game speed is not 60 ticks per second")
if bad: print("  FAIL:", "; ".join(bad)); sys.exit(1)
print("  ok")
P
done
[ $fail = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
