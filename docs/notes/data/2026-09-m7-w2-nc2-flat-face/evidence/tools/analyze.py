#!/usr/bin/env python3
"""NC2 scratch: summarize NC2 probe traces. usage: analyze.py TRACE... """
import re, sys, collections

OUT = {0: 'stepped', 1: 'unbounded', 2: 'vetoed', 3: 'depblocker', 4: 'declined'}


def kv(line):
    return dict(re.findall(r'(\w+)=(\S+)', line))


for path in sys.argv[1:]:
    gates = []
    rides = {}
    undos = 0
    exch = []
    drops = []
    passes = []
    terms = []
    end = None
    first_eq = None
    retreats = 0
    last_gate = None
    for line in open(path):
        if not line.startswith('NC2 '):
            continue
        tag = line.split()[1]
        d = kv(line)
        if tag == 'PASS':
            passes.append(d)
            if first_eq is None and float(d['eq']) > 1e-8:
                first_eq = (d['it'], d['eq'])
        elif tag == 'GATE':
            gates.append(d)
        elif tag == 'RIDE':
            rides[d['it']] = int(d['outcome'])
        elif tag == 'UNDO':
            undos += 1
        elif tag == 'EXCH':
            exch.append(d)
        elif tag == 'DROP':
            drops.append((d, line.split()[3], line.split()[4]))
        elif tag == 'TERM':
            terms.append(d)
        elif tag == 'END':
            end = d
        elif tag == 'RETREAT':
            retreats += 1
    print(f'== {path.split("/")[-1]}  END {end}  drops={len(drops)} gates={len(gates)} undos={undos} exch={len(exch)} retreats={retreats} first_eq>1e-8={first_eq}')
    cls = collections.Counter()
    for g in gates:
        o = rides.get(g['it'])
        curv, tol = float(g['curv']), float(g['tol'])
        slope, stol = float(g['slope']), float(g['slope_tol'])
        pred, dd = float(g['pred']), float(g['dd'])
        flat = abs(slope) <= stol
        if o == 2:
            reason = 'i-nullspace' if g['nsok'] == '0' else ('ii-flat' if flat else 'ii-disagree')
        else:
            reason = OUT.get(o, 'none')
        signagree = (slope < 0) == (pred < 0)
        cls[(reason, 'curv0' if curv == 0 else ('curv<0' if curv < 0 else 'curv>0'), 'agree' if signagree else 'DISAGREE')] += 1
    for k, v in sorted(cls.items()):
        print('   ', k, v)
    for g in gates[:3] + gates[-3:]:
        print('    GATE', ' '.join(f'{k}={g[k]}' for k in ('it', 'curv', 'pnorm', 'slope', 'slope_tol', 'dd', 'zs', 'pred', 'nsok')), 'outcome', rides.get(g['it']))
    for t in terms[-2:]:
        print('    TERM', t)
    if passes:
        print('    last PASS', passes[-1])
