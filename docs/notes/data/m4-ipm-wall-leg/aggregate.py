#!/usr/bin/env python3
"""Aggregate a wall leg log into the two estimators the protocol reads.

Usage: aggregate.py <ipm_wall_leg or layout_wall_leg log>

Prints, per arm, cell name and size: the median-of-per-rep-medians delta, the
minimum-of-per-rep-minimums delta, and the base arm's rep-to-rep spread. The
minimum estimator is the low-noise one; a median delta inside the printed
spread is not a signal. See the protocol header of ipm_wall_leg.sh.

Both legs are read by this one aggregator: the cell name is whatever the probe
prints before `n=` ("wide" for the IPM leg, "construct"/"transcribe"/
"transcribe+key"/"solve"/"solve1" for the layout leg). Trailing identity columns are
ignored here -- they are compared between arms by reading the log, and a
difference in one is a correctness finding rather than a timing one.
"""
import re
import statistics as st
import sys


def main(path):
    med_s, min_s, arm = {}, {}, None
    row = re.compile(
        r"rep\d+ (base|head) (\S+) n=(\d+) reps=\d+ median_s=([\d.]+) min_s=([\d.]+)"
    )
    order = []
    for line in open(path):
        line = line.strip()
        if line.startswith("=== arm"):
            arm = line.split(":")[1].strip()
            continue
        m = row.match(line)
        if not m:
            continue
        cell, n, side = m.group(2), int(m.group(3)), m.group(1)
        if (arm, cell, n) not in order:
            order.append((arm, cell, n))
        key = (arm, cell, n, side)
        med_s.setdefault(key, []).append(float(m.group(4)))
        min_s.setdefault(key, []).append(float(m.group(5)))

    for arm, cell, n in order:
        if (arm, cell, n, "head") not in med_s:
            continue
        b, h = med_s[(arm, cell, n, "base")], med_s[(arm, cell, n, "head")]
        bm, hm = min(min_s[(arm, cell, n, "base")]), min(min_s[(arm, cell, n, "head")])
        spread = 100 * (max(b) - min(b)) / st.median(b)
        print(
            f"{arm:9s} {cell:15s} n={n:<5d}"
            f" median {st.median(b):.6f} -> {st.median(h):.6f}"
            f" ({100 * (st.median(h) - st.median(b)) / st.median(b):+.3f}%)"
            f" | minimum {bm:.6f} -> {hm:.6f} ({100 * (hm - bm) / bm:+.3f}%)"
            f" | base rep-spread {spread:.2f}%"
        )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "ipm_wall.log")
