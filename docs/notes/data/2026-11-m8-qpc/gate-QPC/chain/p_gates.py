#!/usr/bin/env python3
"""P-POWER (SENS-Q half) and P-GATED (DECLARATION-QPC section 4) from the leg N0 grid CSVs.
usage: p_gates.py <N0 root (<root>/<arm>/{R,A}-{refactorize,border}.csv)> <out gated.tsv>
P-GATED = cells (dump, algebra) where R shows dual_regularization_decreases > 0, OR a kOptimal exit whose data-half reading fails
(cmp_sensq.vqp_failures, which includes the pinned-optimum check -- the only data-half reading the declared code has), OR
optimal_row_gray_exits > 0, OR an exit the H data half refuses (certificate_gate_refusals > 0 under QPC, the arm that carries C2).
P-POWER = per arm, SENS-Q cells with converged_refinement_arms > 0 / certificate_gate_refusals > 0 (C1 / C2 exercised)."""
import os, sys
here = os.path.dirname(os.path.abspath(__file__))
legs = os.path.normpath(os.path.join(here, "..", "..", "legs"))
sys.path.insert(0, legs)
import cmp_sensq as cs
root, out = sys.argv[1], sys.argv[2]
pinned = cs.load_pinned(os.path.join(legs, "pinned_optima.tsv"))
gated = {}
for alg in ("refactorize", "border"):
    R = cs.load(os.path.join(root, "QPC", "R-%s.csv" % alg))
    H = cs.load(os.path.join(root, "QPC", "A-%s.csv" % alg))
    for key, r in sorted(R.items()):
        why = []
        if float(r.get("dual_regularization_decreases", 0) or 0) > 0: why.append("R:dual_regularization_decreases")
        if r["status"] == "0" and cs.vqp_failures(key, r, pinned): why.append("R:kOptimal-data-half-fails")
        if float(r.get("optimal_row_gray_exits", 0) or 0) > 0: why.append("R:optimal_row_gray_exits")
        h = H.get(key)
        if h and float(h["certificate_gate_refusals"]) > 0: why.append("QPC:data-half-refuses")
        if why: gated[key] = why
with open(out, "w") as f:
    f.write("# P-GATED: cells gated in (R and QPC member-0 grid readings, leg N0); columns: dump, algebra, reasons\n")
    for (d, a), why in sorted(gated.items()): f.write("%s\t%s\t%s\n" % (d, a, ",".join(why)))
print("P-GATED cells gated in: %d of %d (dump x algebra)" % (len(gated), 2 * len(R)))
for arm in ("QPC", "QPC-C1", "QPC-C2"):
    for alg in ("refactorize", "border"):
        H = cs.load(os.path.join(root, arm, "A-%s.csv" % alg))
        c1 = sum(1 for r in H.values() if float(r["converged_refinement_arms"]) > 0)
        c2 = sum(1 for r in H.values() if float(r["certificate_gate_refusals"]) > 0)
        print("POWER SENS-Q %-7s %-11s cells=%d C1_exercised=%d C2_exercised=%d" % (arm, alg, len(H), c1, c2))
