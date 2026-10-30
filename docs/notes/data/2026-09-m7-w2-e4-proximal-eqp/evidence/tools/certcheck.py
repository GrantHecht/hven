#!/usr/bin/env python3
"""E4-18: check a QpFarkasCertificate offline, in numpy, on the dump's FULL data.

The engine's claim (src/qp/qp_engine.cpp verify_farkas): with c = sign*(blocker row or
unit vector), y_e free, y_i <= 0 on the working rows, and box_part = c - Ae'y_e - Ai'y_i,
every feasible x of the TR-restricted QP satisfies
    c'x = y_e'(Ae x) + y_i'(Ai x) + box_part'x
        >= y_e'be + y_i'bi + sum_i box_part_i * (lower_i if box_part_i>0 else upper_i) = implied
and the blocker demands c'x <= limit. implied - limit > margin is a contradiction.
Here we RE-DERIVE box_part, implied and the two inequality directions from the dump, and
independently minimise c'x over the feasible set with HiGHS (an LP) to show no feasible x
can reach implied -- i.e. the bound is not merely asserted, it is attained-or-unattainable.
usage: certcheck.py DUMP CERT"""
import sys, numpy as np, scipy.sparse as sp
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from d4load import load
from scipy.optimize import linprog

def readcert(path):
    d = {}
    with open(path) as f:
        while True:
            l = f.readline()
            if not l: break
            t = l.split()
            if len(t) == 2 and t[0] in ("y_e", "y_i", "box_part", "x"):
                k = int(t[1]); d[t[0]] = np.array([float(f.readline()) for _ in range(k)])
            elif len(t) == 2:
                d[t[0]] = float(t[1])
    return d

dump, certf = sys.argv[1], sys.argv[2]
d = load(dump); ct = readcert(certf)
n, me, mi = d["n"], d["me"], d["mi"]
Ae = d["mat"]["Ae"] if me else sp.csr_matrix((0, n))
be = d["vec"]["be"] if me else np.zeros(0)
Ai = d["mat"]["Ai"] if mi else sp.csr_matrix((0, n))
bi = d["vec"]["bi"] if mi else np.zeros(0)
lo, up = d["lo"], d["up"]          # TR-tightened effective bounds, as the engine's copy has them
print(f"CERTCHECK {dump}\n  cert {certf}")
print(f"  present={ct['present']:.0f} status={ct['status']:.0f} blocker={ct['blocker']:.0f} "
      f"is_ineq={ct['blocker_is_ineq']:.0f} sign={ct['sign']:+.0f}")
if ct["present"] < 0.5:
    print("  NO CERTIFICATE -- nothing to check"); sys.exit(0)
sign = ct["sign"]; j = int(ct["blocker"])
c = (Ai[j].toarray().ravel() if ct["blocker_is_ineq"] > 0.5 else np.eye(1, n, j).ravel()) * sign
y_e, y_i, bp_eng = ct["y_e"], ct["y_i"], ct["box_part"]
bp = c - (Ae.T @ y_e if me else 0) - (Ai.T @ y_i if mi else 0)
print(f"  box_part re-derived: max|mine - engine's| = {np.abs(bp - bp_eng).max():.3e}")
print(f"  y_i sign discipline (must be <= 0): max y_i = {y_i.max() if mi else 0.0:.3e}")
# the implied bound, re-derived; an open side (no finite bound) is the residual
implied = float(y_e @ be) + (float(y_i @ bi) if mi else 0.0)
open_sq = 0.0; nopen = 0
for i in range(n):
    b = bp[i]
    if b == 0.0: continue
    bnd = lo[i] if b > 0 else up[i]
    if not np.isfinite(bnd):
        open_sq += b * b; nopen += 1
    else:
        implied += b * bnd
print(f"  implied re-derived = {implied:.17g}   engine = {ct['implied']:.17g}  diff = {implied-ct['implied']:.3e}")
print(f"  dropped (open-sided) entries: {nopen}, ||dropped||2 = {np.sqrt(open_sq):.3e}  "
      f"engine residual = {ct['residual']:.3e}  exact_tol = {ct['exact_tol']:.3e}")
print(f"  limit = {ct['limit']:.17g}  margin = {ct['margin']:.3e}  implied - limit = {implied-ct['limit']:.6g}")
# INDEPENDENT: minimise c'x over the feasible set (HiGHS). Infeasible LP => the QP is
# infeasible outright; a finite minimum >= implied confirms the bound; a minimum < limit
# would EXHIBIT a point the certificate forbids => FALSE certificate.
bounds = [(None if not np.isfinite(lo[i]) else lo[i], None if not np.isfinite(up[i]) else up[i]) for i in range(n)]
r = linprog(c, A_ub=(Ai.tocsr() if mi else None), b_ub=(bi if mi else None),
            A_eq=(Ae.tocsr() if me else None), b_eq=(be if me else None), bounds=bounds, method="highs")
print(f"  LP  min c'x over the feasible set: status={r.status} ({r.message.strip()})")
if r.status == 0:
    print(f"      min c'x = {r.fun:.17g}  vs implied {implied:.17g}  vs limit {ct['limit']:.17g}")
    if r.fun < ct["limit"] - 0.0:
        print("      *** FALSE CERTIFICATE: a feasible x with c'x <= limit EXISTS ***")
    else:
        print("      certificate consistent: no feasible x meets the blocker's limit")
elif r.status == 2:
    print("      the feasible set is EMPTY (HiGHS): the QP is infeasible, so the certificate")
    print("      cannot be contradicted by any point, and the kInfeasible verdict is correct")
