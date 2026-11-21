#!/usr/bin/env python3
"""PF-2e P2 read-out: TRAIN rows of arm NEW vs arm REF (train_check.row_equal). A row is a STATUS mover when `status`
differs; a PATH mover when any other key differs outside the declared counter-only set (COUNTER_ONLY, the probe's
expected movers); each path mover gets an objective-quality read. Also the sqp instruction ratio NEW/REF over all rows
and over the rows whose path did not move. usage: path_quality.py NEW_DIR REF_DIR DN3_DIR"""
import glob, math, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_check as tc

COUNTER_ONLY = ("side.harness_qpsum_dep_test_solves", "side.harness_qpsum_dep_stall_admits", "side.harness_qpsum_dep_grey_rereads",
                "side.backend_solve")


def instr(p):
    for line in open(p + ".stat"):
        if "instructions:u" in line:
            return float(line.split(",")[0])
    return float("nan")


def f(r, k):
    try:
        return float(r.get(k, "nan"))
    except ValueError:
        return float("nan")


def quality(new, old):
    """better / worse / same local solution, from status, objective and primal feasibility."""
    sn, so = new.get("status"), old.get("status")
    fn, fo = f(new, "objective"), f(old, "objective")
    pn = max(f(new, "kkt_primal_eq"), f(new, "kkt_primal_ineq"))
    po = max(f(old, "kkt_primal_eq"), f(old, "kkt_primal_ineq"))
    same_f = math.isfinite(fn) and math.isfinite(fo) and abs(fn - fo) <= 1e-6 * max(1.0, abs(fo))
    if sn == so == "optimal":
        if same_f:
            return "SAME local solution (both optimal, objective equal to 1e-6 rel)"
        return ("BETTER" if fn < fo else "WORSE") + f" local solution (both optimal, objective {fo:.10g} -> {fn:.10g})"
    if same_f:
        return f"SAME objective to 1e-6 rel, status {so} both, primal {po:.2g} -> {pn:.2g}"
    feas = "feasibility better" if pn < po else "feasibility worse" if pn > po else "feasibility equal"
    return (f"NO SOLUTION either side (status {so}); objective {fo:.6g} -> {fn:.6g}, primal infeas {po:.2g} -> {pn:.2g} "
            f"({feas}; {'lower' if fn < fo else 'higher'} objective)")


ndir, ref, d3 = sys.argv[1:4]
roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
status_m, path_m, counter_m, missing = [], [], [], []
tot = {"all": [0.0, 0.0], "stable": [0.0, 0.0]}
for pop, lane, prob in roster:
    tag = f"{pop}-{lane}-{prob}"
    for arm in ("D", "K"):
        pn, po = os.path.join(ndir, "cells", f"{tag}.{arm}"), os.path.join(ref, "cells", f"{tag}.{arm}")
        eq, new, old, diffs, _ = tc.row_equal(pn, po)
        if eq is None:
            missing.append(f"{tag}.{arm}")
            continue
        keys = [d.split(":")[0] for d in diffs]
        path_keys = [k for k in keys if not k.startswith(COUNTER_ONLY)]
        moved = bool(path_keys)
        if lane == "hven-sqp":
            tot["all"][0] += instr(po); tot["all"][1] += instr(pn)
            if not moved:
                tot["stable"][0] += instr(po); tot["stable"][1] += instr(pn)
        if eq:
            continue
        if "status" in keys:
            status_m.append(f"{tag}.{arm}")
        if moved:
            path_m.append(f"{tag}.{arm}")
            print(f"PATH_MOVER {tag}.{arm}: {quality(new, old)}")
            print(f"    iterations {old.get('iterations')} -> {new.get('iterations')}, qp_minor_iters "
                  f"{old.get('side.qp_minor_iters')} -> {new.get('side.qp_minor_iters')}, keys {','.join(path_keys[:10])}")
        else:
            counter_m.append(f"{tag}.{arm}")
print(f"SUMMARY rows {2 * len(roster)}; status movers {len(status_m)}; path movers {len(path_m)}; counter-only movers "
      f"{len(counter_m)}; missing/capped {len(missing)}")
for r in status_m:
    print(f"  STATUS_MOVER {r}")
for k, (a, b) in tot.items():
    print(f"INSTR hven-sqp {k:6s} REF {a:.6e} NEW {b:.6e} NEW/REF {b / a:.6f}")
