#!/usr/bin/env python3
"""M8 walk cap (../DECLARATION.md, criterion I2): the census criterion for a classification-changing fix whose census
movers were NOT measured before the declaration (no dev census was taken: one census is ~6 h serial-tiered).

usage: census_walkcap_check.py BASELINE.csv FRESH.csv PF1_MOVERS.csv

Row reading and the deadline rule are DN-1d r3's census_check.py (../../2026-11-m8-dn1d-singular/r3/legs/), unchanged:
  * 13 columns (cell_id .. kkt_residual); '#' lines are comments.
  * DEADLINE-BOUND cells (baseline status dnf_*, and f7_n10000_path_physics) pass iff fresh is dnf_* or Optimal.
  * every other cell's REFERENCE row is PF-1's declared row if it is one of PF-1's two movers, else the baseline row.
What this criterion adds, fixed before data:
  * a non-deadline cell whose fresh row differs from its reference is a WALK-CAP MOVER, printed with its direction:
      WORSE    reference status Optimal, fresh status not Optimal                          -> FAIL
      BETTER   reference status not Optimal, fresh status Optimal
      NEUTRAL  any other change (same status, counters moved; or one non-Optimal status for another)
  * CENSUS_WALKCAP_PASS (exit 0) iff the roster matches, every deadline cell passes, and no mover is WORSE;
    else CENSUS_WALKCAP_FAIL (exit 1). Every mover, of every direction, is listed for the report and the ruling.
"""
import csv
import sys

PHYSICS = "f7_n10000_path_physics"
NCOL = 13
COLS = ["cell_id", "family", "n_nodes", "window", "taxonomy", "degenerate", "status", "factorizations", "qp_minors",
        "escapes", "qp_subproblems", "qp_fact_per_qp", "kkt_residual"]


def rows(path):
    with open(path) as f:
        lines = [ln for ln in f if not ln.startswith("#") and ln.strip()]
    r = list(csv.reader(lines))
    if r and r[0][0] == "cell_id":
        r = r[1:]
    return {row[0]: row[:NCOL] for row in r}


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    base, fresh, pf1 = rows(sys.argv[1]), rows(sys.argv[2]), rows(sys.argv[3])
    ok = True
    if set(base) != set(fresh):
        print("ROSTER_DIFFERS missing=%s extra=%s" % (sorted(set(base) - set(fresh)), sorted(set(fresh) - set(base))))
        ok = False
    tally = {"WORSE": 0, "BETTER": 0, "NEUTRAL": 0}
    n_exact = n_deadline = 0
    for cell in sorted(base):
        if cell not in fresh:
            continue
        b, f = base[cell], fresh[cell]
        if b[6].startswith("dnf_") or cell == PHYSICS:
            n_deadline += 1
            if f == b:
                continue
            good = f[6].startswith("dnf_") or f[6] == "Optimal"
            print("DEADLINE_CELL %s %s: base %s -> fresh %s" % ("ok" if good else "FAIL", cell, ",".join(b[6:]),
                                                                  ",".join(f[6:])))
            ok = ok and good
            continue
        ref = pf1.get(cell, b)
        if f == ref:
            n_exact += 1
            continue
        if ref[6] == "Optimal" and f[6] != "Optimal":
            d = "WORSE"
        elif ref[6] != "Optimal" and f[6] == "Optimal":
            d = "BETTER"
        else:
            d = "NEUTRAL"
        tally[d] += 1
        moved = [f"{COLS[i]} {ref[i]} -> {f[i]}" for i in range(6, NCOL) if ref[i] != f[i]]
        print("WALKCAP_MOVER %s %s (reference: %s): %s" % (d, cell, "PF-1 declared row" if cell in pf1 else "baseline",
                                                          "; ".join(moved)))
        if d == "WORSE":
            ok = False
    print("cells=%d exact=%d movers=%d (worse %d, better %d, neutral %d) deadline_bound=%d" % (
        len(base), n_exact, sum(tally.values()), tally["WORSE"], tally["BETTER"], tally["NEUTRAL"], n_deadline))
    print("CENSUS_WALKCAP_PASS" if ok else "CENSUS_WALKCAP_FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
