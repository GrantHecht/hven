#!/usr/bin/env python3
"""WA-1 r5: TRAIN totals (side.qp_minor_iters, factorizations) and status changes of each run vs a reference run.
usage: train_totals.py REF_DIR RUN_DIR..."""
import glob, os, sys
def tot(d):
    m = f = 0; st = {}
    for o in glob.glob(d + '/cells/*.out'):
        kv = dict(l.rstrip('\n').split('=', 1) for l in open(o) if '=' in l)
        m += int(kv.get('side.qp_minor_iters', 0)); f += int(kv.get('factorizations', 0))
        st[os.path.basename(o)] = kv.get('status')
    return m, f, st
A = tot(sys.argv[1])
for d in sys.argv[2:]:
    X = tot(d)
    ch = [k for k in A[2] if A[2][k] != X[2].get(k)]
    print(f"{d}: rows {len(X[2])}; qp minors {A[0]} -> {X[0]} ({100*(X[0]/A[0]-1):+.2f}%); factorizations {A[1]} -> {X[1]} ({100*(X[1]/A[1]-1):+.2f}%); status changes {len(ch)}: " + ' '.join(f"{k[:-4]}:{A[2][k]}->{X[2].get(k)}" for k in ch))
