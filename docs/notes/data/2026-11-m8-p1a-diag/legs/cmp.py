#!/usr/bin/env python3
"""cmp.py ROOT ARM [ARM...]: each arm vs ROOT/base on TRAIN rows: status movers, QM/LA geomean on both-optimal, row moves."""
import os, sys, math, glob
root = sys.argv[1]
RANK = {"optimal": 2, "acceptable": 1}
def load(d):
    rows = {}
    for f in glob.glob(f"{d}/*.out"):
        r = {}
        for l in open(f):
            if "=" in l:
                k, v = l.rstrip("\n").split("=", 1); r[k] = v
        res = open(f[:-4] + ".result").read() if os.path.exists(f[:-4] + ".result") else ""
        r["_capped"] = "CAPPED" in res
        rows[os.path.basename(f)[:-4].replace("hven-sqp-", "")] = r
    return rows
base = load(f"{root}/base")
SKIP = {"options_label", "hven_pin_sha", "wall_s", "options_digest"}
def gi(r, k):
    try: return float(r.get(k, "nan"))
    except ValueError: return float("nan")
for arm in sys.argv[2:]:
    rows = load(f"{root}/{arm}")
    better, worse, moved, lq, la = [], [], 0, [], []
    tqb = tqa = 0
    for k, b in sorted(base.items()):
        a = rows.get(k)
        if a is None: worse.append(k + "(missing)"); continue
        diff = any(a.get(x) != b.get(x) for x in set(a) | set(b) if x not in SKIP and not x.startswith("wall"))
        moved += diff
        sb, sa = b.get("status"), a.get("status")
        if a["_capped"]: sa = "CAPPED"
        rb, ra = RANK.get(sb, 0), RANK.get(sa, 0)
        if ra > rb: better.append(f"{k}:{sb}->{sa}")
        elif ra < rb: worse.append(f"{k}:{sb}->{sa}")
        if sb == "optimal" and sa == "optimal":
            qb, qa = gi(b, "side.qp_minor_iters"), gi(a, "side.qp_minor_iters")
            fb, fa = gi(b, "factorizations"), gi(a, "factorizations")
            lq.append(math.log((qa + 1) / (qb + 1))); la.append(math.log((fa + 1) / (fb + 1)))
            tqb += qb; tqa += qa
    gq = math.exp(sum(lq) / len(lq)) if lq else float("nan")
    ga = math.exp(sum(la) / len(la)) if la else float("nan")
    wq = sum(1 for x in lq if x < math.log(0.9)); lsq = sum(1 for x in lq if x > -math.log(0.9))
    print(f"{arm:14s} moved={moved:3d} better={len(better)} worse={len(worse)} QMgeo={gq:.4f} LAgeo={ga:.4f} QMsum {tqb:.0f}->{tqa:.0f} QM10%w/l={wq}/{lsq}")
    for x in better: print(f"    + {x}")
    for x in worse: print(f"    - {x}")
