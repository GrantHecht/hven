#!/usr/bin/env python3
"""Compare instruction-count CSVs (icount.py output) base vs head over the same cells.
usage: icmp.py <base.csv> <head.csv> [label]   -> markdown: per (lane) median/sum B/A ratio and the identity check.
Identity: every CELL key except wall-clock-like ones (key contains 'wall' or 'seconds' or 'time') equal on every cell; status/iterations/factorizations/objective always compared.
--ignore-prefix P (repeatable via env IGN="p1,p2") drops keys starting with the prefix (the no-ledger variant drops harness_qp* columns)."""
import csv, os, statistics, sys
def load(p): return {(r["lane"], r["problem"]): r for r in csv.DictReader(open(p))}
a, b = load(sys.argv[1]), load(sys.argv[2]); label = sys.argv[3] if len(sys.argv) > 3 else ""
ign = [x for x in os.environ.get("IGN", "").split(",") if x]
cells = sorted(set(a) & set(b))
def skip(k): return any(t in k for t in ("wall", "seconds", "time", "rc")) or any(k.startswith(p) for p in ign)
out = {}
bad = []
for c in cells:
    ra, rb = a[c], b[c]
    ia, ib = ra["instructions"], rb["instructions"]
    if ia in ("DNF", "") or ib in ("DNF", ""): bad.append((c, "DNF")); continue
    out.setdefault(c[0], []).append((c, int(ia), int(ib)))
    for k in ra:
        if k in ("instructions", "rc") or skip(k): continue
        if ra[k] != rb.get(k): bad.append((c, k, ra[k], rb.get(k)))
print(f"### {label}  base={os.path.basename(sys.argv[1])} head={os.path.basename(sys.argv[2])}  cells common={len(cells)}")
print("| lane | cells | sum base | sum head | sum B/A | median B/A | min B/A | max B/A |\n|---|---:|---:|---:|---:|---:|---:|---:|")
for lane, rows in sorted(out.items()):
    r = [ib / ia for _, ia, ib in rows]
    sa, sb = sum(x[1] for x in rows), sum(x[2] for x in rows)
    print(f"| {lane} | {len(rows)} | {sa:,} | {sb:,} | {sb/sa:.5f} | {statistics.median(r):.5f} | {min(r):.5f} | {max(r):.5f} |")
med = {l: statistics.median([ia for _, ia, _ in rows]) for l, rows in out.items()}
print("median base cell (instructions): " + ", ".join(f"{l} {m:,.0f}" for l, m in med.items()))
print(f"identity violations: {len(bad)}")
for x in bad[:20]: print("  ", x)
