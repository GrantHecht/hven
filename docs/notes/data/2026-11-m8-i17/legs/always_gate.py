#!/usr/bin/env python3
"""I17 leg P4: the Always-forced TRAIN measurement (item 3) and its identity criteria, on DN-3's hven-sqp TRAIN cells (the Always labels exist on the SQP lane only).
Directories hold cells/<tag>.<ARM> from train_enum.sh; A = the base 64e384e2a, B = the I17 code.
  A/cells: X = always-default                          B/cells: X = always-default (the adopted clamp), N = default (kAuto),
                                                                 A<k> = cap<k>-always-default, N<k> = cap<k>-default, k in K_SET
Criteria (printed, exit 0 iff all hold):
  C1 no row of B.X differs from A.X in any key but the ledger-derived harness columns (`side.harness_qp*`); the ledger-only rows are exactly MOVERS;
     the .optdump of B.X is A.X's plus the four new option lines.
  C2 AUTO UNMOVED: for every k in K_SET, N<k> equals N on every .out key (the cap is not read under kAuto).
  C3 THE ADOPTION RULE: the smallest k in K_SET whose A<k> equals B.X on every .out key is ADOPT_K (2^10), and no A<k> at or above it differs.
  C4 recorded, not asserted: the rows each smaller k moves, with direction (status rank optimal > acceptable > other).
usage: always_gate.py A_DIR B_DIR DN3_DIR MOVERS.txt ADOPT_K"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_check as t  # noqa: E402

K_SET = [6, 8, 10, 13, 16, 20]
RANK = {"optimal": 2, "acceptable": 1}


def outdiff(a, b):
    ra, oa = t.load(a)
    rb, ob = t.load(b)
    if not (oa and ob):
        return ["NO_ROW_OR_CAPPED"]
    ex, _, og = t.compare(ra, rb)
    return ex + og


def main():
    adir, bdir, d3, movf, adopt = sys.argv[1:5] + [sys.argv[5]]
    adopt = int(adopt)
    declared = {l.split("#", 1)[0].strip() for l in open(movf) if l.split("#", 1)[0].strip()}
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
    cells = [f"{p}-{l}-{q}" for p, l, q in roster if l == "hven-sqp"]
    pa = lambda c, tg: os.path.join(adir, "cells", f"{c}.{tg}")
    pb = lambda c, tg: os.path.join(bdir, "cells", f"{c}.{tg}")
    ok = True
    # C1
    ledger, other, missing = [], [], []
    for c in cells:
        eq, new, old, diffs, _ = t.row_equal(pb(c, "X"), pa(c, "X"), True)
        if eq is None:
            missing.append(c)
        elif not eq:
            (ledger if all(d.split(":")[0].startswith("side.harness_qp") for d in diffs) else other).append(c)
    print(f"C1 cells {len(cells)}; missing/capped {len(missing)}; ledger-only movers {len(ledger)}; other movers {len(other)}")
    for c in other:
        print("  FAIL OTHER_MOVER", c)
    und = sorted(set(ledger) - declared)
    absent = sorted(declared - set(ledger))
    for c in und:
        print("  FAIL UNDECLARED_LEDGER_MOVER", c)
    for c in absent:
        print("  FAIL DECLARED_LEDGER_MOVER_DID_NOT_MOVE", c)
    ok &= not (missing or other or und or absent)
    # C2
    for k in K_SET:
        mv = [c for c in cells if outdiff(pb(c, f"N{k}"), pb(c, "N"))]
        print(f"C2 Auto at cap 2^{k}: rows differing from Auto default: {len(mv)}")
        for c in mv:
            print("  FAIL AUTO_MOVED", c)
        ok &= not mv
    # C3 / C4
    adopted = None
    for k in K_SET:
        mv = [c for c in cells if outdiff(pb(c, f"A{k}"), pb(c, "X"))]
        if not mv and adopted is None:
            adopted = k
        tally = {"BETTER": 0, "WORSE": 0, "NEUTRAL": 0}
        for c in mv:
            a_, _ = t.load(pb(c, "X"))
            b_, _ = t.load(pb(c, f"A{k}"))
            ra, rb = RANK.get(a_.get("status"), 0), RANK.get(b_.get("status"), 0)
            d = "BETTER" if rb > ra else "WORSE" if rb < ra else "NEUTRAL"
            tally[d] += 1
            print(f"  C4 cap 2^{k} {d:7s} {c}: {a_.get('status')}/{a_.get('iterations')} -> {b_.get('status')}/{b_.get('iterations')}")
        print(f"C3 Always at cap 2^{k}: rows differing from the adopted default: {len(mv)} {tally}")
        if adopted is not None and k >= adopted and mv:
            print("  FAIL ADOPTED_OR_LARGER_MOVES", k)
            ok = False
    print(f"C3 smallest cap with no moved row: 2^{adopted}; declared ADOPT_K 2^{adopt}")
    ok &= adopted == adopt
    print("I17_ALWAYS_PASS" if ok else "I17_ALWAYS_FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
