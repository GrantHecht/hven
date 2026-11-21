#!/usr/bin/env python3
# For each grey re-read: test history, re-read history, flip, features. Usage: greys.py <logs...>
import sys
sys.path.insert(0, __import__('os').path.dirname(__file__))
from parse import records, flist
rows = []
for r in records(sys.argv[1:]):
    C = r['C']
    if C['grey'] != '1': continue
    T = r.get('test'); K = r.get('cert')
    if T is None or K is None: continue
    tr = flist(T['rhos']); kr = flist(K['rhos']); kres = flist(K['ress'])
    rows.append(dict(file=r['file'], flip=int(C['flip']), rho=float(T['rho']), tsol=int(T['solves']),
                     trhos=tr, krhos=kr, kres=kres, kbrk=int(K['brk']), bord=int(T['bord']),
                     v1=float(T['v1']), vmaxsc=float(T['vmaxsc']), anorm=float(T['anorm']),
                     au=float(T['au']), un=float(T['un']), rc=float(T['rc']), rx=float(T['rx']),
                     amem=float(T['amemmax']), rcert=float(C['rcert']), floor=float(C['floor'])))
if __name__ == '__main__':
    import collections
    print('greys', len(rows), 'flips', sum(r['flip'] for r in rows))
    for r in rows:
        if r['flip'] or '-v' in sys.argv:
            pass
    # print compact table
    for r in rows:
        k = r['tsol'] - 1
        kr = r['krhos']
        q = [kr[i+1]/kr[i] if kr[i] > 0 else 0 for i in range(len(kr)-1)]
        print('%d rho=%.3g k=%d bord=%d krhos=%s q=%s v1=%.3g vsc/a=%.3g LB=%.3g rc=%.2g %s' % (
            r['flip'], r['rho'], k, r['bord'], ','.join('%.2g' % x for x in kr), ','.join('%.2g' % x for x in q),
            r['v1'], r['vmaxsc']/r['anorm'], (abs(r['au'])/(r['un']*r['anorm']) if r['un'] > 0 else 0), r['rc'],
            r['file'].split('/')[-1]))
