#!/usr/bin/env python3
"""Aggregate an ARM-H four-arm IPM wall leg log (BASE / ARM-H / LANDED / FIXED).

Usage: aggregate_armh.py <armh wall log>

Adapted from ../m4-ipm-wall-leg/aggregate.py for a four-arm read instead of
the standing two-arm (base/head) one. For each (mode, n) cell prints the
median-of-per-rep-medians and minimum-of-per-rep-minimums for all four
sides, EACH ARM'S OWN rep-spread, and five deltas.

THE MINIMUM ESTIMATOR GOVERNS. Every delta below is read off
minimum-of-per-rep-minimums, and every classification comparison is made on
those deltas. The median is printed beside it as the second estimator and
never drives a verdict -- see ../m4-ipm-wall-leg/ipm_wall_leg.sh's own header
for why the median is not read directly on these problem sizes (its
rep-to-rep noise there, 0.5-0.8%, is comparable to the effects being looked
for).

THE SPREAD TERM IS MAX-OF-REP-SPREADS, PER ARM. Each arm's spread is the max
over its own reps' spreads -- the widest disagreement that arm shows with
itself across this log's reps, taken over both estimators as one number. All
four are printed. A delta is then marked `within` iff its magnitude does not
exceed the max of the two arms it compares, and `outside` otherwise: no arm
can be said to differ from another by less than either of them differs from
itself. This replaces the earlier base-arm-only rep-spread, which described
one arm's stability and was then applied to comparisons involving three
other arms.

The deltas:

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
                  assumption.
  fixed_vs_landed the residual between the two, i.e. whether the fix batch
                  moved the number at all.

This session is a NOISE-FLOOR CALIBRATION, not an accept-vs-mitigate
adjudication (owner ruling, docs/notes/2026-08-m4-ledger.md 2026-08-22): the
spread ACROSS the four arms at bit-identical answers is the layout-luck
variance the re-derived band ceiling has to sit above, and the per-arm
max-of-rep-spreads printed here is the within-arm term that read is set
against.

The adjudication reads n=240 rows first (the cells that cleared the standing
precedent); n=60/120 rows ride along as a free consistency check.
"""
import re
import statistics as st
import sys

SIDES = ("base", "armh", "landed", "fixed")


def rep_spread(values):
    """One arm's rep-spread in percent: widest rep-to-rep disagreement,
    normalized by that arm's own median. Zero for a single rep."""
    if len(values) < 2:
        return 0.0
    centre = st.median(values)
    return 100 * (max(values) - min(values)) / centre


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

            # Each arm's own spread, over both estimators, taken as one
            # number: the wider of the two is what that arm demonstrably does
            # to itself across this log's reps.
            spread = {
                s: max(rep_spread(med[(mode, n, s)]), rep_spread(mn[(mode, n, s)])) for s in SIDES
            }

            def delta(side, reference):
                """Percent change on the MINIMUM estimator, with the
                within-spread verdict against the two arms' own spreads."""
                value = 100 * (minima[side] - minima[reference]) / minima[reference]
                floor = max(spread[side], spread[reference])
                return f"{value:+.3f}% [{'within' if abs(value) <= floor else 'outside'}]"

            print(
                f"{mode:9s} n={n:<5d}"
                f" median base={medians['base']:.6f} armh={medians['armh']:.6f}"
                f" landed={medians['landed']:.6f} fixed={medians['fixed']:.6f}"
                f" | minimum base={minima['base']:.6f} armh={minima['armh']:.6f}"
                f" landed={minima['landed']:.6f} fixed={minima['fixed']:.6f}"
                f" | armh_vs_base {delta('armh', 'base')}"
                f" landed_vs_base {delta('landed', 'base')}"
                f" armh_vs_landed {delta('armh', 'landed')}"
                f" fixed_vs_base {delta('fixed', 'base')}"
                f" fixed_vs_landed {delta('fixed', 'landed')}"
                f" | max-of-rep-spreads base={spread['base']:.2f}%"
                f" armh={spread['armh']:.2f}% landed={spread['landed']:.2f}%"
                f" fixed={spread['fixed']:.2f}%"
            )


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "armh_wall.log")
