#!/usr/bin/env python3
"""M8 DN-0 IPM r1 (DN-0's summary.py, IPM lane only, plus sample-count, sampling-defect and SC counter-identity lines): population-wide summaries of results/cells.csv (informational; the rule is rule.py).

Per engine x population: status counts, per-cell median of each bucket share, the
instruction-weighted aggregate share (sum of bucket instructions / sum of totals, from the
per-run sampled solver instructions), rho quantiles and the share of cells with rho <= 0.5,
pooled negative-eigenvalue agreement over all replayed matrices, and the same for the
steady-state reading.

Usage: summary.py CELLS_CSV [REPLAY_DIR]
"""
import collections
import csv
import glob
import os
import statistics
import sys

B = ["B_sym", "B_fac", "B_kkt", "B_eval", "B_setup", "B_eng"]


def f(x):
    try:
        return float(x)
    except (TypeError, ValueError):
        return None


def q(v, p):
    v = sorted(v)
    if not v:
        return None
    k = (len(v) - 1) * p
    lo, hi = int(k), min(int(k) + 1, len(v) - 1)
    return v[lo] + (v[hi] - v[lo]) * (k - lo)


def main():
    rows = list(csv.DictReader(open(sys.argv[1])))
    rdir = sys.argv[2] if len(sys.argv) > 2 else None
    for lane in ("hven-ipm",):
        for pop in ("hs", "sd"):
            cs = [r for r in rows if r["engine"] == lane and r["population"] == pop]
            print(f"== {lane} / {pop}: {len(cs)} cells; status {dict(collections.Counter(r['status'] for r in cs))}")
            for prefix, tot_key, label in (("share_", "rec_solver_instr", "primary"),
                                           ("ss_share_", "ss_instr", "steady-state")):
                med = {b: statistics.median([f(r[prefix + b]) for r in cs if f(r[prefix + b]) is not None]) for b in B}
                W = sum(f(r[tot_key]) or 0 for r in cs)
                agg = {b: sum((f(r[prefix + b]) or 0) * (f(r[tot_key]) or 0) for r in cs) / W for b in B}
                print(f"  {label:12s} per-cell median: " + " ".join(f"{b}={med[b]:.3f}" for b in B))
                print(f"  {label:12s} weighted agg.  : " + " ".join(f"{b}={agg[b]:.3f}" for b in B))
                fac = [(f(r[prefix + 'B_sym']) or 0) + (f(r[prefix + 'B_fac']) or 0) for r in cs]
                print(f"  {label:12s} (B_sym+B_fac) >= 0.40 on {sum(1 for x in fac if x >= 0.40)}/{len(cs)} cells; "
                      f"B_kkt >= 0.25 on {sum(1 for r in cs if (f(r[prefix + 'B_kkt']) or 0) >= 0.25)}/{len(cs)}")
            ns = [f(r.get("rec_samples_solver")) for r in cs if f(r.get("rec_samples_solver")) is not None]
            if ns:
                print(f"  solver samples per cell (all reps): min={min(ns):.0f} q25={q(ns,.25):.0f} med={q(ns,.5):.0f} max={max(ns):.0f}; "
                      f"cells with sampling defects: {sum(1 for r in cs if (f(r.get('rec_sampling_defects')) or 0) > 0)}")
            ro = [f(r.get("rec_over_meas")) for r in cs if f(r.get("rec_over_meas")) is not None]
            sp = [f(r.get("instr_spread_S")) for r in cs if f(r.get("instr_spread_S")) is not None]
            if ro:
                print(f"  SC (record binary) / S median instructions: min={min(ro):.6f} med={q(ro,.5):.6f} max={max(ro):.6f}; "
                      f"S-round spread (max-min)/median: med={q(sp,.5):.2e} max={max(sp):.2e}")
            rho = [f(r["rho"]) for r in cs if f(r["rho"]) is not None]
            rw = [f(r["rho_engine_weighted"]) for r in cs if f(r["rho_engine_weighted"]) is not None]
            if rho:
                print(f"  rho: n={len(rho)} min={min(rho):.3f} q25={q(rho,.25):.3f} med={q(rho,.5):.3f} "
                      f"q75={q(rho,.75):.3f} max={max(rho):.3f}; rho<=0.5 on {sum(1 for x in rho if x <= 0.5)}")
                print(f"  rho_engine_weighted: med={q(rw,.5):.3f} q25={q(rw,.25):.3f} q75={q(rw,.75):.3f}; "
                      f"<=0.5 on {sum(1 for x in rw if x <= 0.5)}, <1 on {sum(1 for x in rw if x < 1)}")
            ag = [f(r["neg_agree"]) for r in cs if f(r["neg_agree"]) is not None]
            if ag:
                print(f"  neg_agree per cell: med={q(ag,.5):.3f} min={min(ag):.3f}; cells at >= 0.99: {sum(1 for x in ag if x >= .99)}/{len(ag)}")
            if rdir:
                n = a = ae = fid = ok_s = ok_d = 0
                for r in cs:
                    p = os.path.join(rdir, f"{pop}-{lane}-{r['problem']}.replay.csv")
                    if not os.path.exists(p):
                        continue
                    for m in csv.DictReader(open(p)):
                        n += 1
                        a += int(m["neg_agree"])
                        ae += int(int(m["dense_neg"]) >= 0 and int(m["dense_neg"]) == int(m["engine_neg"]))
                        fid += int(int(m["sparse_neg"]) >= 0 and int(m["sparse_neg"]) == int(m["engine_neg"]))
                        ok_s += int(m["sparse_status"] == "0")
                        ok_d += int(m["dense_status"] == "0")
                if n:
                    print(f"  pooled matrices: {n}; neg agree (replay sparse vs dense) {a/n:.4f}; "
                          f"engine vs dense {ae/n:.4f}; replay fidelity {fid/n:.4f}; sparse ok {ok_s}, dense ok {ok_d}")


if __name__ == "__main__":
    main()
