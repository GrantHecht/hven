#!/usr/bin/env python3
"""NC2 scratch: objective / KKT of every dump whose status or minors moved between two tags (E2 on),
both algebras. usage: fcmp.py BASE TAG"""
import re, sys
S = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/nc2-fix1/corpus'
N = {0: 'opt', 1: 'cap', 2: 'inf', 3: 'num'}


def key(path):
    m = re.search(r'dumps/([^/]+)/(qp\d+)\.txt', path)
    if m:
        return f'{m.group(1)}/{m.group(2)}'
    m = re.search(r'(Cs_STEENBRB_qp\d)\.txt', path)
    return 'R1_' + m.group(1)


def load(t, alg):
    out = {}
    for lst in ('old36', 'new15'):
        try:
            for line in open(f'{S}/{t}-{alg}-{lst}.txt'):
                parts = line.split()
                out[key(parts[0])] = dict(p.split('=', 1) for p in parts[1:] if '=' in p)
        except FileNotFoundError:
            pass
    return out


a, b = sys.argv[1], sys.argv[2]
for alg in ('border', 'refactorize'):
    A, B = load(a, alg), load(b, alg)
    print(f'## {alg}')
    for k in A:
        x, y = A[k], B.get(k)
        if y is None or (x['status'], x['minors'], x['f']) == (y['status'], y['minors'], y['f']):
            continue
        fx, fy = float(x['f']), float(y['f'])
        rel = abs(fy - fx) / max(1.0, abs(fx)) if fx == fx and fy == fy else float('nan')
        print(f"  {k}: {N[int(x['status'])]} {x['minors']} f={x['f']} -> {N[int(y['status'])]} {y['minors']} f={y['f']} (rel {rel:.2e}) stat {x['stat']}->{y['stat']} sgn {y['sgn']}")
