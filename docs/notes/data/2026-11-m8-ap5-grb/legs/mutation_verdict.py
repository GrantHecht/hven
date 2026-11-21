#!/usr/bin/env python3
"""M8 AP-5 G-RB (../DECLARATION.md M): the control fails nothing; every MUST-KILL id is killed by a failing test; r1 is REPORTED and may survive.
usage: mutation_verdict.py <mutation-table.csv> <out>"""
import csv
import sys

MUST = ['s1', 's2', 'l1', 'l2', 'o1', 'o2', 'o3', 'n1', 'n2', 'p1', 't1']
REPORTED = ['r1']
BUILD_KILL = set()
rows = {r['id']: r for r in csv.DictReader(open(sys.argv[1]))}
out, ok = [], True
ctl = rows.get('control')
if ctl is None or ctl['killed_by'].strip() != 'NONE':
    ok = False
    out.append('control: FAILS SOMETHING (%s)' % (ctl['killed_by'] if ctl else 'missing'))
else:
    out.append('control: fails nothing')
for mid in MUST + REPORTED:
    r = rows.get(mid)
    if r is None:
        state = 'MISSING'
    elif r['build'] == 'BUILD_FAIL':
        state = 'KILLED (build)' if mid in BUILD_KILL else 'INSTRUMENT FAULT (build)'
    elif r['build'] != 'ok':
        state = 'INSTRUMENT FAULT (%s)' % r['build']
    elif r['killed_by'].strip() in ('', 'SURVIVES'):
        state = 'SURVIVES'
    else:
        state = 'KILLED by ' + r['killed_by'].strip()
    must = mid in MUST
    if must and not state.startswith('KILLED'):
        ok = False
    out.append('%-4s %s %s' % (mid, 'must-kill' if must else 'reported ', state))
out.append('MUTATION_VERDICT ' + ('PASS' if ok else 'FAIL'))
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
print('\n'.join(out))
sys.exit(0 if ok else 1)
