#!/usr/bin/env python3
"""I17 item-3 measurement table. For hven-sqp roster cells only: per configuration (a cells directory + arm tag), the count of rows by
status, the total iterations over rows that finish `optimal`, capped/missing rows, and the rows that differ from a reference
configuration in status or iterations (BETTER / WORSE / NEUTRAL by status rank optimal > acceptable > other).
usage: always_table.py DN3_DIR REF_NAME REF_DIR:ARM [NAME DIR:ARM ...]     (the first configuration is the reference)"""
import os
import sys
from collections import Counter

RANK = {"optimal": 2, "acceptable": 1}


def load(prefix):
    row = {}
    res = prefix + ".result"
    capped = os.path.exists(res) and "CAPPED" in open(res).read()
    if os.path.exists(prefix + ".out"):
        for line in open(prefix + ".out"):
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                row[k] = v
    return (None if capped or "status" not in row else row)


def main():
    d3 = sys.argv[1]
    cfgs = []
    a = sys.argv[2:]
    for i in range(0, len(a), 2):
        d, arm = a[i + 1].rsplit(":", 1)
        cfgs.append((a[i], d, arm))
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
    cells = [f"{p}-{l}-{q}" for p, l, q in roster if l == "hven-sqp"]
    rows = {n: {c: load(os.path.join(d, "cells", f"{c}.{arm}")) for c in cells} for n, d, arm in cfgs}
    ref = cfgs[0][0]
    print(f"hven-sqp TRAIN cells: {len(cells)}")
    for n, _, _ in cfgs:
        st = Counter(r["status"] if r else "MISSING_OR_CAPPED" for r in rows[n].values())
        it = sum(int(r["iterations"]) for r in rows[n].values() if r and r["status"] == "optimal")
        mi = sum(int(r.get("side.qp_minor_iters", 0)) for r in rows[n].values() if r)
        print(f"{n:28s} optimal {st.get('optimal', 0):3d}  other {sum(st.values()) - st.get('optimal', 0):3d}  iters(optimal) {it:6d}  qp_minors {mi:8d}  {dict(st)}")
    for n, _, _ in cfgs[1:]:
        tally = Counter()
        print(f"--- {n} vs {ref}: rows differing in status or iterations")
        for c in cells:
            a_, b_ = rows[ref][c], rows[n][c]
            if a_ is None or b_ is None:
                print(f"  {c}: MISSING/CAPPED ({ref} {a_ is not None}, {n} {b_ is not None})")
                continue
            if a_["status"] != b_["status"] or a_["iterations"] != b_["iterations"]:
                ra, rb = RANK.get(a_["status"], 0), RANK.get(b_["status"], 0)
                d = "BETTER" if rb > ra else "WORSE" if rb < ra else "NEUTRAL"
                tally[d] += 1
                print(f"  {d:7s} {c}: {a_['status']}/{a_['iterations']} -> {b_['status']}/{b_['iterations']}")
        print(f"  tally {dict(tally)}")


if __name__ == "__main__":
    main()
