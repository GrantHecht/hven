#!/usr/bin/env python3
"""Compare two TRAIN-23 cell CSVs column for column, wall excluded.

WALL IS NOT COMPARED (`wall_s`, `cell_rc`): CLAUDE.md section 7 makes counters
the asserted currency and timings informational. Everything else -- the status,
the outcome, the objective, the four KKT residuals and every `side.*` counter --
is compared as BYTES, because these are deterministic per-process columns taken
at `MKL_NUM_THREADS=1` in a serial run.

`outcome` is compared too, and `dnf_wall` is the one wall-DEPENDENT value in it;
a cell that changes only between `dnf_wall` and something else is flagged as
CENSORED rather than as a moved counter.
"""

import argparse
import csv
import sys

IGNORED = {"wall_s", "cell_rc"}


def load(path):
    with open(path, newline="") as handle:
        return {row["problem"]: row for row in csv.DictReader(handle)}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--label-a", default="A")
    ap.add_argument("--label-b", default="B")
    args = ap.parse_args()

    a, b = load(args.a), load(args.b)
    problems = sorted(set(a) | set(b))
    moved, censored = [], []

    for problem in problems:
        ra, rb = a.get(problem), b.get(problem)
        if ra is None or rb is None:
            moved.append((problem, "PRESENT ONLY IN ONE FILE", "", ""))
            continue
        keys = sorted((set(ra) | set(rb)) - IGNORED)
        diffs = [(k, ra.get(k, ""), rb.get(k, "")) for k in keys
                 if ra.get(k, "") != rb.get(k, "")]
        if not diffs:
            continue
        wall_involved = any("dnf_wall" in (x, y) for _, x, y in diffs)
        target = censored if wall_involved else moved
        for key, x, y in diffs:
            target.append((problem, key, x, y))

    print("# {} vs {}".format(args.label_a, args.label_b))
    print("# cells: {}".format(len(problems)))
    print("# MOVED (counter/status/objective): {} field differences".format(len(moved)))
    for problem, key, x, y in moved:
        print("MOVED   {:12s} {:28s} {} -> {}".format(problem, key, x, y))
    print("# CENSORED (a dnf_wall on one side): {} field differences".format(len(censored)))
    for problem, key, x, y in censored:
        print("CENSOR  {:12s} {:28s} {} -> {}".format(problem, key, x, y))
    return 1 if moved else 0


if __name__ == "__main__":
    sys.exit(main())
