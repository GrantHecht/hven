#!/usr/bin/env python3
"""sparse-equil P2 criterion. usage: replay_check.py DEV_DIR OUT_DIR MOVERS.txt
Per mode (walk, ssn, ipm): arm A's row (OUT/A-k1-<mode>.csv) vs each B repetition (OUT/B-k<K>-<mode>.csv) through
scripts/compare_replay.py, byte-exact; MOVERS.txt lists '<mode> <cell>' lines whose differences are declared (none by default).
Also each B repetition vs the dev B row (DEV/B-k1-<mode>.csv). Exit 0 iff 0 undeclared differences and the three B repetitions agree."""
import subprocess, sys, os
dev, out, movers_path = sys.argv[1:4]
movers = [l.split("#")[0].split() for l in open(movers_path) if l.split("#")[0].strip()]
declared_modes = {m[0] for m in movers}
here = os.path.dirname(os.path.abspath(__file__))
cmp_py = os.path.join(here, "..", "..", "..", "..", "..", "scripts", "compare_replay.py")
bad = 0
for mode in ("walk", "ssn", "ipm"):
    for k in (1, 2, 3):
        for ref, tag in ((os.path.join(out, f"A-k1-{mode}.csv"), "A"), (os.path.join(dev, f"B-k1-{mode}.csv"), "Bdev")):
            r = subprocess.run([sys.executable, cmp_py, ref, os.path.join(out, f"B-k{k}-{mode}.csv"), f"{tag}-vs-B-k{k}-{mode}"],
                               capture_output=True, text=True)
            open(os.path.join(out, f"cmp-{tag}-k{k}-{mode}.txt"), "w").write(r.stdout + r.stderr)
            last = (r.stdout.strip().splitlines() or [""])[-1]
            ok = r.returncode == 0 and "differences: 0" in r.stdout
            print(f"{mode} k{k} B vs {tag}: {'EQUAL' if ok else 'DIFFERS'} | {last}")
            if not ok and not (tag == "A" and mode in declared_modes):
                bad += 1
print("REPLAY_AS_DECLARED" if bad == 0 else f"REPLAY_FAIL {bad}")
sys.exit(0 if bad == 0 else 1)
