#!/bin/sh
# X10: the light bar follows the game. The S27 replay (7,273 frames: a one-player game with 3 deaths and 2 energizers, then a two-player game) is played on a pretend DualSense
# (SDL virtual joystick with a SetLED callback). The colour log ("Light bar: r,g,b (mood) at machine frame N") must change at the same frames as the vibration events of the
# same replay (X11 part: deaths, extra life, round cleared, game over and new game are compared with the event reader's frames by tests/test_lightbar_scen.cpp): red begins at the frame of a GHOST NEAR event, blue-white begins right at an ENERGIZER, and ends at the FRIGHT END frame. Purist and Xbox pads get nothing.
cd "$(dirname "$0")/.." || exit 2
sh tools/build_app.sh >/dev/null || { echo "FAIL: build"; exit 1; }
export DISPLAY=${DISPLAY:-:99}; pgrep Xvfb >/dev/null || { Xvfb :99 -screen 0 1280x800x24 >/dev/null 2>&1 & sleep 1; }
D=build/x10; mkdir -p $D; rm -f $D/*.txt
python3 tools/analysis/oracle_replay.py --make $D/script.replay 7273 >/dev/null || { echo "FAIL: replay"; exit 1; }
g++ -std=c++17 -O2 -Wall -Wextra tests/test_lightbar_scen.cpp src/app/replay.cpp src/board/machine.cpp src/board/bus.cpp -o $D/test_lightbar_scen 2>&1 | grep -E "error|warning" && { echo "FAIL: build scen"; exit 1; }
$D/test_lightbar_scen $D/script.replay || exit 1      # X11: flashes on the real machine against the event reader
run() { n=$1; shift; timeout 250 build/app/pacman-port --no-audio --scale 2 --replay $D/script.replay --fast --config $D/$n.cfg --no-hiscore --report $D/$n.txt "$@" >/dev/null 2>&1; }
run ps5 --virtual-type ps5
run xbox --virtual-type xbox360
run pur --purist --virtual-type ps5
python3 - <<'PY'
import re, sys
D = "build/x10"; fail = 0
def chk(what, ok, extra=""):
    global fail
    print(("  ok: " if ok else "  FAIL: ") + what + (" " + extra if extra else ""))
    if not ok: fail = 1
log = open(f"{D}/ps5.txt").read()
moods = [(int(m.group(5)), m.group(4), (int(m.group(1)), int(m.group(2)), int(m.group(3)))) for m in re.finditer(r"Light bar: (\d+),(\d+),(\d+) \(([a-z ]+(?: \(no game\))?)\) at machine frame (\d+)", log)]
vib = [(int(m.group(2)), m.group(1)) for m in re.finditer(r"Vibration: ([A-Z ]+) at machine frame (\d+)", log)]
near_starts = [f for f, mo, c in moods if mo == "near a ghost"]
fr_starts = [f for f, mo, c in moods if mo == "ghosts frightened"]
fr_ends = [moods[i + 1][0] for i, (f, mo, c) in enumerate(moods) if mo == "ghosts frightened" and i + 1 < len(moods)]
heart = sorted(f for f, n in vib if n == "GHOST NEAR"); energ = sorted(f for f, n in vib if n == "ENERGIZER"); fend = sorted(f for f, n in vib if n == "FRIGHT END")
print("  info: red starts at", near_starts[:12], "... (", len(near_starts), ")"); print("  info: heartbeats at", heart[:12], "... (", len(heart), ")")
print("  info: blue-white starts", fr_starts, "energizers", energ, "ends", fr_ends, "fright end events", fend)
chk("the replay gave red moments (a ghost came close)", len(near_starts) >= 3, f"({len(near_starts)})")
bad = [f for f in near_starts if f not in heart]
chk("every red start is at the frame of a GHOST NEAR heartbeat event", not bad, f"(not matching: {bad[:5]})")
chk("two frightened periods (the two energizers)", len(fr_starts) == 2 and len(energ) == 2, f"({fr_starts} / {energ})")
chk("blue-white starts at the energizer frame (within 2 frames)", all(0 <= a - b <= 2 for a, b in zip(fr_starts, energ)) and len(fr_starts) == len(energ), f"({fr_starts} vs {energ})")
chk("blue-white ends at the FRIGHT END event frame (within 1 frame)", len(fr_ends) == len(fend) and all(abs(a - b) <= 1 for a, b in zip(fr_ends, fend)), f"({fr_ends} vs {fend})")
chk("red colours are red (g = b = 0)", all(c[1] == 0 and c[2] == 0 and c[0] > 80 for f, mo, c in moods if mo == "near a ghost"))
deaths = sorted(f for f, n in vib if n == "DEATH"); dflash = [int(m.group(1)) for m in re.finditer(r"Light bar flash: death at machine frame (\d+)", log)]
chk("the program's colour log has a red flash at each DEATH vibration frame", dflash == deaths and len(deaths) == 5, f"({dflash} vs {deaths})")
chk("the log starts dark (self-test / attract) and goes dark again at game over", [mo for f, mo, c in moods][0] == "dark (no game)" and any(mo == "dark (no game)" and f > 4000 for f, mo, c in moods))
chk("blue-white colour is 140,190,255", all(c == (140, 190, 255) for f, mo, c in moods if mo == "ghosts frightened"))
m = re.search(r"light bar received: (\d+) colour commands, last (\d+),(\d+),(\d+)", log)
chk("the pretend DualSense received many fading colour commands", m and int(m.group(1)) > len(moods), f"({m.group(1) if m else None} commands for {len(moods)} mood changes)")
chk("and the last one is the neutral blue put back on exit", m and (int(m.group(2)), int(m.group(3)), int(m.group(4))) == (0, 32, 255))
for n, why in (("xbox", "Xbox pretend pad"), ("pur", "Purist")):
    t = open(f"{D}/{n}.txt").read(); m2 = re.search(r"light bar received: (\d+)", t)
    chk(f"{why}: no light bar commands and no colour log", m2 and int(m2.group(1)) == 0 and "Light bar:" not in t)
print("PASS: the light bar log changes at the same frames as the near-ghost and frightened events of the replay" if not fail else "FAIL")
sys.exit(fail)
PY
