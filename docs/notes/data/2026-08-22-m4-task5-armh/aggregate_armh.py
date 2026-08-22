#!/usr/bin/env python3
"""Aggregate an ARM-H three-arm IPM wall leg log (BASE / ARM-H / LANDED).

Usage: aggregate_armh.py <armh wall log>

Adapted from ../m4-ipm-wall-leg/aggregate.py for a three-arm read instead of
the standing two-arm (base/head) one. For each (mode, n) cell prints the
median-of-per-rep-medians and minimum-of-per-rep-minimums for all three
sides, the BASE arm's own rep-to-rep spread, and three deltas read off the
MINIMUM estimator (the low-noise one -- see ../m4-ipm-wall-leg/ipm_wall_leg.sh's
own header for why the median is not read directly on these problem sizes):

  armh_vs_base    does ARM-H alone (candidate_point.h grown, dispatch tail
                  reverted to its pre-task bare-else form) reproduce the
                  LANDED-vs-BASE band trip by itself?
  landed_vs_base  the already-adjudicated band trip
                  (docs/notes/data/2026-08-21-m4-task5-wall/README.md),
                  recomputed on this run for a same-occasion comparison.
  armh_vs_landed  residual between the two HEAD-side arms. Near zero means
                  ARM-H and LANDED read the same (mechanism confirmed as
                  layout, not the dispatch edit); a move back toward BASE
                  implicates the dispatch edit instead.

The adjudication reads n=240 rows first (the cells that cleared the standing
precedent); n=60/120 rows ride along as a free consistency check.
"""
import re
import statistics as st
import sys


def main(path):
    med, mn, mode = {}, {}, None
    row = re.compile(
        r"rep\d+ (base|armh|landed) wide n=(\d+) reps=\d+ median_s=([\d.]+) min_s=([\d.]+)"
    )
    for line in open(path):
        line = line.strip()
        if line.startswith("=== arm"):
            mode = line.split(":")[1].strip()
            continue
        m = row.match(line)
        if not m:
            continue
        key = (mode, int(m.group(2)), m.group(1))
        med.setdefault(key, []).append(float(m.group(3)))
        mn.setdefault(key, []).append(float(m.group(4)))

    for mode in sorted({k[0] for k in med}):
        for n in sorted({k[1] for k in med if k[0] == mode}):
            b_med = st.median(med[(mode, n, "base")])
            a_med = st.median(med[(mode, n, "armh")])
            l_med = st.median(med[(mode, n, "landed")])
            b_min = min(mn[(mode, n, "base")])
            a_min = min(mn[(mode, n, "armh")])
            l_min = min(mn[(mode, n, "landed")])
            spread = 100 * (max(med[(mode, n, "base")]) - min(med[(mode, n, "base")])) / b_med
            d_ab = 100 * (a_min - b_min) / b_min
            d_lb = 100 * (l_min - b_min) / b_min
            d_al = 100 * (a_min - l_min) / l_min
            print(
                f"{mode:9s} n={n:<5d}"
                f" median base={b_med:.6f} armh={a_med:.6f} landed={l_med:.6f}"
                f" | minimum base={b_min:.6f} armh={a_min:.6f} landed={l_min:.6f}"
                f" | armh_vs_base {d_ab:+.3f}% landed_vs_base {d_lb:+.3f}%"
                f" armh_vs_landed {d_al:+.3f}%"
                f" | base rep-spread {spread:.2f}%"
            )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "armh_wall.log")
