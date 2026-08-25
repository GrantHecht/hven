#!/usr/bin/env python3
"""Aggregate a standing-bench wall leg log into the two estimators the ledger's
standing rule reads: median-of-reps and minimum-of-reps, plus the base arm's own
rep-to-rep spread. Same reading rule as the IPM leg's aggregate.py — a delta
inside the printed base spread is not a signal.

Usage: bench_aggregate.py <wall_leg log>
"""

import re
import statistics as st
import sys


def main(path):
    cells, cell = {}, None
    for line in open(path):
        line = line.rstrip()
        if line.startswith("==="):
            cell = line.split()[1]
            continue
        m = re.match(r"\s+rep(\d+) (base|head) ([\d.]+)", line)
        if m and cell:
            cells.setdefault((cell, m.group(2)), []).append(float(m.group(3)))

    for c in dict.fromkeys(k[0] for k in cells):
        b, h = cells[(c, "base")], cells[(c, "head")]
        bmed, hmed, bmin, hmin = st.median(b), st.median(h), min(b), min(h)
        print(
            f"{c:14s} median {bmed:.6f} -> {hmed:.6f} ({100 * (hmed - bmed) / bmed:+.3f}%)"
            f" | minimum {bmin:.6f} -> {hmin:.6f} ({100 * (hmin - bmin) / bmin:+.3f}%)"
            f" | base rep-spread {100 * (max(b) - min(b)) / bmed:.2f}%"
        )


if __name__ == "__main__":
    main(sys.argv[1])
