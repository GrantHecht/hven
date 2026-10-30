#!/usr/bin/env python3
"""Is the dump's QP feasible WITHOUT the trust region? (E4-18's framing question.)"""
import sys, numpy as np, scipy.sparse as sp
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from d4load import load
from scipy.optimize import linprog
for path in sys.argv[1:]:
    d = load(path); n, me, mi = d["n"], d["me"], d["mi"]
    Ae = d["mat"]["Ae"] if me else sp.csr_matrix((0, n)); be = d["vec"]["be"] if me else np.zeros(0)
    Ai = d["mat"]["Ai"] if mi else sp.csr_matrix((0, n)); bi = d["vec"]["bi"] if mi else np.zeros(0)
    for tag, lo, up in (("TR-restricted", d["lo"], d["up"]), ("raw bounds", d["lo_real"], d["up_real"])):
        c = np.concatenate([np.zeros(n), np.ones(2 * me + mi)])
        A_eq = sp.hstack([Ae, sp.eye(me), -sp.eye(me), sp.csr_matrix((me, mi))]).tocsr() if me else None
        A_ub = sp.hstack([Ai, sp.csr_matrix((mi, 2 * me)), -sp.eye(mi)]).tocsr() if mi else None
        b = [(None if not np.isfinite(lo[i]) else lo[i], None if not np.isfinite(up[i]) else up[i]) for i in range(n)]
        b += [(0, None)] * (2 * me + mi)
        r = linprog(c, A_ub=A_ub, b_ub=(bi if mi else None), A_eq=A_eq, b_eq=(be if me else None), bounds=b, method="highs")
        print(f"{path.split('/')[-2]}/{path.split('/')[-1][:-4]:4s} {tag:14s} tr={d['tr']:.6g} phase1 status={r.status} min_l1_violation={r.fun!r}")
