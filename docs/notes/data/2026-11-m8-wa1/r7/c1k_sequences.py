#!/usr/bin/env python3
"""WA-1 rider 2, ruling 1: kOk regularized-step SEQUENCES. Merges the `WA1R chain` records of one solve whose regularized
steps fall on consecutive passes (a `restart` record: the next pass took another regularized step on a changed working set)
into one sequence: f0 and fnr (the no-rule landing) of its first step, fe after the first pass that took no regularized step.
POSTPONED: that pass lands at or above the no-rule f of the sequence's first step. usage: c1k_sequences.py <cells dir>"""
import collections, glob, os, sys
kv = lambda line: dict(t.split('=', 1) for t in line.split() if '=' in t)
fl = lambda f: 1e-12 * max(1.0, abs(f))
T = collections.Counter(); fol = collections.Counter()
print('cell sequences steps ended_by_pass postponed below_norule above_start following_step_restores_rows solve_end')
for e in sorted(glob.glob(os.path.join(sys.argv[1], '*.err'))):
    recs = [kv(l) for l in open(e, errors='replace') if l.startswith('WA1R chain')]
    if not recs:
        continue
    seqs, cur = [], None
    for d in recs:
        if cur is not None and cur[-1]['end'] == 'restart' and d['solve'] == cur[-1]['solve'] and int(d['iter0']) == int(cur[-1]['end_iter']):
            cur.append(d)
        else:
            if cur: seqs.append(cur)
            cur = [d]
    if cur: seqs.append(cur)
    c = collections.Counter()
    for s in seqs:
        a, z = s[0], s[-1]
        f0, fnr, fe = float(a['f0']), float(a['fnr']), float(z['fe'])
        c['sequences'] += 1; c['steps'] += sum(int(d['n']) for d in s)
        if z['end'].startswith('solve_end'):
            c['solve_end'] += 1; continue
        c['ended_by_pass'] += 1
        fol[z['follow']] += 1
        p = fe >= fnr - fl(fnr)
        c['postponed'] += p; c['below_norule'] += not p
        c['above_start'] += fe > f0 + fl(f0)
        c['restores'] += float(z['re']) < 0.5 * float(z['rl'])
    T.update(c)
    print(os.path.basename(e)[:-4], *(c[k] for k in ('sequences', 'steps', 'ended_by_pass', 'postponed', 'below_norule', 'above_start', 'restores', 'solve_end')))
print('TOTAL', ' '.join(f'{k}={T[k]}' for k in ('sequences', 'steps', 'ended_by_pass', 'postponed', 'below_norule', 'above_start', 'restores', 'solve_end')))
print('FOLLOW', ' '.join(f'{k}:{v}' for k, v in sorted(fol.items())))
