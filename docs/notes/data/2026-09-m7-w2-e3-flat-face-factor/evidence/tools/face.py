#!/usr/bin/env python3
"""NC2 scratch: analyze a dumped walk state (dumpstate.sh) against its D4 dump: the face's
equality consistency, rank, reduced Hessian, and the exact EQP.  usage: face.py DUMP STATE"""
import sys
import numpy as np
import scipy.linalg as sl
sys.path.insert(0, '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/docs/notes/data/2026-09-m7-w2-r2-composite-landing/runners')
import d4load

d = d4load.load(sys.argv[1])
n = d['n']
st = {}
with open(sys.argv[2]) as f:
    while True:
        h = f.readline().split()
        if not h:
            break
        k, m = h[0], int(h[1])
        st[k] = [float(f.readline()) for _ in range(m)]
x = np.array(st['x']); ex = np.array(st['eqpx']); bs = np.array(st['bs'], dtype=int)
H = d['mat']['H'].toarray(); H = H + np.triu(H, 1).T
Ae = d['mat']['Ae'].toarray(); be = d['vec']['be']; g = d['vec']['g']
F = np.where(bs == 0)[0]
print('n', n, 'free', len(F), 'me', Ae.shape[0])
print('|Ae x - be|', np.abs(Ae @ x - be).max(), '|Ae eqpx - be|', np.abs(Ae @ ex - be).max())
AF = Ae[:, F]
sv = sl.svdvals(AF)
rank = int((sv > 1e-10 * sv[0]).sum())
print('Ae_F shape', AF.shape, 'rank', rank, 'smallest sv', sv[-5:] if len(sv) > 5 else sv)
Z = sl.null_space(AF, rcond=1e-10)
print('null(Ae_F) dim', Z.shape[1])
if Z.shape[1]:
    HZ = Z.T @ H[np.ix_(F, F)] @ Z
    ev = np.linalg.eigvalsh(HZ)
    print("Z'HZ eig min/max", ev.min(), ev.max(), 'n_neg', (ev < -1e-10).sum(), 'n_zero', (np.abs(ev) <= 1e-10).sum())
    gF = (H @ x + g)[F]
    rg = Z.T @ gF
    print('|Z^T grad| ', np.abs(rg).max())
    # component of the reduced gradient on the zero-curvature subspace
    w, V = np.linalg.eigh(HZ)
    flat = V[:, np.abs(w) <= 1e-10]
    if flat.shape[1]:
        print('|flat^T Z^T grad|', np.abs(flat.T @ rg).max(), '(nonzero => the face is unbounded along a ray)')
p = ex - x
print('p inf', np.abs(p).max(), 'p on pins', np.abs(p[bs != 0]).max() if (bs != 0).any() else 0)
print('Ae p inf', np.abs(Ae @ p).max())
# regularization-bias check: at a declared KKT point, is Z'grad ~ delta * Z'x ?
if Z.shape[1]:
    gF = (H @ x + g)[F]
    print('|Z^T grad|', np.abs(Z.T @ gF).max(), ' delta*|Z^T x_F|', 1e-8 * np.abs(Z.T @ x[F]).max(), ' |x_F|inf', np.abs(x[F]).max())
    print('corr(Z^T grad, Z^T x_F)', float(np.dot(Z.T @ gF, Z.T @ x[F]) / (np.linalg.norm(Z.T @ gF) * np.linalg.norm(Z.T @ x[F]) + 1e-300)))
# exact multipliers (least squares over all n stationarity rows) and the pins' reduced costs
P = np.where(bs != 0)[0]
M = np.hstack([Ae.T, -np.eye(n)[:, P]])
gt = H @ x + g
sol, res, rk, _ = np.linalg.lstsq(M, -gt, rcond=None)
lam = sol[:Ae.shape[0]]; z = sol[Ae.shape[0]:]
print('stationarity LS residual', np.abs(M @ sol + gt).max(), 'rank', rk, 'of', M.shape[1])
wrong = []
for k, i in enumerate(P):
    s = bs[i]
    v = -z[k] if s == 1 else (z[k] if s == 2 else 0.0)
    if v > 1e-9:
        wrong.append((float(v), int(i), int(s)))
wrong.sort(reverse=True)
print('exact wrong-signed pin multipliers (viol, idx, side):', wrong[:8], 'count', len(wrong))
