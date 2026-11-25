#!/usr/bin/env python3
"""QPC SENS-Q comparator (DECLARATION-QPC sections 5.2 V-QP and 6 N0/Q) over hven_qp_dump_grid CSVs.

  cmp_sensq.py n0  R.csv H.csv        -- N0: every (file, algebra) cell whose H row has converged_refinement_arms == 0 AND
                                         certificate_gate_refusals == 0 must equal R's row on every column except those two
                                         (R prints NA there). Cells present on one side only are differences. Movers (a new
                                         counter non-zero) are listed with their counters and both statuses. rc 1 on any
                                         difference outside the declared-mover rule, 0 otherwise.
  cmp_sensq.py vqp X.csv pinned.tsv   -- V-QP: every kOptimal (status 0) row re-checked from the grid's INDEPENDENT readings:
                                         d_rows <= 10 feas_tol, d_product / d_dual_scale <= 100 feas_tol, d_stationarity <= 10 opt_tol,
                                         k_stationarity_beyond_rounding <= 10 opt_tol, k_sign <= opt_tol (the dump's own
                                         tolerances), and |f - f*| <= 1e-9 max(1, |f*|) where pinned.tsv pins f*. A NaN fails.
                                         rc 1 if any kOptimal row fails, 0 otherwise. Prints every failing row.
"""
import csv
import math
import sys

NEW = ("converged_refinement_arms", "certificate_gate_refusals")


def load(path):
    with open(path, newline="") as f:
        rows = list(csv.DictReader(f))
    out = {}
    for r in rows:
        key = (r["file"], r["algebra"])
        if key in out:
            sys.exit(f"STOP: duplicate cell {key} in {path}")
        out[key] = r
    return out


def n0(rp, hp):
    R, H = load(rp), load(hp)
    bad, movers = [], []
    for key in sorted(set(R) | set(H)):
        if key not in R or key not in H:
            bad.append(f"ONE-SIDED {key} R={'yes' if key in R else 'no'} H={'yes' if key in H else 'no'}")
            continue
        r, h = R[key], H[key]
        if any(h[c] == "NA" for c in NEW):
            sys.exit(f"STOP: the H CSV {hp} lacks the QPC counters (NA): not an H build")
        mover = any(int(h[c]) != 0 for c in NEW)
        if mover:
            movers.append(f"MOVER {key[0]} {key[1]} status R={r['status']} H={h['status']} arms={h[NEW[0]]} "
                          f"gate={h[NEW[1]]} minors R={r['minor_iters'] if 'minor_iters' in r else '?'} "
                          f"H={h['minor_iters'] if 'minor_iters' in h else '?'} f R={r['objective']} H={h['objective']}")
            continue
        diff = [c for c in r if c not in NEW and r[c] != h.get(c)]
        if diff:
            bad.append(f"DIFF {key[0]} {key[1]} columns={diff[:12]}{'...' if len(diff) > 12 else ''}")
    for m in movers:
        print(m)
    for b in bad:
        print(b)
    print(f"N0 cells={len(set(R) | set(H))} movers={len(movers)} differences={len(bad)}")
    return 1 if bad else 0


def vqp(xp, pinned_path):
    X = load(xp)
    pinned = {}
    with open(pinned_path) as f:
        for ln in f:
            if ln.startswith("#") or not ln.strip():
                continue
            name, val = ln.split("\t")[:2]
            pinned[name] = float(val)
    fails, n_opt = [], 0
    for key, r in sorted(X.items()):
        if r["status"] != "0":
            continue
        n_opt += 1
        g = lambda c: float(r[c])
        ftol, otol = g("feas_tol"), g("opt_tol")
        checks = [("d_rows", g("d_rows"), 10 * ftol), ("d_product/d_dual_scale", g("d_product") / g("d_dual_scale"), 100 * ftol),
                  ("d_stationarity", g("d_stationarity"), 10 * otol),
                  ("k_stationarity_beyond_rounding", g("k_stationarity_beyond_rounding"), 10 * otol),
                  ("k_sign", g("k_sign"), otol)]
        if key[0] in pinned:
            fs = pinned[key[0]]
            checks.append(("objective_vs_pinned", abs(g("objective") - fs), 1e-9 * max(1.0, abs(fs))))
        why = [f"{n}={v:.3e}>{t:.3e}" for n, v, t in checks if not (v <= t)]  # NaN fails
        if why:
            fails.append(f"FALSE-CERTIFICATE {key[0]} {key[1]} " + " ".join(why))
    for x in fails:
        print(x)
    print(f"V-QP kOptimal={n_opt} pinned_present={sum(1 for k in X if k[0] in pinned)} failures={len(fails)}")
    return 1 if fails else 0


def main():
    if len(sys.argv) != 4 or sys.argv[1] not in ("n0", "vqp"):
        print(__doc__)
        return 2
    return n0(sys.argv[2], sys.argv[3]) if sys.argv[1] == "n0" else vqp(sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    sys.exit(main())
