#!/usr/bin/env python3
"""d4sum.py DIR [M]: count the [d4] kOptimal QP exits in the TRAIN cells' .err files; flag rows/ftol > M (default 10)
and compl/ftol > 100 (kTerminalPolishComplFactor); per-cell list of flagged."""
import sys, glob, re, os
d = sys.argv[1]; M = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
tot = rows = compl = either = 0; cells = {}
for f in sorted(glob.glob(f"{d}/*.err")):
    c = os.path.basename(f)[:-4]
    for l in open(f, errors="replace"):
        if not l.startswith("[d4]"): continue
        m = re.search(r"rows/ftol=(\S+) compl/ftol=(\S+)", l)
        r, k = float(m.group(1)), float(m.group(2))
        tot += 1; fr = r > M; fk = k > 100.0
        rows += fr; compl += fk; either += fr or fk
        if fr or fk:
            e = cells.setdefault(c, [0, 0, 0, 0.0, 0.0]); e[0] += fr; e[1] += fk; e[2] += 1
            e[3] = max(e[3], r); e[4] = max(e[4], k)
print(f"{d}: kOptimal QP exits {tot}; rows>{M:g}*ftol {rows}; compl>100*ftol {compl}; either {either}; cells hit {len(cells)}")
for c, e in sorted(cells.items(), key=lambda kv: -kv[1][2]):
    print(f"   {c}: rows {e[0]} compl {e[1]} (max rows/ftol {e[3]:.2e}, max compl/ftol {e[4]:.2e})")
