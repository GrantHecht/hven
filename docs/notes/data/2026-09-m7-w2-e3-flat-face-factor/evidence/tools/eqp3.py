#!/usr/bin/env python3
"""E3 scratch: dense re-solve of the bound-eliminated regularized EQP of a dumped walk state
(eqp_solve.h's system, one refinement step), plus (i) the exact regularized answer's row
residual, (ii) the true contraction of the one refinement step, (iii) the PROXIMAL-centred
variant.  usage: eqp3.py DUMP STATE [delta] [mu]"""
import sys
import numpy as np
W = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9'
sys.path.insert(0, W + '/docs/notes/data/2026-09-m7-w2-r2-composite-landing/runners')
import d4load

d = d4load.load(sys.argv[1])
st = {}
with open(sys.argv[2]) as f:
    while True:
        h = f.readline().split()
        if not h:
            break
        st[h[0]] = [float(f.readline()) for _ in range(int(h[1]))]
x = np.array(st['x']); ex = np.array(st['eqpx']); bs = np.array(st['bs'], dtype=int)
aw = np.array(st.get('aw', []), dtype=int)
n = len(x)
H = d['mat']['H'].toarray(); H = H + np.triu(H, 1).T
Ae = d['mat']['Ae'].toarray(); be = d['vec']['be']; g = d['vec']['g']
Ai = d['mat']['Ai'].toarray() if d['mat']['Ai'].shape[0] else np.zeros((0, n))
bi = d['vec']['bi'] if len(aw) else np.zeros(0)
lo, up = d['vec']['lower'], d['vec']['upper']
F = np.where(bs == 0)[0]; P = np.where(bs != 0)[0]
xf = np.zeros(n)
for i in P:
    xf[i] = up[i] if bs[i] == 2 else lo[i]
xf[P] = x[P]   # the engine's TR window: pins sit at the effective bound, i.e. at x
dl = float(sys.argv[3]) if len(sys.argv) > 3 else 1e-8
mu = float(sys.argv[4]) if len(sys.argv) > 4 else 1e-8
nf, me = len(F), Ae.shape[0]
nw = len(aw)
AW = Ai[aw][:, :] if nw else np.zeros((0, n))
bW = np.array([bi[r] for r in aw]) if nw else np.zeros(0)
dim = nf + me + nw
K = np.zeros((dim, dim))
K[:nf, :nf] = H[np.ix_(F, F)] + dl * np.eye(nf)
C = np.vstack([Ae[:, F], AW[:, F]]) if (me + nw) else np.zeros((0, nf))
K[nf:, :nf] = C; K[:nf, nf:] = C.T
K[nf:, nf:] = -mu * np.eye(me + nw)
Cp = np.vstack([Ae[:, P], AW[:, P]]) if (me + nw) else np.zeros((0, len(P)))
rhs0 = np.concatenate([-g[F] - H[np.ix_(F, P)] @ xf[P],
                       np.concatenate([be, bW]) - Cp @ xf[P]])
reg = np.concatenate([dl * np.ones(nf), -mu * np.ones(me + nw)])
K0 = K - np.diag(reg)
print(f'state nf={nf} me={me} nw={nw} n={n} pinned={len(P)} dl={dl:g} mu={mu:g}')
grad = H @ x + g

def report(lab, rhs, y, y1):
    for tag, yy in (('reg', y), ('ref1', y1)):
        xs = xf.copy(); xs[F] = yy[:nf]
        r0 = rhs - K0 @ yy                     # UNREGULARIZED residual
        rrow = np.abs(Ae @ xs - be).max() if me else 0.0
        rho = r0[:nf]
        p = xs - x
        gp = float(grad @ p)
        cen = 0.5 * dl * (float(x[F] @ x[F]) - float(xs[F] @ xs[F]))
        php = float(p @ (H @ p))
        print(f'{lab:5s} {tag:4s} |Ae x-be|={rrow:.6e} |x-x_k|inf={np.abs(p).max():.6e} '
              f'|lam|inf={np.abs(yy[nf:]).max():.6e} ||rho||inf={np.abs(rho).max():.6e} '
              f'||r_row||inf={(np.abs(r0[nf:]).max() if dim>nf else 0.0):.6e} '
              f"g'p={gp:.6e} centre={cen:.6e} p'Hp={php:.6e} df={gp+0.5*php:.6e}")
    return

