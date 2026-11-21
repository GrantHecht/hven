#!/usr/bin/env python3
"""WA-1 r5: the C1 measurement. Reads the WA1M lines (scratch commit 2f9ad0e67, env HVEN_WA1_MEAS) a
TRAIN enumeration's cells wrote to stderr and reports, per cell, what the pass after each footprint
MOVE did on the unchanged working set. usage: c1_summary.py <cells dir>"""
import glob, os, re, sys
cells = sys.argv[1]
kv = lambda line: dict(t.split('=', 1) for t in line.split() if '=' in t)
tot = dict(cells=0, moves=0, chains=0, chain_end_ascent=0, chain_end_ride=0, chain_end_other=0, declines=0, decline_rose=0)
rows = []
for e in sorted(glob.glob(os.path.join(cells, '*.err'))):
    lines = [l for l in open(e, errors='replace') if l.startswith('WA1M ')]
    if not lines:
        continue
    tot['cells'] += 1
    moves = chains = asc = ride = other = dec = dec_rose = 0
    maxchain = 0
    for i, l in enumerate(lines):
        d = kv(l)
        if l.startswith('WA1M fp') and d['code'] == '3':
            moves += 1
        if l.startswith('WA1M fp') and d['code'] == '1':
            dec += 1
        if l.startswith('WA1M step') and d['code'] == '1' and float(d['df']) > 0:
            dec_rose += 1
        if l.startswith('WA1M am '):
            maxchain = max(maxchain, int(d['chain']))
            # the pass after a move: what did it do (next fp line of the same solve/iter, else a step line)
            nxt = [m for m in lines[i + 1:i + 3] if kv(m).get('iter') == d['iter'] and kv(m).get('solve') == d['solve']]
            fp = [kv(m) for m in nxt if m.startswith('WA1M fp')]
            st = [kv(m) for m in nxt if m.startswith('WA1M step')]
            if fp and fp[0]['code'] == '3':
                continue  # the chain continues with another move
            chains += 1
            if st and float(st[0]['df']) > 0:
                asc += 1
            elif fp and fp[0]['code'] == '2':
                ride += 1
            else:
                other += 1
    rows.append((os.path.basename(e)[:-4], moves, chains, asc, ride, other, maxchain, dec, dec_rose))
    for k, v in zip(['moves', 'chains', 'chain_end_ascent', 'chain_end_ride', 'chain_end_other', 'declines', 'decline_rose'], [moves, chains, asc, ride, other, dec, dec_rose]):
        tot[k] += v
print('cell moves chains chain_ends_in_ascending_ordinary_step chain_ends_in_ride chain_ends_other max_chain code1_declines code1_steps_rose')
for r in rows:
    print(' '.join(str(x) for x in r))
print('TOTAL', ' '.join(f'{k}={v}' for k, v in tot.items()))
