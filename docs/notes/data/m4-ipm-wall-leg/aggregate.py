#!/usr/bin/env python3
"""Aggregate an IPM wall leg log into the two estimators the protocol reads.

Usage: aggregate.py <ipm_wall_leg log>

Prints, per arm and cell: the median-of-per-rep-medians delta, the
minimum-of-per-rep-minimums delta, and the base arm's rep-to-rep spread. The
minimum estimator is the low-noise one; a median delta inside the printed
spread is not a signal. See the protocol header of ipm_wall_leg.sh.
"""
import re
import statistics as st
import sys


def main(path):
    med_s, min_s, arm = {}, {}, None
    row = re.compile(r"rep\d+ (base|head) wide n=(\d+) reps=\d+ median_s=([\d.]+) min_s=([\d.]+)")
    for line in open(path):
        line = line.strip()
        if line.startswith("=== arm"):
            arm = line.split(":")[1].strip()
            continue
        m = row.match(line)
        if not m:
            continue
        key = (arm, int(m.group(2)), m.group(1))
        med_s.setdefault(key, []).append(float(m.group(3)))
        min_s.setdefault(key, []).append(float(m.group(4)))

    for arm in sorted({k[0] for k in med_s}):
        for n in sorted({k[1] for k in med_s if k[0] == arm}):
            b, h = med_s[(arm, n, "base")], med_s[(arm, n, "head")]
            bm, hm = min(min_s[(arm, n, "base")]), min(min_s[(arm, n, "head")])
            spread = 100 * (max(b) - min(b)) / st.median(b)
            print(
                f"{arm:9s} n={n:<5d}"
                f" median {st.median(b):.6f} -> {st.median(h):.6f}"
                f" ({100 * (st.median(h) - st.median(b)) / st.median(b):+.3f}%)"
                f" | minimum {bm:.6f} -> {hm:.6f} ({100 * (hm - bm) / bm:+.3f}%)"
                f" | base rep-spread {spread:.2f}%"
            )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "ipm_wall.log")
