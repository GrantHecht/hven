#!/usr/bin/env python3
"""PF-4 r2 probe 3 summary (the backward-error tables). Reads probe 3's per-column CSVs (one row per border column of
every k >= 2 Schur batch: eta of the batch solve's column and of the single solve of the same right-hand side, both
against K0 on the numerics the solve used, eta = ||K0 x - b||inf / (||K0||inf ||x||inf + ||b||inf); the backend's
refinement steps; eta after one more refinement step through solve_mat).
usage: probe3_summary.py <csv>..."""
import csv
import math
import sys
from collections import Counter

TIER = {0: "plain", 1: "scaled", 2: "matched"}


def q(xs, p):
    return xs[min(len(xs) - 1, int(p * len(xs)))] if xs else float("nan")


def gm_log(pairs):
    v = [math.log10(a / b) for a, b in pairs if a > 0 and b > 0]
    return (sum(v) / len(v), sum(1 for x in v if x > 0) / len(v), len(v)) if v else (float("nan"), float("nan"), 0)


print("cell.label.trajectory | tier | pp>0 | columns | batches | eta batch: median p99 max | eta single: median p99 max | "
      "mean log10(eta_b/eta_s) | share batch worse | refinement steps batch / single | max rel col diff")
for path in sys.argv[1:]:
    name = path.split("/")[-1].removesuffix(".csv")
    rows = list(csv.DictReader(open(path)))
    nb = len(set(r["batch"] for r in rows))
    groups = {}
    for r in rows:
        groups.setdefault((int(r["tier"]), r["pp"] != "0"), []).append(r)
    for (t, pp), rs in sorted(groups.items()):
        eb = sorted(float(r["eta_b"]) for r in rs)
        es = sorted(float(r["eta_s"]) for r in rs)
        g, w, _ = gm_log([(float(r["eta_b"]), float(r["eta_s"])) for r in rs])
        rb = dict(Counter(r["ref_b"] for r in rs))
        rsx = dict(Counter(r["ref_s"] for r in rs))
        rd = max(float(r["reldiff"]) for r in rs)
        same = sum(int(r["resolve_same"]) for r in rs)
        print(f"{name} | {TIER[t]} | {pp} | {len(rs)} | {nb} | {q(eb,.5):.2e} {q(eb,.99):.2e} {eb[-1]:.2e} | "
              f"{q(es,.5):.2e} {q(es,.99):.2e} {es[-1]:.2e} | {g:+.3f} | {w:.3f} | {rb} / {rsx} | {rd:.2e}"
              + ("" if same == len(rs) else f" | RESOLVE_DIFFERS {len(rs)-same}"))
    E = [(float(r["eta_b"]), float(r["eta_s"]), float(r["eta_b1"])) for r in rows]
    for t in (1e-12, 1e-10, 1e-8):
        G = [(min(b, b1) if b > t else b, s) for b, s, b1 in E]
        print(f"  offline guard t={t:g} (keep-while-falling one step on columns with eta_b > t): flagged "
              f"{sum(1 for b, _, _ in E if b > t) / len(E):.4f}, mean log10(eta_guard/eta_s) {gm_log(G)[0]:+.3f}, "
              f"max eta guard {max(x for x, _ in G):.2e} vs single {max(s for _, s in G):.2e}")
