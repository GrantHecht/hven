#!/usr/bin/env python3
"""Aggregate an ARM-H four-arm IPM wall leg log (BASE / ARM-H / LANDED / FIXED).

Usage: aggregate_armh.py <armh wall log>

Adapted from ../m4-ipm-wall-leg/aggregate.py for a four-arm read instead of
the standing two-arm (base/head) one. For each (mode, n) cell prints the
median-of-per-rep-medians and minimum-of-per-rep-minimums for all four
sides, the BASE arm's own rep-to-rep spread, and four deltas read off the
MINIMUM estimator (the low-noise one -- see ../m4-ipm-wall-leg/ipm_wall_leg.sh's
own header for why the median is not read directly on these problem sizes):

  armh_vs_base    does ARM-H alone (candidate_point.h grown, dispatch tail
                  reverted to its pre-task bare-else form) reproduce the
                  LANDED-vs-BASE band trip by itself?
  landed_vs_base  the already-adjudicated band trip
                  (docs/notes/data/2026-08-21-m4-task5-wall/README.md),
                  recomputed on this run for a same-occasion comparison.
  armh_vs_landed  residual between the two 8503f39-side arms. Near zero means
                  ARM-H and LANDED read the same (mechanism confirmed as
                  layout, not the dispatch edit); a move back toward BASE
                  implicates the dispatch edit instead.
  fixed_vs_base   the same question asked of the state that actually ships.
                  The fix batch rewrote the dispatch tail's refuse message and
                  added a per-entry coordinate check to the bridge's scatter,
                  so LANDED's adjudication does not transfer to it by
                  assumption. Read on the same occasion and by the same
                  estimator as the other three; fixed_vs_landed is printed
                  beside it as the residual between the two.

The adjudication reads n=240 rows first (the cells that cleared the standing
precedent); n=60/120 rows ride along as a free consistency check.
"""
import re
import statistics as st
import sys

SIDES = ("base", "armh", "landed", "fixed")


def main(path):
    med, mn, mode = {}, {}, None
    row = re.compile(
        r"rep\d+ (base|armh|landed|fixed) wide n=(\d+) reps=\d+ median_s=([\d.]+) min_s=([\d.]+)"
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
            missing = [s for s in SIDES if (mode, n, s) not in med]
            if missing:
                print(f"{mode:9s} n={n:<5d} INCOMPLETE -- no rows for: {', '.join(missing)}")
                continue
            medians = {s: st.median(med[(mode, n, s)]) for s in SIDES}
            minima = {s: min(mn[(mode, n, s)]) for s in SIDES}
            base_reps = med[(mode, n, "base")]
            spread = 100 * (max(base_reps) - min(base_reps)) / medians["base"]

            def delta(side, reference):
                return 100 * (minima[side] - minima[reference]) / minima[reference]

            print(
                f"{mode:9s} n={n:<5d}"
                f" median base={medians['base']:.6f} armh={medians['armh']:.6f}"
                f" landed={medians['landed']:.6f} fixed={medians['fixed']:.6f}"
                f" | minimum base={minima['base']:.6f} armh={minima['armh']:.6f}"
                f" landed={minima['landed']:.6f} fixed={minima['fixed']:.6f}"
                f" | armh_vs_base {delta('armh', 'base'):+.3f}%"
                f" landed_vs_base {delta('landed', 'base'):+.3f}%"
                f" armh_vs_landed {delta('armh', 'landed'):+.3f}%"
                f" fixed_vs_base {delta('fixed', 'base'):+.3f}%"
                f" fixed_vs_landed {delta('fixed', 'landed'):+.3f}%"
                f" | base rep-spread {spread:.2f}%"
            )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "armh_wall.log")
