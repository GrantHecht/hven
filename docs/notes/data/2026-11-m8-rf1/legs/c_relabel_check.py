#!/usr/bin/env python3
"""RF-1 gate C, criterion C2 (../DECLARATION-RF-1-C.md): the TRAIN identity-except-label check.

usage: c_relabel_check.py NEW_DIR DN3_DIR REF_DIR

Per roster cell and arm (D, K), the NEW row is compared with the REF row by train_check.py's row equality (every key of
the .out except options_label and hven_pin_sha, the lane rc, residual-class keys inside the calibrated gate, the
.optdump), with exactly three declared exceptions:
  - `status` and `outcome` (the harness's status class) may differ: that is the label;
  - an hven-sqp row's NEW .optdump may carry the four `termination.acceptable_*_tol` lines SqpOptions gains (they are
    dropped from the new dump before the byte comparison; the IPM dump already had them and is compared whole).
Every other difference is a PATH MOVER (criterion: 0). Every status mover is printed with its old and new status, the
four residuals the lane reports (caller units) and `side.qp_maxiter_exits` / `side.qp_numerical_exits`, and classified:
  RELABEL_ACCEPTABLE   numerical_error/stalled -> acceptable, the four residuals inside the default tier
                       (stationarity 1e-2; equality, inequality, complementarity 1e-3)
  RELABEL_ACCEPTABLE?  as above but a lane residual outside the tier (the engine reads stationarity in the engine's
                       own measure; a scaled solve can differ) -- reported, explained per row
  RELABEL_STALLED      numerical_error -> stalled with side.qp_maxiter_exits > 0
  UNEXPECTED           anything else (criterion: 0)
Exit 0 iff every row is present, path movers 0 and UNEXPECTED 0.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import train_check as tc  # noqa: E402

LABEL_KEYS = {"status", "outcome"}
TIER = {"kkt_stationarity": 1e-2, "kkt_primal_eq": 1e-3, "kkt_primal_ineq": 1e-3, "kkt_complementarity": 1e-3}
NEW_SQP_DUMP = ("termination.acceptable_stationarity_tol", "termination.acceptable_equality_tol",
                "termination.acceptable_inequality_tol", "termination.acceptable_complementarity_tol")


def dump_equal(pn, po, sqp):
    if not (os.path.exists(pn + ".optdump") and os.path.exists(po + ".optdump")):
        return False
    new = open(pn + ".optdump").read().splitlines()
    old = open(po + ".optdump").read().splitlines()
    if sqp:
        new = [l for l in new if not l.split(" = ", 1)[0] in NEW_SQP_DUMP]
    return new == old


def fnum(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return float("nan")


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    ndir, d3, ref = sys.argv[1:4]
    roster = [l.rstrip("\n").split("\t") for l in open(os.path.join(d3, "roster.tsv")) if l.strip()]
    rows = missing = path_movers = 0
    classes = {}
    for pop, lane, prob in roster:
        tag = f"{pop}-{lane}-{prob}"
        for arm in ("D", "K"):
            rows += 1
            pn = os.path.join(ndir, "cells", f"{tag}.{arm}")
            po = os.path.join(ref, "cells", f"{tag}.{arm}")
            new, okn = tc.load(pn)
            old, oko = tc.load(po)
            if not (okn and oko):
                missing += 1
                print(f"MISSING {tag}.{arm} (new {okn}, ref {oko})")
                continue
            ex, _ig, og = tc.compare({k: v for k, v in new.items() if k not in LABEL_KEYS},
                                     {k: v for k, v in old.items() if k not in LABEL_KEYS})
            if not dump_equal(pn, po, lane == "hven-sqp"):
                ex.append("OPTDUMP_DIFFERS")
            if ex or og:
                path_movers += 1
                print(f"PATH_MOVER {tag}.{arm}")
                for d in (ex + og)[:16]:
                    print(f"    {d}")
            if new.get("status") == old.get("status"):
                continue
            o, n = old.get("status"), new.get("status")
            res = {k: fnum(new.get(k)) for k in TIER}
            inside = all(res[k] <= TIER[k] for k in TIER)
            capx = int(new.get("side.qp_maxiter_exits", "0") or 0)
            if lane == "hven-sqp" and o in ("numerical_error", "stalled") and n == "acceptable":
                cls = "RELABEL_ACCEPTABLE" if inside else "RELABEL_ACCEPTABLE?"
            elif lane == "hven-sqp" and o == "numerical_error" and n == "stalled" and capx > 0:
                cls = "RELABEL_STALLED"
            else:
                cls = "UNEXPECTED"
            classes[cls] = classes.get(cls, 0) + 1
            print(f"STATUS_MOVER {cls} {tag}.{arm} {o} -> {n} "
                  + " ".join(f"{k}={new.get(k)}" for k in TIER)
                  + f" qp_maxiter_exits={capx} qp_numerical_exits={new.get('side.qp_numerical_exits')}"
                  + f" objective={new.get('objective')} iterations={new.get('iterations')}")
    print(f"C2_SUMMARY rows {rows}; missing/capped {missing}; path movers {path_movers}; status movers "
          f"{sum(classes.values())} " + " ".join(f"{k}={v}" for k, v in sorted(classes.items())))
    ok = missing == 0 and path_movers == 0 and classes.get("UNEXPECTED", 0) == 0
    print("C2_AS_DECLARED" if ok else "C2_CRITERION_NOT_MET")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
