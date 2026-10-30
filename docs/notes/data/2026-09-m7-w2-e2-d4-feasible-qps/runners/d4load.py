"""E2-D4 loader for D4QP dumps (the format qp_engine.cpp's scratch instrumentation writes).

Format (text, one item per line):
  D4QP 1
  dims n me mi
  opts primal_delta dual_mu feas_tol opt_tol max_iter schur_cap schur_cond_max ws_algebra(0 refactorize,1 border) tr_radius independent threads
  overrides tr_radius primal_delta dual_mu      (tr +inf / negative = the opts value)
  warm W seed S hot H k0_reused K
  result status=.. minors=.. ...                (the in-cell solve's outcome and E2 counters)
  <vector>  name k   then k lines                (g be bi lower upper seed_x x_final)
  <matrix>  name r c nnz  then nnz lines "i j v" (H: upper triangle; Ae; Ai)
  <int vec> name k   then k lines                (seed_bound_state seed_ineq_active final_bound_state final_active_ineq)
BoundState codes: 0 free, 1 at lower, 2 at upper, 3 fixed. QpStatus codes: see qp_types (0 optimal ...).
The QP: min g'x + 1/2 x'Hx  s.t. Ae x = be, Ai x <= bi, lower <= x <= upper, and the l_inf trust region
|x - x0| <= tr about x0 = clamp(seed_x or 0, lower, upper)."""
import numpy as np, scipy.sparse as sp

INTS = ("seed_bound_state", "seed_ineq_active", "final_bound_state", "final_active_ineq")


def load(path):
    d = {"vec": {}, "mat": {}, "ivec": {}}
    with open(path) as f:
        while True:
            line = f.readline()
            if not line:
                break
            t = line.split()
            if not t:
                continue
            k = t[0]
            if k == "D4QP":
                continue
            if k == "dims":
                d["n"], d["me"], d["mi"] = map(int, t[1:4]); continue
            if k == "opts":
                v = t[1:]
                d["opts"] = dict(primal_delta=float(v[0]), dual_mu=float(v[1]), feas_tol=float(v[2]), opt_tol=float(v[3]),
                                 max_iter=int(v[4]), schur_cap=int(v[5]), schur_cond_max=float(v[6]), ws_algebra=int(v[7]),
                                 tr_radius=float(v[8]), independent=int(v[9]), threads=int(v[10]))
                continue
            if k == "overrides":
                d["overrides"] = dict(tr_radius=float(t[1]), primal_delta=float(t[2]), dual_mu=float(t[3])); continue
            if k == "warm":
                d["warm"], d["seed"] = int(t[1]), int(t[3]); continue
            if k == "result":
                d["result"] = dict(kv.split("=") for kv in t[1:]); continue
            if k in INTS:
                m = int(t[1]); d["ivec"][k] = np.array([int(f.readline()) for _ in range(m)], dtype=int); continue
            if len(t) == 2:
                m = int(t[1]); d["vec"][k] = np.array([float(f.readline()) for _ in range(m)]); continue
            r, c, nz = int(t[1]), int(t[2]), int(t[3])
            rows, cols, vals = [], [], []
            for _ in range(nz):
                a, b, v = f.readline().split(); rows.append(int(a)); cols.append(int(b)); vals.append(float(v))
            d["mat"][k] = sp.csr_matrix((vals, (rows, cols)), shape=(r, c))
    n = d["n"]
    Hu = d["mat"]["H"]
    d["Hs"] = (Hu + Hu.T - sp.diags(Hu.diagonal())).tocsr()
    lo, up = d["vec"]["lower"].copy(), d["vec"]["upper"].copy()
    lo[lo <= -1e19] = -np.inf; up[up >= 1e19] = np.inf
    d["lo_real"], d["up_real"] = lo, up
    x0 = d["vec"].get("seed_x", np.zeros(n)) if d["seed"] else np.zeros(n)
    x0 = np.minimum(np.maximum(x0, lo), up)
    tr = d["overrides"]["tr_radius"]
    if not np.isfinite(tr):
        tr = d["opts"]["tr_radius"]
    d["tr"], d["x0"] = tr, x0
    if np.isfinite(tr):
        d["lo"], d["up"] = np.maximum(lo, x0 - tr), np.minimum(up, x0 + tr)
    else:
        d["lo"], d["up"] = lo, up
    return d
