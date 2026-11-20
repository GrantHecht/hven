#!/usr/bin/env python3
"""Cross-run determinism of the pass-A instruction totals.  usage: pf3_determinism.py dirA dirB [dirC...] (cells dirs)"""
import glob, json, os, sys
runs = []
for d in sys.argv[1:]:
    r = {}
    for f in glob.glob(d + "/*.json"):
        c = json.load(open(f))
        if "error" in c or c.get("truncated"):
            continue
        r[c["cell"]] = (c["instructions_total"], c.get("status"), c.get("iterations"), c.get("factorizations"))
    runs.append(r)
cells = sorted(set.intersection(*[set(r) for r in runs]))
worst = 0; nstat = 0; rows = []
for k in cells:
    vals = [r[k][0] for r in runs]
    rel = (max(vals) - min(vals)) / min(vals)
    stat_same = len({r[k][1:] for r in runs}) == 1
    worst = max(worst, rel); nstat += (not stat_same)
    rows.append((rel, k, vals, stat_same))
rows.sort(reverse=True)
print(f"runs: {len(runs)}; cells in all: {len(cells)}; status/iterations/factorizations identical in all runs for {len(cells)-nstat} cells; "
      f"max relative instruction spread {worst:.3e}; cells with spread 0: {sum(1 for r in rows if r[0]==0)}")
for rel, k, vals, ss in rows[:8]:
    print(f"  {rel:.3e} {k} {vals} stat_same={ss}")
