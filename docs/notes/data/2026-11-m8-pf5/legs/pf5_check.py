#!/usr/bin/env python3
"""PF-5 (../DECLARATION-PF-5.md; r1: ../DECLARATION-PF-5-r1.md adds the plain rule for rows carrying none of the
three PF-5 columns -- the hven-ipm lane): the row identity check of the TRAIN (DN-3's 238 roster cells x D/K) and nlp-35 legs.

Row loading and comparison are train_check.py's load()/compare() (imported from this directory, byte copies of PF-4
r2's), with one rule of PF-5's own on top:
  - the three PF-5 movers are excluded from the plain comparison: `side.symbolic_analysis_reuses` (new; absent in arm A),
    `side.backend_symbolic_analyses` and `side.backend_symbolic_repeats` (fall by the hits);
  - instead each row must satisfy, exactly (R = B's side.symbolic_analysis_reuses; d = A.backend_symbolic_analyses -
    B.backend_symbolic_analyses, the platform analyses the sparse-arm hits replaced):
        0 <= d <= R   and   A.backend_symbolic_repeats - B.backend_symbolic_repeats == d
    (a sparse-arm hit replaces one platform analysis, a repeat of one earlier in the same SQP solve; a dense-arm hit
    replaces only the dense session's set-up, which neither backend counter sees, so R - d is the dense-arm hits);
  - every other key, the lane exit code and the .optdump equal (residual-class values within train_check's calibrated
    gate, listed); a missing or capped row fails.
usage: pf5_check.py train <A dir> <B dir> <roster.tsv>      (cells/<tag>.<D|K>)
       pf5_check.py nlp   <A dir> <B dir> <cell> ...        (nlp-hven-sqp-<cell>.D)
Prints ROWS_AS_DECLARED (exit 0) or ROWS_FAIL (exit 1), and the hit totals."""
import importlib.util
import os
import sys

_spec = importlib.util.spec_from_file_location("tc", os.path.join(os.path.dirname(os.path.abspath(__file__)), "train_check.py"))
tc = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(tc)

R, AN, RP = "side.symbolic_analysis_reuses", "side.backend_symbolic_analyses", "side.backend_symbolic_repeats"


def check(pa, pb):
    a, oka = tc.load(pa)
    b, okb = tc.load(pb)
    if not (oka and okb):
        return [f"NO_ROW_OR_CAPPED (A {oka}, B {okb})"], [], (0, 0)
    if all(k not in a and k not in b for k in (R, AN, RP)):
        # r1: a row that carries none of the three (the hven-ipm lane reports no SqpCounters) compares plainly.
        ex, ig, og = tc.compare(b, a)
        da = open(pa + ".optdump").read() if os.path.exists(pa + ".optdump") else None
        db = open(pb + ".optdump").read() if os.path.exists(pb + ".optdump") else None
        if da is None or da != db:
            ex.append("OPTDUMP_DIFFERS")
        return ex + og, ig, (0, 0)
    if R not in b:
        return ["B_LACKS_" + R], [], (0, 0)
    reuses = int(b[R])
    fails = []
    if any(k not in a or k not in b for k in (AN, RP)):
        fails.append("MISSING_BACKEND_COUNTER")
    else:
        d = int(a[AN]) - int(b[AN])
        if not (0 <= d <= reuses) or int(a[RP]) - int(b[RP]) != d:
            fails.append(f"IDENTITY: A an/rep {a[AN]}/{a[RP]} B an/rep {b[AN]}/{b[RP]} reuses {reuses}")
    a2 = {k: v for k, v in a.items() if k not in (R, AN, RP)}
    b2 = {k: v for k, v in b.items() if k not in (R, AN, RP)}
    ex, ig, og = tc.compare(b2, a2)
    da = open(pa + ".optdump").read() if os.path.exists(pa + ".optdump") else None
    db = open(pb + ".optdump").read() if os.path.exists(pb + ".optdump") else None
    if da is None or da != db:
        ex.append("OPTDUMP_DIFFERS")
    return fails + ex + og, ig, (reuses, int(a.get(AN, 0)) - int(b.get(AN, 0)) if not fails else 0)


def main():
    mode, adir, bdir = sys.argv[1], sys.argv[2], sys.argv[3]
    rows = []
    if mode == "train":
        roster = [l.rstrip("\n").split("\t") for l in open(sys.argv[4]) if l.strip()]
        assert len(roster) == 238, len(roster)
        for pop, lane, prob in roster:
            for arm in ("D", "K"):
                name = f"{pop}-{lane}-{prob}.{arm}"
                rows.append((name, os.path.join(adir, "cells", name), os.path.join(bdir, "cells", name)))
    else:
        for c in sys.argv[4:]:
            name = f"nlp-hven-sqp-{c}.D"
            rows.append((name, os.path.join(adir, name), os.path.join(bdir, name)))
    nfail, ingate, total, sparse, hit_rows = 0, 0, 0, 0, 0
    for name, pa, pb in rows:
        diffs, ig, (reuses, d) = check(pa, pb)
        sparse += d
        ingate += len(ig)
        for x in ig:
            print(f"INGATE {name} {x}")
        total += reuses
        hit_rows += reuses > 0
        if diffs:
            nfail += 1
            print(f"FAIL {name}: " + "; ".join(diffs[:10]))
    print(f"SUMMARY rows {len(rows)}; failing {nfail}; residual in-gate non-byte-equal values {ingate}; "
          f"rows with hits {hit_rows}; total symbolic_analysis_reuses {total} (sparse-arm, = platform analyses saved: "
          f"{sparse}; dense-arm: {total - sparse})")
    print("ROWS_AS_DECLARED" if nfail == 0 else "ROWS_FAIL")
    return 0 if nfail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
