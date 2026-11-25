#!/usr/bin/env python3
"""QPC SENS-Q comparator (DECLARATION-QPC sections 5.2 V-QP and 6 N0/Q) over hven_qp_dump_grid CSVs.

  cmp_sensq.py n0  R.csv H.csv        -- N0: every (file, algebra) cell whose H row has converged_refinement_arms == 0 AND
                                         certificate_gate_refusals == 0 must equal R's row on every column except those two
                                         (R prints NA there). Cells present on one side only are differences. Movers (a new
                                         counter non-zero) are listed with their counters and both statuses. rc 1 on any
                                         difference outside the declared-mover rule, 0 otherwise.
  cmp_sensq.py vqp X.csv pinned.tsv   -- V-QP: every kOptimal (status 0) row re-checked from the grid's INDEPENDENT readings:
                                         d_rows <= 10 feas_tol, d_product_local (each term over its row's local dual scale,
                                         settler C567) <= 100 feas_tol, d_off_working == 0, d_pslack_rel (the slack of
                                         working rows / pinned bounds under a non-rounding-zero multiplier, settler C568)
                                         <= 100 feas_tol, k_stationarity_beyond_rounding
                                         (qp_dump_kkt, rounding floor c_i u m_i) <= 10 opt_tol, k_sign <= opt_tol (the dump's
                                         own tolerances), and |f - f*| within pinned.tsv's per-cell tolerance (the pinning test's own bound). A NaN
                                         fails. The NORMWISE stationarity d_stationarity is a REPORTED column (printed per
                                         kOptimal row as d_stat/otol), never a verdict: a normwise residual can sit beneath
                                         double's rounding floor where large terms cancel (C11 SAWPATH qp1 reads ~502 opt_tol
                                         at its certified exit, test_elastic_witness_start.cpp). rc 1 if any kOptimal row
                                         fails, 0 otherwise. Prints every failing row.
  cmp_sensq.py honest R.csv A.csv pinned.tsv
                                      -- V-HONEST (section 5.2): every R kOptimal row that passes every V-QP check must be
                                         kOptimal in A with |f_A - f_R| <= 1e-9 max(1, |f_R|). rc 1 on any exception.
  cmp_sensq.py m1 X.csv               -- the M1 development reading (DECLARATION section 4): every kOptimal row whose
                                         UNSCALED product d_product > 100 feas_tol, with d_rows/ftol, the worst term's kind
                                         and index, its slack in ftol (d_worst_rel / ftol), its multiplier over its local
                                         scale, and its local product in ftol; flags FOOTPRINT-SLACK where the slack exceeds
                                         100 ftol; and every kOptimal row with d_pslack_rel > 100 ftol (M1-PRICED-SLACK,
                                         the C568 slack test's subject; 0 expected at H). rc 0 always (a reading).
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
    pinned = load_pinned(pinned_path)
    fails, n_opt = [], 0
    for key, r in sorted(X.items()):
        if r["status"] != "0":
            continue
        n_opt += 1
        g = lambda c: float(r[c])
        otol = g("opt_tol")
        print(f"REPORT {key[0]} {key[1]} d_stat/otol={g('d_stationarity') / otol:.3e} "
              f"k_beyond/otol={g('k_stationarity_beyond_rounding') / otol:.3e} "
              f"unscaled_product/ftol={g('d_product') / g('feas_tol'):.3e}")
        why = vqp_failures(key, r, pinned)
        if why:
            fails.append(f"FALSE-CERTIFICATE {key[0]} {key[1]} " + " ".join(why))
    for x in fails:
        print(x)
    print(f"V-QP kOptimal={n_opt} pinned_present={sum(1 for k in X if k[0] in pinned)} failures={len(fails)}")
    return 1 if fails else 0


def load_pinned(pinned_path):
    """file -> (f*, kind, t): kind 'rel' = |f - f*| <= t max(1, |f*|), 'abs' = |f - f*| <= t (the pinning test's bound)."""
    pinned = {}
    with open(pinned_path) as f:
        for ln in f:
            if ln.startswith("#") or not ln.strip():
                continue
            name, val, tol = ln.rstrip("\n").split("\t")[:3]
            kind, t = tol.split(":")
            if kind not in ("rel", "abs"):
                sys.exit(f"STOP: bad tolerance {tol!r} for {name} in {pinned_path}")
            pinned[name] = (float(val), kind, float(t))
    return pinned


def vqp_failures(key, r, pinned):
    """The V-QP checks of one kOptimal row; [] when it passes. A NaN fails."""
    g = lambda c: float(r[c])
    ftol, otol = g("feas_tol"), g("opt_tol")
    checks = [("d_rows", g("d_rows"), 10 * ftol), ("d_product_local", g("d_product_local"), 100 * ftol),
              ("d_off_working", g("d_off_working"), 0.0), ("d_pslack_rel", g("d_pslack_rel"), 100 * ftol),
              ("k_stationarity_beyond_rounding", g("k_stationarity_beyond_rounding"), 10 * otol),
              ("k_sign", g("k_sign"), otol)]
    if key[0] in pinned:
        fs, kind, t = pinned[key[0]]
        checks.append(("objective_vs_pinned", abs(g("objective") - fs), t * max(1.0, abs(fs)) if kind == "rel" else t))
    return [f"{n}={v:.3e}>{t:.3e}" for n, v, t in checks if not (v <= t)]  # NaN fails


def honest(rp, ap, pinned_path):
    """V-HONEST: an R kOptimal row passing every V-QP check must stay kOptimal under the arm, same objective."""
    R, A, pinned = load(rp), load(ap), load_pinned(pinned_path)
    n_honest, bad = 0, []
    for key, r in sorted(R.items()):
        if r["status"] != "0" or vqp_failures(key, r, pinned):
            continue
        n_honest += 1
        a = A.get(key)
        if a is None:
            bad.append(f"HONEST-MISSING {key[0]} {key[1]}")
            continue
        fr = float(r["objective"])
        fa = float(a["objective"]) if a["status"] == "0" else float("nan")
        if a["status"] != "0" or not (abs(fa - fr) <= 1e-9 * max(1.0, abs(fr))):
            bad.append(f"HONEST-REFUSED {key[0]} {key[1]} status R=0 A={a['status']} f R={r['objective']} "
                       f"A={a['objective']} gate={a.get('certificate_gate_refusals')} arms={a.get('converged_refinement_arms')}")
    for b in bad:
        print(b)
    print(f"V-HONEST honest_R={n_honest} exceptions={len(bad)}")
    return 1 if bad else 0


def m1(xp):
    X = load(xp)
    n_opt = n_hit = n_foot = n_foot_any = n_pslack = 0
    for key, r in sorted(X.items()):
        if r["status"] != "0":
            continue
        n_opt += 1
        g = lambda c: float(r[c])
        ftol = g("feas_tol")
        if g("d_pslack_rel") / ftol > 100.0 or g("d_pslack_rel") != g("d_pslack_rel"):
            print(f"M1-PRICED-SLACK {key[0]} {key[1]} pslack/ftol={g('d_pslack_rel') / ftol:.3e}")
            n_pslack += 1
        big_slack = g("d_slack_rel") / ftol
        if big_slack > 100.0:
            print(f"M1-SLACK {key[0]} {key[1]} term={r['d_slack_kind']}{r['d_slack_index']} slack/ftol={big_slack:.3e} "
                  f"mult={g('d_slack_mult'):.3e} local={g('d_slack_local'):.3e} "
                  f"mult/local={g('d_slack_mult') / g('d_slack_local'):.3e} FOOTPRINT-SLACK")
            n_foot_any += 1
        if not (g("d_product") > 100 * ftol):
            continue
        n_hit += 1
        slack = g("d_worst_rel") / ftol
        foot = not (slack <= 100.0)
        n_foot += foot
        loc = g("d_worst_local")
        print(f"M1 {key[0]} {key[1]} unscaled/ftol={g('d_product') / ftol:.3e} rows/ftol={g('d_rows') / ftol:.3e} "
              f"term={r['d_worst_kind']}{r['d_worst_index']} slack/ftol={slack:.3e} mult={g('d_worst_mult'):.3e} "
              f"local={loc:.3e} mult/local={g('d_worst_mult') / loc if loc > 0 else float('nan'):.3e} "
              f"local_product/ftol={g('d_product_local') / ftol:.3e}{' FOOTPRINT-SLACK' if foot else ''}")
    print(f"M1 kOptimal={n_opt} unscaled>100ftol={n_hit} footprint_slack_on_worst_term(>100ftol)={n_foot} "
          f"any_priced_term_slack>100ftol={n_foot_any} non_rounding_zero_slack>100ftol={n_pslack}")
    return 0


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "m1":
        return m1(sys.argv[2])
    if len(sys.argv) == 5 and sys.argv[1] == "honest":
        return honest(sys.argv[2], sys.argv[3], sys.argv[4])
    if len(sys.argv) != 4 or sys.argv[1] not in ("n0", "vqp"):
        print(__doc__)
        return 2
    return n0(sys.argv[2], sys.argv[3]) if sys.argv[1] == "n0" else vqp(sys.argv[2], sys.argv[3])


if __name__ == "__main__":
    sys.exit(main())
