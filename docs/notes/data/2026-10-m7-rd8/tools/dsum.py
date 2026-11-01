#!/usr/bin/env python3
"""Summarize a D4QP dump: dims, opts, H stats (eigs via numpy), bounds, Ae structure, seed."""
import sys
import numpy as np


def load(path):
    d = {}
    lines = open(path).read().split("\n")
    i = 0
    while i < len(lines):
        L = lines[i].split()
        i += 1
        if not L:
            continue
        k = L[0]
        if k in ("H", "Ae", "Ai") and len(L) == 4:
            r, c, nz = map(int, L[1:])
            M = np.zeros((r, c))
            for j in range(nz):
                a, b, v = lines[i + j].split()
                M[int(a), int(b)] = float(v)
            i += nz
            d[k] = M
        elif k in ("g", "be", "bi", "lower", "upper", "seed_x", "x_final", "seed_bound_state",
                   "seed_ineq_active", "final_bound_state", "final_active_ineq") and len(L) == 2:
            nz = int(L[1])
            d[k] = np.array([float(lines[i + j]) for j in range(nz)])
            i += nz
        else:
            d[k] = L[1:]
    return d


d = load(sys.argv[1])
for k in ("dims", "opts", "overrides", "warm", "result"):
    print(k, d.get(k))
H = d["H"]
Hs = np.triu(H) + np.triu(H, 1).T
print("H nnz", np.count_nonzero(H), "upper-stored", np.allclose(H, np.triu(H)))
ev = np.linalg.eigvalsh(Hs)
print("H eig min %.4g max %.4g  neg %d zero(<1e-12) %d pos %d" % (ev[0], ev[-1], (ev < -1e-12).sum(),
      (abs(ev) <= 1e-12).sum(), (ev > 1e-12).sum()))
Ae = d["Ae"]
print("Ae", Ae.shape, "nnz", np.count_nonzero(Ae))
lo, up = d["lower"], d["upper"]
inf = 1e19
print("bounds: fixed", (lo == up).sum(), "lower-only", ((lo > -inf) & (up >= inf)).sum(), "upper-only",
      ((lo <= -inf) & (up < inf)).sum(), "two-sided", ((lo > -inf) & (up < inf) & (lo != up)).sum(),
      "free", ((lo <= -inf) & (up >= inf)).sum())
tw = (lo > -inf) & (up < inf) & (lo != up)
print("two-sided ranges: lo min %.3g max %.3g, up min %.3g max %.3g" % (lo[tw].min(), lo[tw].max(), up[tw].min(), up[tw].max()))
g = d["g"]
print("g inf %.4g" % abs(g).max())
if "seed_x" in d:
    print("seed x inf %.4g, seed pins %d" % (abs(d["seed_x"]).max(), (d["seed_bound_state"] != 0).sum()))
# reduced Hessian on null(Ae) for the free face (no pins)
Q, R = np.linalg.qr(Ae.T, mode="complete")
r = np.linalg.matrix_rank(Ae)
Z = Q[:, r:]
RH = Z.T @ Hs @ Z
ev2 = np.linalg.eigvalsh(RH)
print("rank Ae", r, "reduced H dim", RH.shape[0], "neg", (ev2 < -1e-10).sum(), "min %.4g max %.4g" % (ev2[0], ev2[-1]))
