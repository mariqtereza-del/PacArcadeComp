#!/usr/bin/env python3
"""S30A: compares the C++ test's logs with the ones MAME wrote from tools/analysis/mame_level256.lua.
Usage: compare_mame_level256.py WORKDIR [EVIDENCE_FILE]   (WORKDIR holds ours_levels.txt, ours_events.txt, ours_vram300.bin and the mame_* twins)
Exit 0 when all three pairs are identical, 1 when not, 3 when the MAME files are missing (the caller decides whether that is acceptable)."""
import hashlib, os, sys
w = sys.argv[1]
out = open(sys.argv[2], "w") if len(sys.argv) > 2 else None
def say(s):
    print(s)
    if out: out.write(s + "\n")
def rd(n): return open(os.path.join(w, n), "rb").read()
for n in ("mame_levels.txt", "mame_events.txt", "mame_vram300.bin"):
    if not os.path.exists(os.path.join(w, n)):
        say("MAME files missing: " + n); sys.exit(3)
ok = True
for what, a, b in (("level table lines (256 level starts + after the wrap)", "levels.txt", "levels.txt"),
                   ("events (level changes, intermissions, level-256 dot count, best dots eaten, rack-test press and wrap)", "events.txt", "events.txt")):
    A, B = rd("ours_" + a).decode().splitlines(), rd("mame_" + b).decode().splitlines()
    same = A == B
    ok &= same
    say("%-95s %s (%d lines each)" % (what, "IDENTICAL" if same else "DIFFERENT", len(A)))
    if not same:
        for i, (x, y) in enumerate(zip(A, B)):
            if x != y:
                say("   first difference at line %d: ours '%s' / MAME '%s'" % (i + 1, x, y)); break
        else:
            say("   different lengths: ours %d, MAME %d" % (len(A), len(B)))
va, vb = rd("ours_vram300.bin"), rd("mame_vram300.bin")
same = va == vb
ok &= same
say("video RAM + colour RAM, 300 frames into level 256 (2,048 bytes): %s; sha1 ours %s, MAME %s" % ("IDENTICAL" if same else "DIFFERENT", hashlib.sha1(va).hexdigest()[:16], hashlib.sha1(vb).hexdigest()[:16]))
if not same:
    d = [i for i in range(min(len(va), len(vb))) if va[i] != vb[i]]
    say("   %d bytes differ, first at offset %s" % (len(d), ["%03X" % x for x in d[:10]]))
snap = os.path.join(w, "mame_snapshot.png"); pic = os.path.join(w, "ours_picture.rgb")
if os.path.exists(snap) and os.path.exists(pic):
    from PIL import Image
    im = Image.open(snap).convert("RGB"); mine = open(pic, "rb").read()
    if im.size != (224, 288) or len(mine) != 224 * 288 * 3:
        say("picture sizes differ: MAME %s, ours %d bytes" % (im.size, len(mine))); ok = False
    else:
        theirs = im.tobytes()
        diff = [i // 3 for i in range(0, len(mine), 3) if mine[i:i+3] != theirs[i:i+3]]
        same = not diff
        ok &= same
        say("the picture of the level-256 screen, 300 frames in (224 x 288 pixels, tiles and sprites): %s; sha1 ours %s, MAME %s" % (
            "IDENTICAL PIXEL FOR PIXEL" if same else "DIFFERENT in %d pixels" % len(diff), hashlib.sha1(mine).hexdigest()[:16], hashlib.sha1(theirs).hexdigest()[:16]))
        if diff:
            xs = [d % 224 for d in diff]; ys = [d // 224 for d in diff]
            say("   differing pixels lie in x %d..%d, y %d..%d; first five (x,y): %s" % (min(xs), max(xs), min(ys), max(ys), [(d % 224, d // 224) for d in diff[:5]]))
else:
    say("picture comparison not done in this run (needs the slower MAME pass with a screenshot: sh tests/test_level256_full.sh --pixels)")
say("RESULT: " + ("PASS (the port and MAME agree on everything compared)" if ok else "FAIL"))
sys.exit(0 if ok else 1)
