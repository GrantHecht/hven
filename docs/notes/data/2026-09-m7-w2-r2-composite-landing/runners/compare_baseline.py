#!/usr/bin/env python3
"""Arm 0 faithfulness: a scratch cell's .out vs the committed re-take 7 rows (results.csv + counters-hven-sqp.csv).
usage: compare_baseline.py OUTFILE CELL [LABEL]   -- byte comparison of every shared column (wall_s excluded)."""
import csv, sys
out, cell = sys.argv[1], sys.argv[2]
label = sys.argv[3] if len(sys.argv) > 3 else "default"
base = f"/home/ghecht/Projects/hven-bench/runs/2026-09-m7-w2-retake7/run-A-{label}/cutest/"
def rows(p):
    with open(p) as f:
        return list(csv.DictReader(l for l in f if not l.startswith("#")))
kv = {}
inside = False
for line in open(out):
    line = line.rstrip("\n")
    if line == "CELL_BEGIN": inside = True; continue
    if line == "CELL_END": inside = False; continue
    if inside and "=" in line:
        k, _, v = line.partition("="); kv[k] = v
res = [r for r in rows(base + "results.csv") if f"/{cell}/" in r["cell_id"]]
cnt = [r for r in rows(base + "counters-hven-sqp.csv") if f"/{cell}/" in r["cell_id"]]
assert len(res) == 1 and len(cnt) == 1, (len(res), len(cnt))
diff = same = 0
for col in ["status", "outcome", "iterations", "factorizations", "objective", "kkt_stationarity", "kkt_primal_eq", "kkt_primal_ineq", "kkt_complementarity"]:
    a, b = res[0][col], kv.get(col, "<absent>")
    if a == b: same += 1
    else: diff += 1; print(f"DIFF results.{col}: committed={a} scratch={b}")
for col, a in cnt[0].items():
    if col == "cell_id": continue
    b = kv.get("side." + col, "<absent>")
    if b == "<absent>" and col.startswith("eqb_"): b = kv.get(col, "<absent>")
    if a == b or (a in ("", "null") and b in ("", "null", "<absent>")): same += 1
    else: diff += 1; print(f"DIFF counters.{col}: committed={a} scratch={b}")
print(f"{cell} {label}: {same} columns identical, {diff} differing -> {'IDENTICAL' if diff == 0 else 'DIFFERS'}")
