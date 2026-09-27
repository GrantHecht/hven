#!/usr/bin/env python3
"""NC2 scratch: slope/curvature along each null-space direction of a dumped face, and the EQP step
read against the released index. usage: line.py DUMP STATE IDX"""
import sys
import numpy as np
import scipy.linalg as sl
sys.path.insert(0, '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/docs/notes/data/2026-09-m7-w2-r2-composite-landing/runners')
import d4load

d = d4load.load(sys.argv[1])
idx = int(sys.argv[3])
st = {}
with open(sys.argv[2]) as f:
    while True:
        h = f.readline().split()
        if not h:
            break
        st[h[0]] = [float(f.readline()) for _ in range(int(h[1]))]
x = np.array(st['x']); ex = np.array(st['eqpx']); bs = np.array(st['bs'], dtype=int)
H = d['mat']['H'].toarray(); H = H + np.triu(H, 1).T
Ae = d['mat']['Ae'].toarray(); g = d['vec']['g']
F = np.where(bs == 0)[0]
ZF = sl.null_space(Ae[:, F], rcond=1e-10)
Z = np.zeros((len(x), ZF.shape[1])); Z[F, :] = ZF
gr = H @ x + g
for k in range(min(Z.shape[1], 6)):
    z = Z[:, k]
    c = z @ H @ z
    print('dir', k, 'slope', gr @ z, 'curv', c, 'z_idx', z[idx], 't*', -(gr @ z) / c if c != 0 else float('inf'),
          'bounds', d['vec']['lower'][idx], d['vec']['upper'][idx], 'x_idx', x[idx], 'bs_idx', bs[idx])
p = ex - x
print('p_idx', p[idx], '|p|', np.linalg.norm(p), 'pHp/pp', p @ H @ p / (p @ p), 'g.p', gr @ p)
