#!/usr/bin/env python3
"""QPC leg N2 (DECLARATION-QPC section 6): member-0 rows of an arm against H0 (the census rows copied into the QPC store),
cell by cell, with the QPC declared-mover rule the harness's `i19_run.py neutrality` does not have.

  python3 n2_qpc.py <hven-bench checkout at 27c02687d9389ed43f3dbef27b4cbe389ca2b5d0> <qpc store> <BINARIES.txt> <arm> [corpora, default TRAIN-238,CAP-238-1,CAP-238-01,NLP-46]

A cell whose ARM side file shows converged_refinement_arms > 0 or certificate_gate_refusals > 0 is a DECLARED MOVER (listed
with both counters, both statuses and the work columns). Every other cell must agree with H0 on every row column except
wall_s and options_digest and on every side column present in both (the harness's own compare_arm_rows: counters and statuses
exact, measure columns by the schema's near-ulp gate); the arm's only-in-arm side columns must be the two QPC counters.
Cells present on one side only, an incomplete corpus, or an undeclared arm-only column are differences. rc 0 iff no
difference; 1 otherwise (a STOP). PILOT-62 carries no side counters (pilot_rows.csv), so it is not read here: its rows are
the V2 certificate leg's.
"""
import os
import sys

NEW = ("converged_refinement_arms", "certificate_gate_refusals")


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    bench, store, binaries, arm = os.path.realpath(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
    corpora = (sys.argv[5] if len(sys.argv) > 5 else "TRAIN-238,CAP-238-1,CAP-238-01,NLP-46").split(",")
    sys.path.insert(0, os.path.join(bench, "tu"))
    import i19_cells
    import i19_run
    spec = i19_run.load_spec()
    i19_cells.register_binary_arms(spec, i19_cells.read_declared_binaries(binaries))
    allowed = set(spec["neutrality"]["N2"]["differ_columns_allowed"])
    diffs, movers, n = [], [], 0
    for corpus in corpora:
        a, b = i19_run.cell_table(store, corpus, "H0"), i19_run.cell_table(store, corpus, arm)
        want = len(i19_cells.cells_of(spec, corpus))
        for side, t in (("H0", a), (arm, b)):
            if len(t) != want:
                diffs.append((corpus, "-", "corpus incomplete on %s: %d of %d" % (side, len(t), want)))
        for cid in sorted(set(a) | set(b)):
            if cid not in a or cid not in b:
                diffs.append((corpus, cid, "present on one side only"))
                continue
            n += 1
            (ra, sa), (rb, sb) = a[cid], b[cid]
            sb = sb or {}
            got = {c: sb.get(c) for c in NEW}
            if any(v is None for v in got.values()):
                diffs.append((corpus, cid, "arm side row lacks the QPC counters %s" % got))
                continue
            if any(int(float(v)) != 0 for v in got.values()):
                movers.append((corpus, cid, ra.get("status"), rb.get("status"), got))
                continue
            d, _, _, und = i19_run.compare_arm_rows({cid: a[cid]}, {cid: b[cid]}, allowed, list(NEW), "H0", arm)
            diffs += [(corpus,) + t for t in d]
            diffs += [(corpus, cid, "undeclared arm-only column %s" % u) for u in und]
    for m in movers:
        print("MOVER %s %s status H0=%s %s=%s %s" % (m[0], m[1], m[2], arm, m[3], m[4]))
    for d in diffs:
        print("DIFF", d)
    print("N2 %s: %d cells compared, %d declared movers, %d differences" % (arm, n, len(movers), len(diffs)))
    return 1 if diffs else 0


if __name__ == "__main__":
    sys.exit(main())
