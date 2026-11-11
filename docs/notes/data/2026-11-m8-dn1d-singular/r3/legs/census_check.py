#!/usr/bin/env python3
# M8 DN-1d r3 (../../DECLARATION-r3.txt): r2's copy of this file re-pointed (r3/, build-dn1de3/); older provenance lines below are historical.
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 / PF-1 (see legs/*.sh line 2), byte for byte below this line.
"""M8 PF-1 (DECLARATION P5): the census criterion, applied mechanically.

usage: census_check.py BASELINE.csv FRESH.csv MOVERS.csv

  * MOVERS.csv: the declared movers, one 13-column row each (cell_id .. kkt_residual), in the census
    schema; lines starting with '#' are comments.
  * DEADLINE-BOUND cells: every cell whose baseline status is dnf_*, and f7_n10000_path_physics. Their
    status is wall-clock dependent (brief section 0) and is not asserted evidence: such a cell passes iff
    its fresh status is dnf_* or Optimal (a deadline outcome or a solve); its counters are not compared.
  * every other cell: a declared mover must equal its declared row EXACTLY on the 13 columns; a cell
    that is not declared must equal the baseline EXACTLY on the 13 columns.
Prints one line per cell that is not an exact baseline match, then CENSUS_AS_DECLARED (exit 0) or
CENSUS_NOT_AS_DECLARED (exit 1).
"""
import csv
import sys

PHYSICS = "f7_n10000_path_physics"
NCOL = 13


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
    base, fresh, movers = rows(sys.argv[1]), rows(sys.argv[2]), rows(sys.argv[3])
    ok = True
    if set(base) != set(fresh):
        print("ROSTER_DIFFERS missing=%s extra=%s" % (sorted(set(base) - set(fresh)), sorted(set(fresh) - set(base))))
        ok = False
    for m in movers:
        if m not in base:
            print("DECLARED_MOVER_NOT_IN_BASELINE %s" % m)
            ok = False
    n_exact = n_mover = n_deadline = 0
    for cell in sorted(base):
        if cell not in fresh:
            continue
        b, f = base[cell], fresh[cell]
        deadline = b[6].startswith("dnf_") or cell == PHYSICS
        if deadline:
            n_deadline += 1
            if f == b:
                continue
            good = f[6].startswith("dnf_") or f[6] == "Optimal"
            print("DEADLINE_CELL %s %s: base %s -> fresh %s" % ("ok" if good else "FAIL", cell, ",".join(b[6:]), ",".join(f[6:])))
            ok = ok and good
            continue
        if cell in movers:
            n_mover += 1
            good = f == movers[cell]
            print("DECLARED_MOVER %s %s: base %s -> fresh %s (declared %s)" % (
                "ok" if good else "FAIL", cell, ",".join(b[6:]), ",".join(f[6:]), ",".join(movers[cell][6:])))
            ok = ok and good
            continue
        if f != b:
            print("UNDECLARED_MOVER FAIL %s: base %s -> fresh %s" % (cell, ",".join(b[6:]), ",".join(f[6:])))
            ok = False
        else:
            n_exact += 1
    print("cells=%d exact=%d declared_movers=%d deadline_bound=%d" % (len(base), n_exact, n_mover, n_deadline))
    print("CENSUS_AS_DECLARED" if ok else "CENSUS_NOT_AS_DECLARED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
