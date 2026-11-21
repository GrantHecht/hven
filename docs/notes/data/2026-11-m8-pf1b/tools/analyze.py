#!/usr/bin/env python3
"""PF-1b: summarize the per-minor CSVs of the replays (pf1b_replay --csv). usage: analyze.py <csv>..."""
import csv
import math
import sys


def fnum(s):
    try:
        v = float(s)
        return None if math.isnan(v) else v
    except ValueError:
        return None


def stats(vals):
    v = sorted(x for x in vals if x is not None)
    if not v:
        return "none"
    return f"n={len(v)} median={v[len(v) // 2]:.2e} p99={v[int(len(v) * 0.99)]:.2e} max={v[-1]:.2e}"


for path in sys.argv[1:]:
    R = list(csv.DictReader(open(path)))
    print(f"== {path}: {len(R)} minors")
    rung = next((i for i in range(1, len(R)) if float(R[i]['mu']) < float(R[i - 1]['mu'])), None)
    print(f"  first dual_mu drop at record {rung}" + (f" ({R[rung - 1]['mu']} -> {R[rung]['mu']})" if rung else ""))
    pert = [r for r in R if int(r['perturbed']) > 0]
    print(f"  verdicts: kOk {sum(r['verdict'] == '0' for r in R)} kSuspect {sum(r['verdict'] == '1' for r in R)} "
          f"kWrong {sum(r['verdict'] == '2' for r in R)}; perturbed passes {len(pert)}, pivots "
          f"{sum(int(r['perturbed']) for r in pert)}, max in a pass {max([int(r['perturbed']) for r in pert], default=0)}")
    print(f"  delta range {min(float(r['delta']) for r in R):.1e}..{max(float(r['delta']) for r in R):.1e}")
    print("  EQP row residual, perturbed passes:", stats(fnum(r['eqp_row_res']) for r in pert))
    print("  EQP row residual, clean passes:    ", stats(fnum(r['eqp_row_res']) for r in R if int(r['perturbed']) == 0))
    print(f"  EQP refusals {sum(int(r['eqp_refused']) for r in R)}")
    t = R[-1]
    print(f"  last pass: action {t['action']} verdict {t['verdict']} perturbed {t['perturbed']} mu {t['mu']}")
    step = max(1, len(R) // 12)
    print("  window: first-minor obj_in ws_in | actions | perturbed passes")
    for a in range(0, len(R), step):
        rs = R[a:a + step]
        acts = {}
        for r in rs:
            acts[r['action']] = acts.get(r['action'], 0) + 1
        astr = " ".join(f"a{k}={v}" for k, v in sorted(acts.items(), key=lambda kv: int(kv[0])))
        print(f"    {a:6d} {float(rs[0]['obj_in']):+.10e} {rs[0]['ws_in']:>5} | {astr} | "
              f"{sum(1 for r in rs if int(r['perturbed']) > 0)}")
