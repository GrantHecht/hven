#!/usr/bin/env python3
"""Split the amended RS move map into the commit groups RS-1a..1e (design page v2, section 3)
and RS-2 (tests).  Writes groups/RS-<g>.map next to the amended map.  Every entry lands in
exactly one group; an entry matching no rule is an error, never a default.
usage: rs_groups.py <ev-dir>"""
import os, re, sys

EV = sys.argv[1]
I = 'include/hven/'

def group(old, new):
    # RS-2: everything under tests/
    if old.startswith('tests/'):
        return 'RS-2'
    # RS-1a foundations (design section 3, RS-1a)
    if old.startswith(I + 'core/detail/'): return 'RS-1a'
    if old.startswith(I + 'detail/interior/typedefs/'): return 'RS-1a'
    if old == I + 'detail/drivers/finite_screen.h': return 'RS-1a'
    if old.startswith(I + 'detail/interior/utils/'): return 'RS-1a'
    if old.startswith('src/interior/utils/'): return 'RS-1a'
    # RS-1b shared numerics and the qp split
    if old in (I + 'detail/interior/barrier_math.h', I + 'detail/interior/bound_set.h',
               I + 'detail/globalization/inertia_regularization.h',
               I + 'detail/interior/kkt_factorization.h', I + 'detail/interior/fixed_variable_row.h',
               'src/interior/kkt_factorization.cpp'):
        return 'RS-1b'
    if old.startswith(I + 'detail/kkt/') or old.startswith(I + 'detail/qp/'): return 'RS-1b'
    if old.startswith('src/kkt/') or old.startswith('src/qp/'): return 'RS-1b'
    # RS-1e: shared surface, trace, clean-up (checked before the engine rules: the
    # drivers/ entries are split by their destination)
    if old == I + 'detail/warmstart/warm_start.h' or old == 'src/warmstart/warm_start.cpp': return 'RS-1e'
    if old.endswith('/.gitkeep'): return 'RS-1e'
    if new.startswith((I + 'solver/', I + 'trace/', I + 'detail/solver/', 'src/solver/', 'src/trace/')):
        return 'RS-1e'
    # RS-1c interior-point engine
    if new.startswith((I + 'ipm/', I + 'detail/ipm/', 'src/ipm/')): return 'RS-1c'
    # RS-1d SQP engine
    if new.startswith((I + 'sqp/', I + 'detail/sqp/', 'src/sqp/')): return 'RS-1d'
    raise SystemExit('UNASSIGNED: %s -> %s' % (old, new))

groups = {}
for line in open(os.path.join(EV, 'RS-move-map-v2-amended.txt')):
    line = line.rstrip('\n')
    if not line.strip(): continue
    old, new = line.split(' -> ', 1)
    groups.setdefault(group(old, new), []).append(line)
os.makedirs(os.path.join(EV, 'groups'), exist_ok=True)
tot = 0
for g in sorted(groups):
    open(os.path.join(EV, 'groups', g + '.map'), 'w').write('\n'.join(groups[g]) + '\n')
    n_del = sum(1 for l in groups[g] if '-> DELETE' in l)
    print('%s: %d entries (%d deletions)' % (g, len(groups[g]), n_del))
    tot += len(groups[g])
print('total', tot)
