#!/usr/bin/env python3
"""E2-D4 independent oracle for a D4QP dump (scipy/HiGHS/Clarabel in a scratchpad venv; never vendored).

  1. feasibility of the dumped QP (the trust region included): HiGHS phase-1 LP, minimal l1 violation;
  2. H's spectrum (dense), and the reduced Hessian Z'HZ on the null space of the walk's FINAL working set
     (the D5 question) and of the equality rows alone;
  3. if H is PSD: the optimum from Clarabel (interior point, independent of hven), its active set, and the
     comparison with the walk's final point and working set;
  4. optionally (--x FILE, a qp_replay --out file) the same comparison for another hven point.
usage: qp_oracle.py DUMP [--x FILE] [--json OUT]"""
import sys, json, numpy as np, scipy.sparse as sp
from scipy.optimize import linprog
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from d4load import load

FIN = 1e19


def cons(d):
    n, me, mi = d["n"], d["me"], d["mi"]
    Ae = d["mat"]["Ae"] if me else sp.csr_matrix((0, n))
    Ai = d["mat"]["Ai"] if mi else sp.csr_matrix((0, n))
    return Ae, d["vec"]["be"] if me else np.zeros(0), Ai, d["vec"]["bi"] if mi else np.zeros(0)


def phase1(d):
    """min sum(pe+qe+ri) s.t. Ae x + pe - qe = be, Ai x - ri <= bi, lo <= x <= up (TR included)."""
    n, me, mi = d["n"], d["me"], d["mi"]
    Ae, be, Ai, bi = cons(d)
    c = np.concatenate([np.zeros(n), np.ones(2 * me + mi)])
    A_eq = sp.hstack([Ae, sp.eye(me), -sp.eye(me), sp.csr_matrix((me, mi))]).tocsr() if me else None
    A_ub = sp.hstack([Ai, sp.csr_matrix((mi, 2 * me)), -sp.eye(mi)]).tocsr() if mi else None
    lo, up = d["lo"], d["up"]
    bounds = [(None if not np.isfinite(lo[i]) else lo[i], None if not np.isfinite(up[i]) else up[i]) for i in range(n)]
    bounds += [(0, None)] * (2 * me + mi)
    r = linprog(c, A_ub=A_ub, b_ub=bi if mi else None, A_eq=A_eq, b_eq=be if me else None, bounds=bounds, method="highs")
    return r.status, (r.fun if r.status == 0 else None), (r.x[:n] if r.status == 0 else None)


def clarabel_qp(d):
    import clarabel
    n, me, mi = d["n"], d["me"], d["mi"]
    Ae, be, Ai, bi = cons(d)
    lo, up = d["lo"], d["up"]
    P = sp.triu(d["Hs"]).tocsc()
    q = d["vec"]["g"]
    fl = np.where(np.isfinite(lo))[0]; fu = np.where(np.isfinite(up))[0]
    blocks = []; rhs = []
    if me: blocks.append(Ae); rhs.append(be)
    if mi: blocks.append(Ai); rhs.append(bi)
    I = sp.eye(n, format="csr")
    blocks.append(I[fu]); rhs.append(up[fu])
    blocks.append(-I[fl]); rhs.append(-lo[fl])
    A = sp.vstack(blocks).tocsc(); b = np.concatenate(rhs)
    cones = ([clarabel.ZeroConeT(me)] if me else []) + [clarabel.NonnegativeConeT(A.shape[0] - me)]
    s = clarabel.DefaultSettings(); s.verbose = False; s.tol_gap_abs = 1e-10; s.tol_gap_rel = 1e-10; s.tol_feas = 1e-10
    s.max_iter = 400
    sol = clarabel.DefaultSolver(P, q, A, b, cones, s).solve()
    x = np.array(sol.x); y = np.array(sol.z)
    return str(sol.status), x, y, (fl, fu)


def f_of(d, x):
    return float(d["vec"]["g"] @ x + 0.5 * x @ (d["Hs"] @ x))


def active_at(d, x, tol=1e-7):
    n, me, mi = d["n"], d["me"], d["mi"]
    _, _, Ai, bi = cons(d)
    rows = []
    if mi:
        s = Ai @ x - bi
        sc = np.maximum(1.0, np.abs(bi))
        rows = list(np.where(np.abs(s) <= tol * sc)[0])
    lo, up = d["lo"], d["up"]
    pins = {}
    for i in range(n):
        if np.isfinite(lo[i]) and abs(x[i] - lo[i]) <= tol * max(1, abs(lo[i])):
            pins[i] = "L" if lo[i] < up[i] else "X"
        elif np.isfinite(up[i]) and abs(x[i] - up[i]) <= tol * max(1, abs(up[i])):
            pins[i] = "U"
    return rows, pins


def violations(d, x):
    Ae, be, Ai, bi = cons(d)
    eq = float(np.abs(Ae @ x - be).max()) if d["me"] else 0.0
    iq = float(max(0.0, (Ai @ x - bi).max())) if d["mi"] else 0.0
    bx = float(max(0.0, (d["lo"] - x).max(), (x - d["up"]).max()))
    return eq, iq, bx


def reduced_hessian(d, rows_w, pins_w):
    """Z'HZ on null([Ae; Ai_W; E_pins]) (dense SVD)."""
    n = d["n"]
    Ae, _, Ai, _ = cons(d)
    blocks = []
    if d["me"]: blocks.append(Ae.toarray())
    if len(rows_w): blocks.append(Ai[rows_w].toarray())
    if len(pins_w):
        E = np.zeros((len(pins_w), n)); E[np.arange(len(pins_w)), pins_w] = 1.0; blocks.append(E)
    if not blocks:
        Z = np.eye(n)
        return np.linalg.eigvalsh(d["Hs"].toarray()), 0, n, None
    A = np.vstack(blocks)
    An = A / np.maximum(np.linalg.norm(A, axis=1, keepdims=True), 1e-300)
    U, S, Vt = np.linalg.svd(An, full_matrices=True)
    rank = int((S > 1e-10 * S.max()).sum())
    Z = Vt[rank:].T
    if Z.shape[1] == 0:
        return np.array([]), rank, 0, S
    ev = np.linalg.eigvalsh(Z.T @ d["Hs"].toarray() @ Z)
    return ev, rank, Z.shape[1], S


