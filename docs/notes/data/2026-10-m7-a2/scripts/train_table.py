#!/usr/bin/env python3
"""A2 TRAIN per-cell table (descriptive): run A of each label against D = sqp-ipm, the A4 cost
statistic per cell (factorizations / max(1, major_iters)), outcome, objective. Usage:
train_table.py TREE"""
import csv
import os
import sys


def rows(path):
    return {r["problem"]: r for r in csv.DictReader(l for l in open(path) if not l.startswith("#"))}


def side(path):
    if not os.path.exists(path):
        return {}
    return {r["cell_id"].split("/")[1]: r
            for r in csv.DictReader(l for l in open(path) if not l.startswith("#"))}


def cost(r, s):
    if r["factorizations"] in ("null", "") or s is None or s.get("major_iters") in (None, "null", ""):
        return None
    return float(r["factorizations"]) / max(1.0, float(s["major_iters"]))


tree = sys.argv[1]
for fam, labels in (("hs", ("sqp-ipm-mu0c", "sqp-ipm-mufloor", "sqp-ipm-mu1e-3")),
                    ("cutest", ("sqp-ipm-mu0c", "sqp-ipm-mufloor"))):
    d = rows(f"{tree}/hven-sqp/default/{fam}-run-A/results.csv")
    ds = side(f"{tree}/hven-sqp/default/{fam}-run-A/counters-hven-sqp.csv")
    xs = {l: (rows(f"{tree}/hven-sqp/{l}/{fam}-run-A/results.csv"),
              side(f"{tree}/hven-sqp/{l}/{fam}-run-A/counters-hven-sqp.csv")) for l in labels}
    print(f"\n### {fam} -- per cell, run A: outcome / cost (factorizations per major); D = sqp-ipm")
    print("| cell | D | " + " | ".join(labels) + " |")
    print("|---|---|" + "---|" * len(labels))
    for p in sorted(d):
        def cell(r, s):
            c = cost(r, s)
            return f"{r['outcome']} / {'-' if c is None else f'{c:.4g}'}"
        line = f"| {p} | {cell(d[p], ds.get(p))} | "
        parts = []
        for l in labels:
            r, s = xs[l][0][p], xs[l][1].get(p)
            mark = ""
            if r["outcome"] != d[p]["outcome"]:
                mark = " **(outcome moved)**"
            elif r["outcome"] == "optimal" and r["objective"] != d[p]["objective"]:
                try:
                    if abs(float(r["objective"]) - float(d[p]["objective"])) > 1e-6 * max(
                            1.0, abs(float(d[p]["objective"]))):
                        mark = f" (f {float(r['objective']):.8g} vs {float(d[p]['objective']):.8g})"
                except ValueError:
                    pass
            parts.append(cell(r, s) + mark)
        print(line + " | ".join(parts) + " |")
