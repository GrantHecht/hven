#!/usr/bin/env python3
"""E2-D4: replay the working-set growth recorded in a D4T trace against the dump's data and, for every
admitted blocker, compute its EXACT distance from span(Ae; W) (dense least squares, rows normalized) and
whether it structurally completes an equality/member row. usage: addseq.py DUMP TRACE [--max-it K]"""
import sys, re, numpy as np
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from d4load import load
d = load(sys.argv[1]); tr = open(sys.argv[2]).read().splitlines()
stopfirst = '--stop-first' in sys.argv
maxit = int(sys.argv[sys.argv.index('--max-it')+1]) if '--max-it' in sys.argv else 10**9
n, me, mi = d['n'], d['me'], d['mi']
Ae = d['mat']['Ae'].toarray() if me else np.zeros((0, n)); Ai = d['mat']['Ai'].toarray() if mi else np.zeros((0, n))
rows = set(); pins = set()
# seed / fixed
lo, up = d['lo'], d['up']
for i in range(n):
    if lo[i] == up[i]: pins.add(i)
if d['seed']:
    sb = d['ivec']['seed_bound_state']; sa = d['ivec']['seed_ineq_active']
    for i in range(n):
        if sb[i] in (1, 2): pins.add(i)
    for r in range(mi):
        if sa[r]: rows.add(r)
def mat(rows, pins):
    B = [Ae] + ([Ai[sorted(rows)]] if rows else [])
    if pins:
        E = np.zeros((len(pins), n)); E[np.arange(len(pins)), sorted(pins)] = 1; B.append(E)
    return np.vstack(B)
def dist(a, rows, pins):
    M = mat(rows, pins); a = a / np.linalg.norm(a)
    Mn = M / np.maximum(np.linalg.norm(M, axis=1, keepdims=True), 1e-300)
    coef, *_ = np.linalg.lstsq(Mn.T, a, rcond=None)
    return np.linalg.norm(a - Mn.T @ coef)
def structural(is_row, idx, rows, pins):
    # does the candidate pin complete an eq/member row (all nonzeros pinned or = idx)?
    if is_row: return False
    P = set(pins) | {idx}
    for r in range(me):
        nz = set(np.nonzero(Ae[r])[0])
        if idx in nz and nz <= P: return ('eq', r)
    for r in rows:
        nz = set(np.nonzero(Ai[r])[0])
        if idx in nz and nz <= P: return ('row', r)
    return False
prev_block = None
it_re = re.compile(r'it=(\d+) (\w+)')
for line in tr:
    m = it_re.search(line)
    if not m: continue
    it = int(m.group(1))
    if it > maxit: break
    kw = m.group(2)
    if kw == 'STEP':
        t = line.split()
        kind, idx = t[3].split('=')[1], int(t[4])
        exch = 'exch=1' in line
        out = re.search(r'out=(row|pin)(-?\d+)', line)
        if exch:
            (rows if out.group(1) == 'row' else pins).discard(int(out.group(2)))
        if kind in ('pin', 'row'):
            a = np.zeros(n); 
            if kind == 'pin': a[idx] = 1
            else: a = Ai[idx].copy()
            dd = dist(a, rows, pins); st = structural(kind == 'row', idx, rows, pins)
            rho = re.search(r'rho=(\S+)', prev_block).group(1) if prev_block else '?'
            flag = 'EXACT-DEP' if dd < 1e-10 else ('near-dep' if dd < 1e-5 else '')
            print(f"it={it} ADD {kind} {idx} {'(exch out '+out.group(1)+out.group(2)+')' if exch else ''} dist={dd:.3e} engine_rho={rho} structural={st} {flag}")
            (pins if kind == 'pin' else rows).add(idx)
            if stopfirst and flag == 'EXACT-DEP' and not exch: break
        prev_block = None
    elif kw == 'BLOCK':
        prev_block = line
    elif kw == 'DROP':
        t = line.split(); (rows if t[3] == 'row' else pins).discard(int(t[4]))
    elif kw == 'DETECT_REMOVE':
        k = re.search(r'kind=(\d) idx=(\d+)', line); (rows if k.group(1) == '1' else pins).discard(int(k.group(2)))
prev_block = None
