#!/usr/bin/env python3
"""RF-1 gate A+B2 leg T: TRAIN rows of arm AB2 against arm C (gate-C/train/C). usage: ab2_train.py NEW_DIR C_DIR ROSTER
Every status mover listed with its objective-quality read (same / better / worse local solution: |df| <= 1e-6 max(1,|f_C|)
is `same`; otherwise the lower objective is `better` only when the new row is at least as feasible to 1e-6 as C's, and
`worse` the other way round); a LOST solve is C optimal/acceptable -> anything else. Totals of factorizations and
iterations over the rows both arms completed, per lane."""
import os, sys

def load(prefix):
    p = prefix + ".out"
    if not os.path.exists(p):
        return None
    row = {}
    for line in open(p, errors="replace"):
        if "=" in line:
            k, v = line.rstrip("\n").split("=", 1)
            row.setdefault(k, v)
    return row

def num(r, k):
    try:
        return float(r.get(k, "nan"))
    except ValueError:
        return float("nan")

SOLVED = ("optimal", "acceptable")
ndir, cdir, roster = sys.argv[1:4]
cells = [l.rstrip("\n").split("\t") for l in open(roster) if l.strip()]
tot = {}
movers, lost, missing = [], [], []
for pop, lane, prob in cells:
    if lane != "hven-sqp":
        continue  # RF-1c: hven-ipm rows are not run
    tag = f"{pop}-{lane}-{prob}"
    for arm in ("D", "K"):
        n = load(os.path.join(ndir, "cells", f"{tag}.{arm}"))
        c = load(os.path.join(cdir, "cells", f"{tag}.{arm}"))
        if n is None or c is None:
            missing.append(f"{tag}.{arm}"); continue
        t = tot.setdefault(lane, {"rows": 0, "fC": 0, "fN": 0, "iC": 0, "iN": 0, "solvedC": 0, "solvedN": 0, "optC": 0, "optN": 0})
        t["rows"] += 1
        for k, kk in (("factorizations", "f"), ("iterations", "i")):
            t[kk + "C"] += int(num(c, k)) if num(c, k) == num(c, k) else 0
            t[kk + "N"] += int(num(n, k)) if num(n, k) == num(n, k) else 0
        t["solvedC"] += c.get("status") in SOLVED; t["solvedN"] += n.get("status") in SOLVED
        t["optC"] += c.get("status") == "optimal"; t["optN"] += n.get("status") == "optimal"
        if n.get("status") == c.get("status"):
            continue
        fc, fn = num(c, "objective"), num(n, "objective")
        hc = max(num(c, "kkt_primal_eq"), num(c, "kkt_primal_ineq"))
        hn = max(num(n, "kkt_primal_eq"), num(n, "kkt_primal_ineq"))
        if fc == fc and fn == fn and abs(fn - fc) <= 1e-6 * max(1.0, abs(fc)):
            q = "same"
        elif fc == fc and fn == fn and fn < fc and hn <= max(hc, 1e-6):
            q = "better"
        elif fc == fc and fn == fn and fn > fc and hc <= max(hn, 1e-6):
            q = "worse"
        else:
            q = "incomparable"
        line = (f"{tag}.{arm} {c.get('status')}->{n.get('status')} quality={q} f {fc:.10g}->{fn:.10g} "
                f"h {hc:.3g}->{hn:.3g} stat {num(c,'kkt_stationarity'):.3g}->{num(n,'kkt_stationarity'):.3g} "
                f"it {c.get('iterations')}->{n.get('iterations')} fact {c.get('factorizations')}->{n.get('factorizations')}")
        movers.append(line)
        if c.get("status") in SOLVED and n.get("status") not in SOLVED:
            lost.append(line)
for m in movers:
    print("STATUS_MOVER", m)
for m in lost:
    print("LOST_SOLVE", m)
for m in missing:
    print("MISSING", m)
for lane, t in sorted(tot.items()):
    print(f"TOTALS {lane}: rows {t['rows']} optimal {t['optC']}->{t['optN']} optimal+acceptable {t['solvedC']}->{t['solvedN']} "
          f"factorizations {t['fC']}->{t['fN']} ({t['fN']-t['fC']:+d}) iterations {t['iC']}->{t['iN']} ({t['iN']-t['iC']:+d})")
print(f"AB2_SUMMARY status movers {len(movers)}; lost solves {len(lost)}; missing {len(missing)}")
