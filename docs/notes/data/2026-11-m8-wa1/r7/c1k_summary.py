#!/usr/bin/env python3
"""WA-1 rider 2, ruling 1: the kOk regularized-step chain measurement. Reads the `WA1R chain` lines (scratch commit
3b381fc6d, env HVEN_WA1_MEAS) a TRAIN enumeration's cells wrote to stderr. A chain = consecutive kOk regularized steps
on one working set; it ends at the first pass that is not one. Per chain: f0 (start), fnr (the no-rule landing: the
refined p's capped step from the chain's start), fl (after the last regularized step), fe (after the following pass).
POSTPONED: the following pass lands at or above the no-rule f (fe >= fnr - floor); ABOVE_START: fe > f0 + floor.
follow codes: -2 the following pass did not reach the rule's site, 0 not asked, 1 fp ride, 2 fp rung, 3 decline,
4 degenerate, 5 merit falls, 7 guard ride. usage: c1k_summary.py <cells dir>"""
import collections, glob, os, sys
kv = lambda line: dict(t.split('=', 1) for t in line.split() if '=' in t)
floor = lambda f: 1e-12 * max(1.0, abs(f))
T = collections.Counter()
follow = collections.Counter()
print('cell chains steps postponed above_start below_norule fe_vs_fl_rose ws_same solve_end')
for e in sorted(glob.glob(os.path.join(sys.argv[1], '*.err'))):
    ch = [kv(l) for l in open(e, errors='replace') if l.startswith('WA1R chain')]
    if not ch:
        continue
    c = collections.Counter()
    for d in ch:
        f0, fnr, fl, fe = (float(d[k]) for k in ('f0', 'fnr', 'fl', 'fe'))
        n = int(d['n'])
        c['chains'] += 1; c['steps'] += n
        if d['end'].startswith('solve_end'):
            c['solve_end'] += 1
            fl = fe  # the solve ended: x is the chain's last landing or the following pass's
        c['postponed'] += fe >= fnr - floor(fnr)
        c['above_start'] += fe > f0 + floor(f0)
        c['below_norule'] += fe < fnr - floor(fnr)
        c['rose_after'] += fe > fl + floor(fl)
        c['ws_same'] += d['ws_same'] == '1'
        follow[d['follow']] += 1
    T.update(c)
    print(os.path.basename(e)[:-4], c['chains'], c['steps'], c['postponed'], c['above_start'], c['below_norule'],
          c['rose_after'], c['ws_same'], c['solve_end'])
print('TOTAL', ' '.join(f'{k}={T[k]}' for k in ('chains', 'steps', 'postponed', 'above_start', 'below_norule', 'rose_after', 'ws_same', 'solve_end')))
print('FOLLOW', ' '.join(f'{k}:{v}' for k, v in sorted(follow.items())))
