#!/usr/bin/env python3
"""Reads one runtime-leg directory (perf/, raw/) and prints the neutrality reading.

usage: analyze.py <leg-dir> <mode> <rounds>

Per arm and cell: the MEDIAN over rounds of each event count. Corpus total = the
SUM of the per-cell medians (never a mean of ratios). Ratios are B/A (B = B1,
A = the pin). The counter STOP condition: each (round, cell) pair's two CSV rows
are compared with scripts/compare_replay.py; any difference is reported first.
"""
import glob
import os
import statistics
import subprocess
import sys

leg, mode, rounds = sys.argv[1], sys.argv[2], int(sys.argv[3])
here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.abspath(os.path.join(here, '../../../../..'))
compare = os.path.join(repo, 'scripts/compare_replay.py')


def read_perf(path):
    vals = {}
    for line in open(path):
        parts = line.strip().split(',')
        if len(parts) < 3 or line.startswith('#'):
            continue
        try:
            vals[parts[2]] = float(parts[0])
        except ValueError:
            vals[parts[2]] = float('nan')
    return vals


cells = sorted({os.path.basename(p).split('-')[3].rsplit('.', 1)[0]
                for p in glob.glob(os.path.join(leg, 'perf', '%s-r1-A-*.txt' % mode))})
if mode == 'interior':
    cells = ['all']

events = None
table = {}
for cell in cells:
    for arm in 'AB':
        runs = []
        for r in range(1, rounds + 1):
            p = os.path.join(leg, 'perf', '%s-r%d-%s-%s.txt' % (mode, r, arm, cell))
            runs.append(read_perf(p))
        if events is None:
            events = sorted(runs[0].keys())
        table[(cell, arm)] = {e: statistics.median([run.get(e, float('nan')) for run in runs]) for e in events}
        table[(cell, arm, 'spread')] = {
            e: (max(run[e] for run in runs) - min(run[e] for run in runs)) / max(1.0, statistics.median([run[e] for run in runs]))
            for e in events}

# counter stop condition
mismatch = 0
checked = 0
for cell in cells:
    for r in range(1, rounds + 1):
        a = os.path.join(leg, 'raw', '%s-r%d-A-%s.csv' % (mode, r, cell))
        b = os.path.join(leg, 'raw', '%s-r%d-B-%s.csv' % (mode, r, cell))
        if not (os.path.exists(a) and os.path.exists(b)):
            print('MISSING CSV', a, b)
            mismatch += 1
            continue
        res = subprocess.run([sys.executable, compare, a, b, '%s r%d %s' % (mode, r, cell)],
                             capture_output=True, text=True)
        checked += 1
        head = res.stdout.splitlines()[0] if res.stdout else res.stderr
        if 'differences: 0' not in head:
            mismatch += 1
            print('COUNTER DIFF', head)
print('counter check: %d (round,cell) pairs compared, %d with a difference or missing' % (checked, mismatch))

print()
print('%-30s %s' % ('cell', '  '.join('%-22s' % ('B/A ' + e) for e in events if e.startswith('instructions') or True)))
sums = {arm: {e: 0.0 for e in events} for arm in 'AB'}
worst_cell = (1.0, '')
for cell in cells:
    row = []
    for e in events:
        a, b = table[(cell, 'A')][e], table[(cell, 'B')][e]
        sums['A'][e] += a
        sums['B'][e] += b
        row.append('%-22.5f' % (b / a if a else float('nan')))
    ia, ib = table[(cell, 'A')]['instructions:u'], table[(cell, 'B')]['instructions:u']
    ratio = ib / ia
    if abs(ratio - 1.0) > abs(worst_cell[0] - 1.0):
        worst_cell = (ratio, cell)
    print('%-30s %s' % (cell, '  '.join(row)))
print()
for e in events:
    print('corpus %-34s A=%.6e B=%.6e  B/A=%.6f' % (e, sums['A'][e], sums['B'][e], sums['B'][e] / sums['A'][e]))
ratio = sums['B']['instructions:u'] / sums['A']['instructions:u']
print()
print('worst per-cell instructions ratio: %.5f (%s)' % worst_cell)
cells_out = [c for c in cells if not 0.99 <= table[(c, 'B')]['instructions:u'] / table[(c, 'A')]['instructions:u'] <= 1.01]
print('cells outside 0.99-1.01: %s' % (cells_out or 'none'))
spread = max(table[(c, arm, 'spread')]['instructions:u'] for c in cells for arm in 'AB')
print('largest within-arm round-to-round instruction spread: %.5f' % spread)
print('CORPUS instructions B/A = %.6f -> %s' % (ratio, 'WITHIN +-0.5 %' if abs(ratio - 1.0) <= 0.005 else 'OUTSIDE +-0.5 %'))
