#!/usr/bin/env python3
"""NC2 fix1 item 1(a): cycle/period analysis of a per-pass sequence CSV (NC2FIX1_SEQ).

usage: cyc.py SEQ.csv [tail_n]
Reports: action histogram, the zero-motion tail, and the smallest period p such that the
LAST tail_n passes satisfy ws_sig_in[i] == ws_sig_in[i-p] for every i in the tail.
"""
import csv
import sys
from collections import Counter

ACT = {0: 'unset', 1: 'startrepair', 2: 'postproberestart', 3: 'dropdantzig', 4: 'dropzeroprobe',
       5: 'suspectescalation', 6: 'terminal', 7: 'ridestepped', 8: 'rideunbounded', 9: 'add',
       10: 'moveonly', 11: 'eliminatedretreat', 12: 'rideundone'}

path = sys.argv[1]
tail_n = int(sys.argv[2]) if len(sys.argv) > 2 else 400
rows = list(csv.DictReader(open(path)))
print(f'{path}: {len(rows)} passes')
print('actions:', dict(Counter(ACT.get(int(r["action"]), r["action"]) for r in rows)))

sig = [int(r['ws_sig_in']) for r in rows]
size = [int(r['ws_size_in']) for r in rows]
xinf = [float(r['x_inf']) for r in rows]

tail = sig[-tail_n:] if len(sig) > tail_n else sig[:]
period = None
for p in range(1, max(2, len(tail) // 3)):
    if all(tail[i] == tail[i - p] for i in range(p, len(tail))):
        period = p
        break
print(f'ws_sig_in period over last {len(tail)} passes: {period}')
print(f'distinct ws_sig_in over last {len(tail)}: {len(set(tail))}')
print(f'distinct ws_size_in over last {len(tail)}: {sorted(set(size[-len(tail):]))}')
print(f'distinct x_inf over last {len(tail)}: {len(set(xinf[-len(tail):]))}')

# the recurrence structure: how often each signature repeats in the tail
c = Counter(tail)
print('top tail signatures (count):', c.most_common(6))

# how many passes moved the iterate at all (dx_inf > step_tol)
moved = sum(1 for r in rows
            if r['dx_inf'] not in ('', 'nan') and float(r['dx_inf']) > float(r['step_tol']))
print(f'passes with dx_inf > step_tol: {moved} of {len(rows)}')
print('last 24 (minor, action, size_in, sig_in, dx_inf, step_tol, released, block):')
for r in rows[-24:]:
    print('  ', r['minor'], ACT.get(int(r['action']), r['action']), r['ws_size_in'],
          r['ws_sig_in'], f"{float(r['dx_inf']):.3e}" if r['dx_inf'] != 'nan' else 'nan',
          f"{float(r['step_tol']):.3e}" if r['step_tol'] != 'nan' else 'nan',
          r['released_index'], r['block_index'])
