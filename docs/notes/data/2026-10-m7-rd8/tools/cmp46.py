#!/usr/bin/env python3
"""Compare a run dir's cells to RETAKE18's measured pin rows, every key (SCW's cmp46.py, base moved to RETAKE18).
usage: cmp46.py RUNDIR
Classes: IDENTICAL (every key equal, pin sha/walls aside), EVALS (only eval-count keys differ), MOVED (same status,
other keys differ), CONVERTED (non-optimal -> optimal), LOST (optimal -> other), CAPPED-BOTH, OTHER.
A finished row whose wall exceeds 300 s is flagged READ (the harness ceiling)."""
import json
import os
import re
import sys

R18 = "/home/ghecht/Projects/hven-bench/runs/2026-10-m7-w2-retake18/measured"
EVAL_KEYS = {"side.evals_full", "side.evals_values", "side.harness_eval_f", "side.harness_eval_grad_f",
             "side.harness_eval_g", "side.harness_eval_jac", "side.harness_eval_hess"}
SKIP = {"hven_pin_sha", "wall_seconds", "side.wall_seconds", "side.solve_impl_seconds"}
rundir = sys.argv[1]
base = os.path.join(R18, "pin")


def read(path):
    if not os.path.exists(path):
        return None
    d = {}
    for line in open(path):
        line = line.rstrip("\n")
        if "=" in line:
            k, v = line.split("=", 1)
            d[k] = v
    return d or None


def wall(d, cell):
    f = os.path.join(d, cell + ".wall")
    if os.path.exists(f):
        t = open(f).read()
        if "CAPPED" in t:
            return "CAPPED"
        m = re.search(r"wall=([0-9.]+)s", t)
        return m.group(1) if m else "?"
    return None


w18 = {}
for line in open(os.path.join(R18, "walls.txt")):
    p = line.split()
    if len(p) >= 2:
        w18[p[0]] = "CAPPED" if p[1] == "CAPPED" else p[-1].rstrip("s")
pin = json.load(open("/home/ghecht/Projects/hven-bench/sets/cutest-nlp.json"))
probs = sorted(p["problem"] for p in pin["problems"] if p["split"] == "TRAIN")
counts = {}
print("| cell | RETAKE18 | now | class | keys differing | wall R18 / now |")
print("|---|---|---|---|---|---|")
for p in probs:
    for lab in ("default", "sqp-qn"):
        c = f"{p}-{lab}"
        if not os.path.exists(os.path.join(rundir, c + ".wall")):
            continue
        a = read(os.path.join(base, c + ".out"))
        b = read(os.path.join(rundir, c + ".out"))
        wb = w18.get(c, "?")
        wn = wall(rundir, c)

        def summ(d):
            if d is None:
                return "no row (capped)"
            return f"{d.get('status')} f {d.get('objective')} {d.get('iterations')} maj"
        if a is None and b is None:
            cls, nd = "CAPPED-BOTH", "-"
        elif a is None or b is None:
            cls = "CONVERTED" if (b is not None and b.get("status") == "optimal") else (
                "LOST" if (a is not None and a.get("status") == "optimal") else "OTHER")
            nd = "-"
        else:
            keys = sorted(k for k in set(a) | set(b) if k not in SKIP and a.get(k) != b.get(k))
            nd = len(keys)
            sa, sb = a.get("status"), b.get("status")
            if sa != sb:
                cls = "CONVERTED" if sb == "optimal" else ("LOST" if sa == "optimal" else "OTHER")
            elif not keys:
                cls = "IDENTICAL"
            elif all(k in EVAL_KEYS for k in keys):
                cls = "EVALS"
            else:
                cls = "MOVED"
            if keys and len(keys) <= 6:
                nd = f"{len(keys)}: " + " ".join(keys)
            elif keys:
                nd = f"{len(keys)}: " + " ".join(keys[:6]) + " ..."
        flag = ""
        for wv in (wb, wn):
            try:
                if float(wv) > 300.0:
                    flag = " READ"
            except (TypeError, ValueError):
                pass
        counts[cls] = counts.get(cls, 0) + 1
        print(f"| {c} | {summ(a)} | {summ(b)} | {cls}{flag} | {nd} | {wb} / {wn} |")
print()
print("COUNTS " + " ".join(f"{k}={v}" for k, v in sorted(counts.items())))