def main():
    path = sys.argv[1]
    xfile = sys.argv[sys.argv.index("--x") + 1] if "--x" in sys.argv else None
    d = load(path)
    n, me, mi = d["n"], d["me"], d["mi"]
    out = {"dump": path, "n": n, "me": me, "mi": mi, "tr": d["tr"], "result": d["result"]}
    print(f"ORACLE {path} n={n} me={me} mi={mi} tr={d['tr']:.6g} in-cell: {' '.join(k+'='+v for k,v in d['result'].items())}")
    st, v1, xf = phase1(d)
    out["phase1_status"], out["phase1_min_l1_violation"] = st, v1
    print(f"  FEASIBILITY phase-1 LP status={st} min_l1_violation={v1}")
    H = d["Hs"].toarray()
    ev = np.linalg.eigvalsh(H)
    out["H_eig_min"], out["H_eig_max"] = float(ev[0]), float(ev[-1])
    print(f"  H eig min={ev[0]:.4e} max={ev[-1]:.4e} n_neg={(ev < -1e-9 * max(1, abs(ev[-1]))).sum()} n_zero={(np.abs(ev) <= 1e-9 * max(1, abs(ev[-1]))).sum()}")
    evE, rE, dimE, _ = reduced_hessian(d, [], [])
    if len(evE):
        print(f"  Z'HZ on null(Ae): dim={dimE} rank(Ae)={rE} eig min={evE[0]:.4e}")
    xw = d["vec"]["x_final"]
    fb = d["ivec"]["final_bound_state"]; fa = list(d["ivec"]["final_active_ineq"])
    pins_w = [i for i in range(n) if fb[i] != 0]
    evW, rW, dimW, SW = reduced_hessian(d, fa, pins_w)
    out["final_ws"] = {"rows": len(fa), "pins": len(pins_w), "rank": rW, "count": me + len(fa) + len(pins_w)}
    print(f"  WALK final: f={f_of(d, xw):.12g} viol eq/in/box={violations(d, xw)} |W| rows={len(fa)} pins={len(pins_w)} me+|W|={me+len(fa)+len(pins_w)} rank={rW} n={n}")
    if SW is not None:
        print(f"    working-set normalized singular values: min={SW.min():.3e} (smallest 3: {np.sort(SW)[:3]})")
    if len(evW):
        print(f"    Z'HZ on the final working set's null space: dim={dimW} eig min={evW[0]:.4e} (next {evW[1] if len(evW)>1 else float('nan'):.4e}) max={evW[-1]:.4e}")
        out["redH_final_min"] = float(evW[0])
    convex = ev[0] >= -1e-9 * max(1.0, abs(ev[-1]))
    # convex on the equality null space is enough for a global optimum when every other constraint is a bound/row
    if convex or (len(evE) and evE[0] >= -1e-9 * max(1.0, abs(ev[-1]))):
        stc, xs, ys, _ = clarabel_qp(d)
        fs = f_of(d, xs)
        out["clarabel_status"], out["f_star"] = stc, fs
        rows, pins = active_at(d, xs)
        out["active_star"] = {"rows": len(rows), "pins": len(pins)}
        print(f"  OPTIMUM (Clarabel) status={stc} f*={fs:.12g} viol={violations(d, xs)} active rows={len(rows)} pins={len(pins)}")
        print(f"    walk final vs x*: |dx|inf={np.abs(xw - xs).max():.4e}  f_walk - f* = {f_of(d, xw) - fs:.4e}")
        wr = set(fa); ar = set(rows)
        wp = {i: 'LUX'[fb[i] - 1] if fb[i] in (1, 2, 3) else '?' for i in pins_w}
        print(f"    rows in W not active at x*: {sorted(wr - ar)[:30]} ; active at x* not in W: {sorted(ar - wr)[:30]}")
        wrong_pins = [(i, wp[i]) for i in pins_w if pins.get(i) != wp[i] and not (wp[i] == 'X')]
        miss_pins = [(i, pins[i]) for i in pins if i not in wp]
        print(f"    pins in W not at that bound at x*: {len(wrong_pins)} {wrong_pins[:30]}")
        print(f"    bounds active at x* not pinned in W: {len(miss_pins)} {miss_pins[:30]}")
        np.save(path.replace('.txt', '.xstar.npy'), xs)
    else:
        print("  H is not PSD on null(Ae): no global oracle; only local checks")
    if xfile:
        r = {}
        with open(xfile) as f:
            while True:
                l = f.readline()
                if not l: break
                t = l.split()
                if t[0] == 'status': r['status'] = int(t[1]); continue
                k = int(t[1]); r[t[0]] = np.array([float(f.readline()) for _ in range(k)])
        x2 = r['x']
        print(f"  POINT {xfile}: status={r['status']} f={f_of(d, x2):.12g} viol={violations(d, x2)}" + (f" f-f*={f_of(d,x2)-out['f_star']:.4e} |dx|inf={np.abs(x2-np.load(path.replace('.txt','.xstar.npy'))).max():.3e}" if 'f_star' in out else ''))
    if "--json" in sys.argv:
        json.dump(out, open(sys.argv[sys.argv.index("--json") + 1], "w"), indent=1, default=str)


if __name__ == "__main__":
    main()
