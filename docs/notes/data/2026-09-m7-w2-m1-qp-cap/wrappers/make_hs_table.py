#!/usr/bin/env python3
"""The hs TRAIN read the M1 fix round owes: the budget arm against the OFF arm
on the OTHER TRAIN family.

THE STANDARD IS THE CUTEST ONE, and the settler's ruling states it: ship 3 iff
the hs split reaches the SAME optimal count with NO new error. A moved hs cell
is a DECLARED COST, not a veto -- the same standard the CUTEst read was judged
under -- so the moved cells are listed with their majors, minors and
factorizations rather than counted against the lever.
"""
import csv, sys, os

IGNORED = {"wall_s", "cell_rc", "hven_pin_sha", "source_path"}
COST = ("side.major_iters", "side.qp_minor_iters", "side.factorizations",
        "side.steps_accepted", "side.rejected_steps", "side.elastic_activations",
        "side.restoration_iters")

def load(p):
    with open(p, newline="") as h:
        return {r["problem"]: r for r in csv.DictReader(h)}

def main():
    d = sys.argv[1] if len(sys.argv) > 1 else ".scratch/m7w2m1/measure-hs"
    off, bud = load(os.path.join(d, "cells-hs-off.csv")), load(os.path.join(d, "cells-hs-budget.csv"))
    probs = sorted(set(off) | set(bud))
    print("# M7 W2 M1 -- the hs TRAIN split ({} cells), OFF (budget 1) against the budget arm (3)\n".format(len(probs)))
    print("| cell | OFF | budget | f OFF | f budget | moved? |")
    print("|---|---|---|---|---|---|")
    moved = []
    for p in probs:
        a, b = off.get(p, {}), bud.get(p, {})
        diffs = [(k, a.get(k, ""), b.get(k, "")) for k in sorted((set(a) | set(b)) - IGNORED)
                 if a.get(k, "") != b.get(k, "")]
        if diffs:
            moved.append((p, diffs))
        print("| `{}` | {} | {} | {} | {} | {} |".format(
            p, a.get("outcome", "?"), b.get("outcome", "?"), a.get("objective", ""),
            b.get("objective", ""), "**MOVED**" if diffs else "unmoved"))
    print("\n## The moved cells, with what they cost\n")
    if not moved:
        print("None: the budget arm is bit-identical to OFF on every hs TRAIN cell.")
    for p, diffs in moved:
        a, b = off.get(p, {}), bud.get(p, {})
        print("**{}** — ".format(p) + "; ".join(
            "{} {} -> {}".format(k.replace("side.", ""), a.get(k, ""), b.get(k, "")) for k in COST
            if a.get(k, "") != b.get(k, "")))
        others = [k for k, _, _ in diffs if k not in COST]
        if others:
            print("   other columns that moved: " + ", ".join(others))
    def tally(rows, key):
        return sum(1 for p in probs if rows.get(p, {}).get("outcome", "?") == key)
    print("\n## The rule, hs arm\n")
    for k in ("optimal", "error", "dnf_wall", "infeasible", "no_row"):
        na, nb = tally(off, k), tally(bud, k)
        if na or nb:
            print("- `{}`: OFF **{}**, budget **{}**".format(k, na, nb))
    off_err = {p for p in probs if off.get(p, {}).get("outcome") == "error"}
    bud_err = {p for p in probs if bud.get(p, {}).get("outcome") == "error"}
    new_err = sorted(bud_err - off_err)
    same_opt = tally(bud, "optimal") >= tally(off, "optimal")
    print("- new errors: **{}** {}".format(len(new_err), new_err))
    print("\n**hs VERDICT: {}**".format(
        "the same optimal count and no new error -- 3 SHIPS" if (same_opt and not new_err)
        else "an optimal lost or an error gained -- the default STAYS 1 and 3 is the recommendation"))
    return 0

if __name__ == "__main__":
    sys.exit(main())
