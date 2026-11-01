#!/usr/bin/env python3
"""Fold every retained leg log under raw/ into one table.

Counters and statuses only: no wall number is read out of these logs and none
is quoted.  Usage: summarise.py [glob ...]
"""
import glob
import os
import sys

ROOT = "/home/ghecht/Projects/hven/.scratch/m7w2a/raw"
KEYS = [
    "status", "outcome", "iterations", "objective",
    "side.steps_accepted", "side.rejected_steps", "side.factorizations",
    "side.qp_minor_iters", "side.elastic_activations", "side.restoration_iters",
    "side.symbolic_analyses", "side.evals_full", "side.evals_values",
    "kkt_stationarity", "kkt_primal_eq", "kkt_primal_ineq",
    "side.sqp_infeasibility_certified", "side.border_refine_steps",
    "side.soc_steps", "side.soc_applied", "side.watchdog_restores",
]


def read(path):
    d = {"tag": os.path.basename(path)[:-4], "lock": "?", "rc": "?"}
    with open(path) as fh:
        for line in fh:
            line = line.rstrip("\n")
            if line.startswith("LOCK="):
                d["lock"] = line.split("=", 1)[1]
            elif line.startswith("LEG_RC="):
                d["rc"] = line.split("=", 1)[1]
            elif "=" in line and not line.startswith(("PGREP", "CELL ", "BINARY", "LEG_")):
                k, _, v = line.partition("=")
                if k in KEYS or k in ("budget_iters", "problem", "options_label", "threw"):
                    d[k] = v
    return d


def main():
    pats = sys.argv[1:] or ["*"]
    paths = []
    for p in pats:
        paths.extend(sorted(glob.glob(os.path.join(ROOT, p + ".log"))))
    cols = ["tag", "lock", "rc", "budget_iters", "status", "iterations",
            "side.steps_accepted", "side.rejected_steps", "side.factorizations",
            "side.qp_minor_iters", "side.elastic_activations",
            "side.restoration_iters", "side.symbolic_analyses",
            "side.evals_full", "objective", "kkt_stationarity", "kkt_primal_eq"]
    short = {c: c.replace("side.", "") for c in cols}
    rows = [read(p) for p in paths]
    widths = {c: max(len(short[c]), *(len(str(r.get(c, "-"))) for r in rows)) for c in cols}
    print("  ".join(short[c].ljust(widths[c]) for c in cols))
    for r in rows:
        print("  ".join(str(r.get(c, "-")).ljust(widths[c]) for c in cols))


if __name__ == "__main__":
    main()
