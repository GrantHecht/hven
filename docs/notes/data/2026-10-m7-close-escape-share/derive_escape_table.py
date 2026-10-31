#!/usr/bin/env python3
"""Derive the per-cell escape table from the committed M7 close census baseline.

Reads bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv (comment lines
start with '#'), writes escape_table.csv next to this script and prints the
aggregate figures quoted in docs/notes/2026-10-m7-close.md appendix A.
No solver is run.  Usage: derive_escape_table.py [baseline.csv] [out.csv]
"""
import csv
import os
import sys

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", "..", "..", ".."))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    root, "bench", "baselines", "2026-10-m7-close-corpus", "walk_baseline.csv")
dst = sys.argv[2] if len(sys.argv) > 2 else os.path.join(here, "escape_table.csv")

with open(src, newline="") as f:
    rows = list(csv.DictReader(l for l in f if not l.startswith("#")))

keep = ["cell_id", "status", "factorizations", "qp_subproblems", "qp_fact_per_qp",
        "escapes", "esc_budget", "esc_singular", "esc_no_contraction",
        "esc_infeasible_suspect", "esc_indefinite", "esc_gate_refused",
        "ipqp_escapes", "ipqp_factorizations"]
esc = keep[6:12]

def val(r, k):
    """-1 is the corpus binary's sentinel on dnf rows (columns not measured)."""
    v = int(r[k])
    return None if v < 0 else v

with open(dst, "w", newline="") as f:
    w = csv.writer(f, lineterminator="\n")
    w.writerow(keep + ["measured", "esc_reason_sum", "reasons_sum_equals_escapes"])
    for r in rows:
        if val(r, "escapes") is None:
            w.writerow([r[k] for k in keep] + [0, "NA", "NA"])
        else:
            s = sum(int(r[k]) for k in esc)
            w.writerow([r[k] for k in keep] + [1, s, int(s == int(r["escapes"]))])

m = [r for r in rows if val(r, "escapes") is not None]
tot = lambda k: sum(int(r[k]) for r in m)
print("cells", len(rows), "measured", len(m), "sentinel(-1)", [(r["cell_id"], r["status"]) for r in rows if r not in m])
print("status (measured)", {s: sum(r["status"] == s for r in m) for s in sorted({r["status"] for r in m})})
print("cells with escapes>0", sum(int(r["escapes"]) > 0 for r in m))
print("cells with ipqp_escapes>0", sum(int(r["ipqp_escapes"]) > 0 for r in m))
print("total escapes", tot("escapes"), {k: tot(k) for k in esc}, "ipqp_escapes", tot("ipqp_escapes"))
print("factorizations over measured cells", tot("factorizations"), "ipqp_factorizations", tot("ipqp_factorizations"))
print("reasons sum != escapes on measured:", [r["cell_id"] for r in m if sum(int(r[k]) for k in esc) != int(r["escapes"])])
