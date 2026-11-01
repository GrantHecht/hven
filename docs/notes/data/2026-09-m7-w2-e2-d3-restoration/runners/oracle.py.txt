#!/usr/bin/env python3
"""E2-D3 independent oracle (scipy/HiGHS in a scratchpad venv; never vendored).
For a dumped point x (with box l,u, ce, ci, Je, Ji):
  * box violation, h = ||ce||_1 + ||max(ci,0)||_1
  * first-order stationarity of the violation measure: the LP
        phi(eps) = min ||ce + Je d||_1 + ||max(ci + Ji d,0)||_1 + ||boxviol(x+d)||_1   s.t. |d|_inf <= eps
    compared with phi(0); a drop proportional to eps means x is NOT a stationary point of the (l1,
    box-inclusive) violation measure
  * the smallest l_inf radius R at which the linearization is feasible WITH the box (bisection),
    i.e. whether the trust-region QP at radius Delta is infeasible (R > Delta)
  * the distance to a feasible point from an independent solve (hven-ipm's final x), if given."""
import sys, numpy as np, scipy.sparse as sp
from scipy.optimize import linprog
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from load import load

def lp_phi(d, eps, box=True):
    x, l, u = d['x'], d['lower'], d['upper']
    n, me, mi = d['n'], d['me'], d['mi']
    Je = d['Je'] if me else sp.csr_matrix((0, n)); Ji = d['Ji'] if mi else sp.csr_matrix((0, n))
    ce = d['ce'] if me else np.zeros(0); ci = d['ci'] if mi else np.zeros(0)
    # variables: dd (n), pe, qe (me), ri (mi), bl, bu (n)   minimize sum pe+qe+ri+bl+bu
    # ce + Je dd = pe - qe ; ci + Ji dd <= ri ; x+dd >= l - bl ; x+dd <= u + bu
    nb = n if box else 0
    nv = n + 2 * me + mi + 2 * nb
    c = np.concatenate([np.zeros(n), np.ones(2 * me + mi + 2 * nb)])
    A_eq = sp.hstack([Je, -sp.eye(me), sp.eye(me), sp.csr_matrix((me, mi + 2 * nb))]) if me else None
    b_eq = -ce if me else None
    rows = []; rhs = []
    if mi:
        rows.append(sp.hstack([Ji, sp.csr_matrix((mi, 2 * me)), -sp.eye(mi), sp.csr_matrix((mi, 2 * nb))])); rhs.append(-ci)
    if box:
        fl = np.isfinite(l); fu = np.isfinite(u)
        Il = sp.eye(n).tocsr()[fl]; Iu = sp.eye(n).tocsr()[fu]
        # -dd - bl <= x - l
        rows.append(sp.hstack([-Il, sp.csr_matrix((Il.shape[0], 2 * me + mi)), -Il, sp.csr_matrix((Il.shape[0], n))])); rhs.append((x - l)[fl])
        # dd - bu <= u - x
        rows.append(sp.hstack([Iu, sp.csr_matrix((Iu.shape[0], 2 * me + mi + n)), -Iu])); rhs.append((u - x)[fu])
    A_ub = sp.vstack(rows).tocsr() if rows else None; b_ub = np.concatenate(rhs) if rows else None
    bounds = [(-eps, eps)] * n + [(0, None)] * (nv - n)
    r = linprog(c, A_ub=A_ub, b_ub=b_ub, A_eq=A_eq, b_eq=b_eq, bounds=bounds, method='highs')
    return r.fun if r.status == 0 else np.nan, r

def feasible_radius(d, hi=1e6):
    """smallest R with phi_hard(R) == 0 where the box is HARD (bl=bu=0)."""
    def feas(R):
        x, l, u = d['x'], d['lower'], d['upper']
        lo = np.maximum(-R, l - x); up = np.minimum(R, u - x)
        if np.any(lo > up + 1e-12): return False
        n, me, mi = d['n'], d['me'], d['mi']
        c = np.concatenate([np.zeros(n), np.ones(2 * me + mi)])
        A_eq = sp.hstack([d['Je'], -sp.eye(me), sp.eye(me), sp.csr_matrix((me, mi))]) if me else None
        A_ub = sp.hstack([d['Ji'], sp.csr_matrix((mi, 2 * me)), -sp.eye(mi)]) if mi else None
        r = linprog(c, A_ub=A_ub, b_ub=(-d['ci'] if mi else None), A_eq=A_eq, b_eq=(-d['ce'] if me else None),
                    bounds=list(zip(lo, up)) + [(0, None)] * (2 * me + mi), method='highs')
        return r.status == 0 and r.fun <= 1e-7 * max(1.0, np.abs(d['ce']).sum() if me else 1.0)
    if not feas(hi): return np.inf
    a, b = 0.0, hi
    if feas(0.0): return 0.0
    for _ in range(60):
        m = 0.5 * (a + b)
        if feas(m): b = m
        else: a = m
        if b - a <= 1e-6 * max(1.0, b): break
    return b

def readx(path):
    out = {}; cur = None
    for line in open(path):
        t = line.split()
        if len(t) == 2 and not t[0][0].isdigit() and t[0][0] not in '-+.':
            cur = t[0]; out[cur] = []
        else:
            out[cur].append(float(t[0]))
    return {k: np.array(v) for k, v in out.items()}

if __name__ == '__main__':
    path = sys.argv[1]; ipm = sys.argv[2] if len(sys.argv) > 2 else None
    d = load(path)
    x, l, u = d['x'], d['lower'], d['upper']
    bv = np.maximum(l - x, 0) + np.maximum(x - u, 0)
    h = np.abs(d['ce']).sum() + (np.maximum(d['ci'], 0).sum() if d['mi'] else 0.0)
    print(f"POINT {path}\n  n={d['n']} me={d['me']} mi={d['mi']} h_l1={h:.10g} box_viol: count={int((bv>0).sum())} max={bv.max():.6g} sum={bv.sum():.6g}")
    phi0, _ = lp_phi(d, 0.0)
    print(f"  phi(0) [h + box viol, linearized] = {phi0:.10g}")
    for eps in (1e-6, 1e-4, 1e-2, 1.0):
        pb, _ = lp_phi(d, eps, box=True); pn, _ = lp_phi(d, eps, box=False)
        print(f"  eps={eps:g}: phi_box={pb:.10g} (drop {phi0-pb:.4g}, rate {(phi0-pb)/eps:.4g})   phi_nobox={pn:.10g} (drop {h-pn:.4g}, rate {(h-pn)/eps:.4g})")
    R = feasible_radius(d)
    print(f"  smallest l_inf radius with a feasible linearization (box hard): {R:.6g}")
    if ipm:
        xi = readx(ipm)['x']
        xc = np.minimum(np.maximum(x, l), u)
        print(f"  independent feasible point (hven-ipm final x): |x_ipm - x|_inf={np.abs(xi-x).max():.6g}  |x_ipm - clamp(x)|_inf={np.abs(xi-xc).max():.6g}  x_ipm box viol max={(np.maximum(l-xi,0)+np.maximum(xi-u,0)).max():.3g}")
