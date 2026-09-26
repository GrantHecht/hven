#!/usr/bin/env python3
"""NC2 fix1: diff EVERY column of two corpus tags, both algebras.
usage: rowcmp.py BASE TAG [--alg border,refactorize] [--skip col1,col2]
A missing tag file for an algebra is reported, not silently skipped."""
import re, sys, os
S = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'corpus')
N = {0: 'opt', 1: 'cap', 2: 'inf', 3: 'num'}


def key(path):
    m = re.search(r'dumps/([^/]+)/(qp\d+)\.txt', path)
    if m:
        return f'{m.group(1)}/{m.group(2)}'
    m = re.search(r'(Cs_STEENBRB_qp\d)\.txt', path)
    return 'R1_' + m.group(1)


def load(t, alg):
    out = {}
    missing = []
    for lst in ('old36', 'new15'):
        f = f'{S}/{t}-{alg}-{lst}.txt'
        if not os.path.exists(f):
            missing.append(os.path.basename(f))
            continue
        for line in open(f):
            parts = line.split()
            if not parts:
                continue
            out[key(parts[0])] = dict(p.split('=', 1) for p in parts[1:] if '=' in p)
    return out, missing


a, b = sys.argv[1], sys.argv[2]
algs = ('border', 'refactorize')
skip = set()
for i, x in enumerate(sys.argv):
    if x == '--alg':
        algs = tuple(sys.argv[i + 1].split(','))
    if x == '--skip':
        skip = set(sys.argv[i + 1].split(','))
total_movers = 0
for alg in algs:
    A, ma = load(a, alg)
    B, mb = load(b, alg)
    print(f'## {alg}   ({a}: {len(A)} rows, missing {ma})  ({b}: {len(B)} rows, missing {mb})')
    for k in sorted(A):
        x, y = A[k], B.get(k)
        if y is None:
            print(f'  {k}: ABSENT in {b}')
            total_movers += 1
            continue
        diffs = []
        for col in x:
            if col in skip:
                continue
            if col not in y:
                diffs.append(f'{col}: {x[col]} -> ABSENT')
            elif x[col] != y[col]:
                diffs.append(f'{col}: {x[col]} -> {y[col]}')
        for col in y:
            if col not in x and col not in skip:
                diffs.append(f'{col}: ABSENT -> {y[col]}')
        if diffs:
            total_movers += 1
            st = f"{N.get(int(x['status']),'?')}->{N.get(int(y['status']),'?')}"
            print(f'  {k} [{st}]: ' + '; '.join(diffs))
    for k in sorted(B):
        if k not in A:
            print(f'  {k}: ABSENT in {a}')
            total_movers += 1
print(f'TOTAL ROWS WITH ANY COLUMN CHANGE: {total_movers}')
