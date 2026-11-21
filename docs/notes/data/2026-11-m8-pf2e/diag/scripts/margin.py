#!/usr/bin/env python3
# Margin of kDepStagnation: the largest step contraction q observed where a cut would have changed the
# outcome had q been above the threshold (flipping re-reads; tests whose outcome was not a stall admission).
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from parse import records, flist
TOL, UND, STEPS = 1e-6, 1e-3, 3
REACH = float(os.environ.get('REACH', '1'))
mq_flip = []; mq_test = []
for rec in records(sys.argv[1:]):
    C = rec['C']; T = rec.get('test'); K = rec.get('cert')
    if T is None: continue
    tr = flist(T['rhos']); ress = flist(T['ress'])
    if T['brk'] == '1': tr = tr[:-1]; ress = ress[:-1]
    target = float(T['target'])
    if T['screen'] == '0' and len(tr) > 1 and (C['verdict'] != '0' or C['grey'] == '1'):
        qs = [tr[j]/tr[j-1] for j in range(1, len(tr)) if STEPS - j > 0 and ress[j] > target and tr[j-1] > 0 and tr[j]*0.5**(STEPS-j) >= UND*REACH]
        if qs: mq_test.append((max(qs), T['rhos'], rec['file']))
    if C['grey'] == '1' and C['flip'] == '1' and K is not None:
        k = int(T['solves']) - 1
        kr = flist(K['rhos'])
        if K['brk'] == '1': kr = kr[:-1]
        qs = [kr[j]/kr[j-1] for j in range(max(k, 1), len(kr)) if STEPS - j > 0 and kr[j-1] > 0 and kr[j]*0.5**(STEPS-j) > TOL*REACH]
        if qs: mq_flip.append((max(qs), K['rhos'], rec['file']))
for name, l in (('flip re-reads', mq_flip), ('non-stall tests', mq_test)):
    l.sort(reverse=True)
    print(name, len(l), 'largest step q at a probe point:')
    for x in l[:6]: print('   q=%.3g rhos=%s %s' % (x[0], x[1], x[2].split('dlog/')[-1]))
