#!/usr/bin/env python3
"""NC2 scratch: re-solve the bound-eliminated regularized EQP of a dumped walk state in dense
arithmetic (the eqp_solve.h system: K = [H_FF + d I, Ae_F^T; Ae_F, -m I], one refinement step)
and compare with the engine's eqp.x. usage: eqp.py DUMP STATE"""
import sys
import numpy as np
sys.path.insert(0, '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/docs/notes/data/2026-09-m7-w2-r2-composite-landing/runners')
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
n = len(x)
H = d['mat']['H'].toarray(); H = H + np.triu(H, 1).T
Ae = d['mat']['Ae'].toarray(); be = d['vec']['be']; g = d['vec']['g']
lo, up = d['vec']['lower'], d['vec']['upper']
F = np.where(bs == 0)[0]; P = np.where(bs != 0)[0]
xf = np.zeros(n)
for i in P:
    xf[i] = up[i] if bs[i] == 2 else lo[i]
# the engine's trust-region window: bounds are the effective (TR) ones; take pins' values from x
xf[P] = x[P]
dl, mu = 1e-8, 1e-8
nf, me = len(F), Ae.shape[0]
K = np.zeros((nf + me, nf + me))
K[:nf, :nf] = H[np.ix_(F, F)] + dl * np.eye(nf)
K[nf:, :nf] = Ae[:, F]; K[:nf, nf:] = Ae[:, F].T
K[nf:, nf:] = -mu * np.eye(me)
rhs = np.concatenate([-g[F] - H[np.ix_(F, P)] @ xf[P], be - Ae[:, P] @ xf[P]])
y = np.linalg.solve(K, rhs)
K0 = K.copy(); K0[:nf, :nf] -= dl * np.eye(nf); K0[nf:, nf:] += mu * np.eye(me)
y1 = y + np.linalg.solve(K, rhs - K0 @ y)
for lab, yy in (('reg', y), ('refined1', y1)):
    xs = xf.copy(); xs[F] = yy[:nf]
    print(lab, '|Ae x* - be|', np.abs(Ae @ xs - be).max(), '|x* - x|inf', np.abs(xs - x).max(), '|x* - eqpx|inf', np.abs(xs - ex).max(), '|lam|', np.abs(yy[nf:]).max())
print('cond(K)', np.linalg.cond(K))
ev = np.linalg.eigvalsh(K)
print('inertia pos/neg', (ev > 0).sum(), (ev < 0).sum(), 'expected', nf, me, 'min|ev|', np.abs(ev).min())
