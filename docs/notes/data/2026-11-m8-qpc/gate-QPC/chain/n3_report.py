#!/usr/bin/env python3
"""N3 (DECLARATION-QPC section 6, settler ruling C584(1)): H and R on CENSUS-57 against the M7-close baseline (CEN-R3).
usage: n3_report.py <H run dir> <R run dir>   (each: run_walk_census.sh --out dir; H dir also has qpc-side.csv)
Lists (from census_compare.py's `MISMATCH <cell>` lines): R mismatches (PRE-EXISTING), H mismatches, H-ONLY mismatches (the only list
that counts against N3's bar: an H-only cell whose own side line has both counters 0 FAILS; one with a non-zero counter is a declared
MOVER, listed). Also CENSUS-57 P-POWER: cells with converged_refinement_arms > 0 / certificate_gate_refusals > 0 under H.
rc 0 = no H-only difference on a both-counters-0 cell; 1 otherwise. A cell on that list must be re-run ALONE before it is called a
deviation (CLAUDE.md section 7)."""
import os, re, sys
def mism(d):
    p = os.path.join(d, "compare.txt")
    return {m.group(1) for m in (re.match(r"MISMATCH (\S+)", l) for l in open(p)) if m}, open(p).read().strip().splitlines()[-1]
H, R = sys.argv[1], sys.argv[2]
mh, th = mism(H); mr, tr = mism(R)
side = {}
sp = os.path.join(H, "qpc-side.csv")
for l in open(sp) if os.path.exists(sp) else []:
    c, a, g = l.strip().split(","); side[c] = (int(a), int(g))
print("R vs baseline:", tr); print("H vs baseline:", th)
print("PRE-EXISTING (R shows it too, %d): %s" % (len(mh & mr), sorted(mh & mr)))
print("R-only mismatches (%d): %s" % (len(mr - mh), sorted(mr - mh)))
bad = []
for c in sorted(mh - mr):
    s = side.get(c)
    if s is None: print("H-ONLY %s: NO SIDE LINE -> counts against N3" % c); bad.append(c)
    elif s == (0, 0): print("H-ONLY %s: both counters 0 -> counts against N3 (re-run ALONE to confirm)" % c); bad.append(c)
    else: print("H-ONLY MOVER %s: converged_refinement_arms=%d certificate_gate_refusals=%d" % ((c,) + s))
print("side lines: %d cells; C1 exercised %d, C2 exercised %d" % (len(side), sum(1 for a, g in side.values() if a > 0), sum(1 for a, g in side.values() if g > 0)))
print("POWER CENSUS-57: C1_exercised=%d C2_exercised=%d (of %d cells)" % (sum(1 for a, g in side.values() if a > 0), sum(1 for a, g in side.values() if g > 0), len(side)))
print("N3 %s: %d H-only differences on both-counters-0 cells" % ("FAIL" if bad else "PASS", len(bad)))
sys.exit(1 if bad else 0)
