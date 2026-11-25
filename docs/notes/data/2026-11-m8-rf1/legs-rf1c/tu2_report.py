#!/usr/bin/env python3
"""RF-1c leg U: R vs H on the TU-2 cutest cells. usage: tu2_report.py H_DIR R_DIR. Lists every cell (R -> H: status, majors(iterations),
factorizations, objective), the movers, LOST (R solved -> H not), FALSE_CERT (H certified infeasible where R did not), THREW, and cost growth
(H/R factorizations) on cells where both end non-solved at the work-limit family (kStalled etc) or solved."""
import glob, os, sys
SOLVED = ("optimal", "acceptable")
def load(f):
    r = {}
    if not os.path.exists(f): return None
    for line in open(f, errors="replace"):
        if "=" in line:
            k, v = line.rstrip("\n").split("=", 1); r.setdefault(k, v)
    return r
def cert(r): return r.get("status") == "infeasible" or r.get("side.sqp_infeasibility_certified") == "1"
def fmt(r): return f"{r.get('status')} it={r.get('iterations')} fact={r.get('factorizations')} f={r.get('objective')}"
h, rd = sys.argv[1:3]
bad = 0; solved = {"R": 0, "H": 0}; n = 0
for f in sorted(glob.glob(os.path.join(h, "cells", "*.out"))):
    nm = os.path.basename(f)[:-4]; n += 1
    a, b = load(f), load(os.path.join(rd, "cells", nm + ".out"))
    if b is None: print("MISSING_R", nm); bad += 1; continue
    if a.get("threw") == "1" or b.get("threw") == "1" or a.get("rc") not in ("0", None) and a.get("rc") != "0":
        print(f"THREW_OR_RC {nm} R rc={b.get('rc')} threw={b.get('threw')} | H rc={a.get('rc')} threw={a.get('threw')}")
    solved["R"] += b.get("status") in SOLVED; solved["H"] += a.get("status") in SOLVED
    if b.get("status") in SOLVED and a.get("status") not in SOLVED:
        print(f"LOST {nm} {b.get('status')}->{a.get('status')}"); bad += 1
    if cert(a) and not cert(b):
        print(f"FALSE_CERT {nm} R {b.get('status')} | H {a.get('status')}"); bad += 1
    mv = (b.get("status"), b.get("iterations"), b.get("factorizations")) != (a.get("status"), a.get("iterations"), a.get("factorizations"))
    try: g = int(a.get("factorizations")) / max(1, int(b.get("factorizations")))
    except (TypeError, ValueError): g = float("nan")
    print(f"{'MOVER' if mv else 'SAME '} {nm} | R {fmt(b)} | H {fmt(a)} | growth {g:.3f}")
print(f"cells {n} SOLVED R {solved['R']} H {solved['H']}")
print("VERDICT", "TU2_NO_LOSS_NO_FALSE_CERT" if bad == 0 else f"TU2_FLAGS({bad})")
