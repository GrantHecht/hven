"""Per-cell table of run A: each A3 label vs D (sqp-ipm), cells whose outcome or cost moved.
Usage: percell.py TREE  (cost = factorizations / max(1, major_iters), the A4 rule's)"""
import csv
import os
import sys

tree = sys.argv[1]
LABELS = ["sqp-ipm-slack1", "sqp-ipm-slack1e3", "sqp-ipm-skip1", "sqp-ipm-skipnever"]


def load(slot, s):
    d = f"{tree}/hven-sqp/{slot}/{s}-run-A"
    res = {r["problem"]: r for r in csv.DictReader(l for l in open(f"{d}/results.csv") if not l.startswith("#"))}
    side = {}
    p = f"{d}/counters-hven-sqp.csv"
    if os.path.exists(p):
        side = {r["cell_id"].split("/")[1]: r for r in csv.DictReader(l for l in open(p) if not l.startswith("#"))}
    return res, side


def cost(r, sd):
    if r["factorizations"] in ("null", ""):
        return None
    m = int(sd.get("major_iters", "0") or 0) if sd else 0
    return int(r["factorizations"]) / max(1, m)


for s in ("hs", "cutest"):
    D, Ds = load("default", s)
    for L in LABELS:
        X, Xs = load(L, s)
        print(f"== {s} {L} (cells that moved vs D)")
        n = 0
        for p in sorted(D):
            d, x = D[p], X[p]
            cd, cx = cost(d, Ds.get(p)), cost(x, Xs.get(p))
            if d["outcome"] != x["outcome"] or cd != cx:
                n += 1
                fd = f"{cd:.3f}" if cd is not None else "-"
                fx = f"{cx:.3f}" if cx is not None else "-"
                print(f"   {p:10s} {d['outcome']:>9s} -> {x['outcome']:<9s} cost/major {fd} -> {fx}  facts {d['factorizations']} -> {x['factorizations']}")
        print(f"   ({n} of {len(D)} moved)")
