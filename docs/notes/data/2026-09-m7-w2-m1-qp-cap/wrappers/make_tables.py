#!/usr/bin/env python3
"""Turn M1's TRAIN-23 cell CSVs into the artifact's tables.

THE DECISION RULE IS THE TASK BRIEF'S AND IS APPLIED HERE, NOT BY EYE, ONCE PER
LEVER: a lever ships ON iff, against its OWN arm, it reaches at least the same
optimal count, introduces no new error in the counter currency, and moves no
other cell. The "both" arm is a CONFIRMATION, never the evidence for either
lever on its own.

`dnf_wall` is the one WALL-DEPENDENT outcome and is a DNF CONDITION, never a
measurement (M4 fix1, F-4): a transition into or out of it is reported in the
CENSORED class rather than counted as a new error or as a moved cell. The
CENSORED list is adjudicated in prose -- a censored transition is a candidate
that must be re-run ALONE before it is called anything -- never silently.
"""

import argparse
import csv
import os
import sys

# The nine cells the A-SQP read's section M1 names, plus ACOPR118, which shows
# the same shape without reaching the exit (its first subproblem is capped and
# the 300 s ceiling fell before a second failure could).
M1_CELLS = ("ACOPP118", "ACOPP57", "ACOPR57", "BRITGAS", "DALLASM", "READING6",
            "STEENBRB", "STEENBRC", "STEENBRG")
M1_TENTH = ("ACOPR118",)
IGNORED = {"wall_s", "cell_rc", "hven_pin_sha", "source_path"}
ARMS = ("budget", "retry", "both")


def load(path):
    with open(path, newline="") as handle:
        return {row["problem"]: row for row in csv.DictReader(handle)}


def diffs_of(ra, rb):
    return [(k, ra.get(k, ""), rb.get(k, ""))
            for k in sorted((set(ra) | set(rb)) - IGNORED)
            if ra.get(k, "") != rb.get(k, "")]


def repeat_agreement(a, b, label):
    out = []
    for problem in sorted(set(a) | set(b)):
        out += [(problem,) + d for d in diffs_of(a.get(problem, {}), b.get(problem, {}))]
    print("## {} repeat agreement: {} field differences".format(label, len(out)))
    for problem, key, x, y in out:
        print("   REPEAT {:12s} {:26s} {} -> {}".format(problem, key, x, y))
    return out


def outcome_of(row):
    return row.get("outcome", "?")


def identity_check(off, m4_off_path):
    """THE FREE IDENTITY CHECK: this task's OFF arm against M4's committed OFF
    arm, on every column that existed there. The counters are additive and both
    levers are at their OFF state, so any difference is a STOP."""
    print("\n## OFF identity against M7 W2 M4's committed OFF arm\n")
    if not os.path.exists(m4_off_path):
        print("MISSING: {}".format(m4_off_path))
        return 1
    old = load(m4_off_path)
    bad = 0
    for problem in sorted(set(old) | set(off)):
        ra, rb = old.get(problem, {}), off.get(problem, {})
        for key in sorted(set(ra) - IGNORED):
            if ra.get(key, "") != rb.get(key, ""):
                print("   DIFFERS {:12s} {:26s} {} -> {}".format(problem, key, ra[key],
                                                                 rb.get(key, "<absent>")))
                bad += 1
    print("Columns that existed in M4's OFF arm and differ here: **{}**{}".format(
        bad, "" if bad else "  (identity holds)"))
    return bad