# --- (1) the code's absolute centring
y = np.linalg.solve(K, rhs0)
y1 = y + np.linalg.solve(K, rhs0 - K0 @ y)
report('ABS', rhs0, y, y1)
print(f'ABS   contraction ||rho_ref||/||rho_unref|| = '
      f'{np.abs((rhs0-K0@y1)[:nf]).max()/max(np.abs((rhs0-K0@y)[:nf]).max(),1e-300):.6e}   '
      f'full-residual contraction = '
      f'{np.abs(rhs0-K0@y1).max()/max(np.abs(rhs0-K0@y).max(),1e-300):.6e}')
# the EXACT regularized answer (a dense solve IS exact to rounding): its row residual is mu*lam
xs = xf.copy(); xs[F] = y[:nf]
print(f'ABS   EXACT reg answer: |Ae x-be|inf={np.abs(Ae@xs-be).max():.6e}  '
      f'mu*|lam_e|inf={mu*np.abs(y[nf:nf+me]).max():.6e}  '
      f'ratio={np.abs(Ae@xs-be).max()/max(mu*np.abs(y[nf:nf+me]).max(),1e-300):.4f}')
print(f'ABS   engine eqpx: |Ae x-be|inf={np.abs(Ae@ex-be).max():.6e}')
xs1 = xf.copy(); xs1[F] = y1[:nf]
print(f'ABS   |eqpx - dense_ref1|inf={np.abs(ex-xs1).max():.6e}  '
      f'|eqpx - dense_reg|inf={np.abs(ex-xs).max():.6e}')
# --- (2) the PROXIMAL centring: rhs free block gets + dl * x_k,F
rhsP = rhs0.copy(); rhsP[:nf] += dl * x[F]
yP = np.linalg.solve(K, rhsP)
yP1 = yP + np.linalg.solve(K, rhsP - K0 @ yP)
report('PROX', rhsP, yP, yP1)
xsP = xf.copy(); xsP[F] = yP[:nf]
print(f'PROX  EXACT reg answer: |Ae x-be|inf={np.abs(Ae@xsP-be).max():.6e}  '
      f'mu*|lam_e|inf={mu*np.abs(yP[nf:nf+me]).max():.6e}')
# --- (3) conditioning / inertia
print(f'cond(K)={np.linalg.cond(K):.6e}')
ev = np.linalg.eigvalsh(K)
print(f'inertia pos/neg/zero-ish {(ev>0).sum()} {(ev<0).sum()} expected {nf} {me+nw} '
      f'min|ev|={np.abs(ev).min():.6e}')
ev0 = np.linalg.eigvalsh(K0)
print(f'K0: cond={np.linalg.cond(K0):.6e} min|ev|={np.abs(ev0).min():.6e} '
      f'pos/neg {(ev0>0).sum()} {(ev0<0).sum()}')
# reduced Hessian on the face (flatness)
if me + nw:
    import numpy.linalg as la
    U, S, Vt = la.svd(C)
    r = int((S > S.max() * 1e-12).sum()) if S.size else 0
    Z = Vt[r:].T
    ZHZ = Z.T @ H[np.ix_(F, F)] @ Z
    print(f'face: rank(C)={r} nullity={Z.shape[1]} ||Z^T H Z||inf='
          f'{np.abs(ZHZ).max() if ZHZ.size else 0.0:.6e} '
          f'min eig ZHZ={np.linalg.eigvalsh(ZHZ).min() if ZHZ.size else float("nan"):.6e}')