def judge(off, arm, label):
    problems = sorted(set(off) | set(arm))
    moved, censored = [], []
    for problem in problems:
        ds = diffs_of(off.get(problem, {}), arm.get(problem, {}))
        if not ds:
            continue
        if any("dnf_wall" in (x, y) for _, x, y in ds):
            censored.append((problem, ds))
        else:
            moved.append((problem, ds))

    print("\n## The {} arm against OFF\n".format(label))
    print("| cell | OFF | {} | objective OFF | objective {} | moved? |".format(label, label))
    print("|---|---|---|---|---|---|")
    for problem in problems:
        ra, rb = off.get(problem, {}), arm.get(problem, {})
        ds = diffs_of(ra, rb)
        tag = ("CENSORED" if any("dnf_wall" in (x, y) for _, x, y in ds)
               else "**MOVED**" if ds else "unmoved")
        star = " **(M1)**" if problem in M1_CELLS else (" *(M1-10th)*" if problem in M1_TENTH
                                                       else "")
        print("| `{}`{} | {} | {} | {} | {} | {} |".format(
            problem, star, outcome_of(ra), outcome_of(rb),
            ra.get("objective", ""), rb.get("objective", ""), tag))

    print("\n### The nine M1 cells, {} arm\n".format(label))
    for problem in M1_CELLS + M1_TENTH:
        ra, rb = off.get(problem, {}), arm.get(problem, {})
        print("**{}** — OFF `{}` status `{}` f=`{}` iters=`{}` facts=`{}`; "
              "{} `{}` status `{}` f=`{}` iters=`{}` facts=`{}`".format(
                  problem,
                  outcome_of(ra), ra.get("status", ""), ra.get("objective", ""),
                  ra.get("iterations", ""), ra.get("factorizations", ""), label,
                  outcome_of(rb), rb.get("status", ""), rb.get("objective", ""),
                  rb.get("iterations", ""), rb.get("factorizations", "")))

    non_m1_moved = [(p, d) for p, d in moved if p not in M1_CELLS + M1_TENTH]
    print("\n### Cells outside the M1 list that moved, {} arm: **{}**".format(
        label, len(non_m1_moved)))
    for problem, ds in non_m1_moved:
        for key, x, y in ds:
            print("   MOVED  {:12s} {:26s} {} -> {}".format(problem, key, x, y))
    print("Censored (a `dnf_wall` on one side): {}".format(
        [p for p, _ in censored]))

    off_opt = sum(1 for p in problems if outcome_of(off.get(p, {})) == "optimal")
    arm_opt = sum(1 for p in problems if outcome_of(arm.get(p, {})) == "optimal")
    off_err = {p for p in problems if outcome_of(off.get(p, {})) == "error"}
    arm_err = {p for p in problems if outcome_of(arm.get(p, {})) == "error"}
    censored_side = {p for p in problems
                     if "dnf_wall" in (outcome_of(off.get(p, {})), outcome_of(arm.get(p, {})))}
    new_err = sorted((arm_err - off_err) - censored_side)
    new_err_censored = sorted((arm_err - off_err) & censored_side)
    others_moved = [p for p, _ in moved if p not in M1_CELLS + M1_TENTH]

    print("\n### The rule, {} arm\n".format(label))
    print("- optimal count: OFF **{}**, {} **{}**  ({})".format(
        off_opt, label, arm_opt, "OK" if arm_opt >= off_opt else "FAILS"))
    print("- new errors, counter currency: **{}** {}  ({})".format(
        len(new_err), new_err, "OK" if not new_err else "FAILS"))
    print("- plus, CENSORED class (reported apart, not counted as a new error): {} {}".format(
        len(new_err_censored), new_err_censored))
    print("- cells outside the M1 list that moved: **{}**  ({})".format(
        len(others_moved), "OK" if not others_moved else "FAILS"))
    verdict = (arm_opt >= off_opt) and not new_err and not others_moved
    print("\n**{} DEFAULT = {}**".format(label.upper(), "ON" if verdict else "OFF"))
    return verdict


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", required=True)
    ap.add_argument("--m4-off",
                    default="docs/notes/data/2026-09-m7-w2-m4-start-point/cells-off-r1.csv")
    args = ap.parse_args()
    d = args.dir

    def path(tag):
        return os.path.join(d, "cells-{}.csv".format(tag))

    if not os.path.exists(path("off")):
        print("MISSING: {}".format(path("off")), file=sys.stderr)
        return 2
    off = load(path("off"))

    print("# M7 W2 M1 — CUTEst TRAIN 23, SQP lane: OFF and the three arms\n")
    identity_check(off, args.m4_off)

    verdicts = {}
    for arm in ARMS:
        r1, r2 = path(arm + "-r1"), path(arm + "-r2")
        if not (os.path.exists(r1) and os.path.exists(r2)):
            print("\n(arm {} not complete: {} / {})".format(arm, os.path.exists(r1),
                                                            os.path.exists(r2)))
            continue
        a1, a2 = load(r1), load(r2)
        repeat_agreement(a1, a2, arm.upper())
        verdicts[arm] = judge(off, a1, arm)

    print("\n## The defaults, per lever\n")
    for arm in ARMS:
        if arm in verdicts:
            print("- **{}**: {}".format(arm, "ON" if verdicts[arm] else "OFF"))
    print("\n(The `both` arm is the CONFIRMATION of a pair of levers that each "
          "won on its own arm, never the evidence for either.)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
