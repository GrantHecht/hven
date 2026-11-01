#!/usr/bin/env python3
"""R2: fold one run's .err into (a) the restoration sub-solve's per-major table from R2JUDGE/R1MAJ/R1QP depth=1 lines,
(b) a one-line cause count: rejects by cause (f-term: ared_f<0 with pred_v>0 exact; QP failure), accepts with ratio<0.75
(no growth), halvings, max radius, and (c) the RESTO_END / main-loop lines. usage: subsum.py ERRFILE [--table]"""
import re, sys
def kv(l):
    l = l.replace('pred_v=ared_v=', 'pred_v=')
    return dict(m.groups() for m in re.finditer(r"(\w+)=(\S+)", l))
path = sys.argv[1]; table = '--table' in sys.argv
L = open(path).read().splitlines()
qp = {}; maj = {}; jd = {}; om = []
for l in L:
    if l.startswith('R1QP depth=1'): d = kv(l); qp[int(d['major'])] = d
    elif l.startswith('R1MAJ depth=1'): d = kv(l); maj[int(d['major'])] = d
    elif l.startswith('R2JUDGE depth=1'): d = kv(l.split(' | ')[0]); d.update(kv(l.split(' | ')[1]) if ' | ' in l else {}); jd[int(d['major'])] = d
    elif l.startswith('R1OMEGA ') : om.append(kv(l))
QS = {'0': 'opt', '1': 'cap', '2': 'inf', '3': 'N3'}; VS = {'0': 'accF', '1': 'accH', '2': 'rej', '3': 'restore'}
c = dict(majors=len(maj), acc=0, acc_nogrow=0, rej_fterm=0, rej_qp=0, rej_other=0, halvings=sum(1 for o in om if o.get('halve') == '1'), rmax=0.0)
rows = []
for k in sorted(maj):
    m = maj[k]; q = qp.get(k, {}); j = jd.get(k, {})
    tr = float(m['tr']); c['rmax'] = max(c['rmax'], tr)
    v = VS.get(m['verdict'], m['verdict']); qs = QS.get(m['qp_status'], m['qp_status'])
    ratio = float(j['ratio']) if 'ratio' in j else None
    if v.startswith('acc'):
        c['acc'] += 1
        if ratio is not None and ratio < 0.75: c['acc_nogrow'] += 1
    elif qs != 'opt': c['rej_qp'] += 1
    elif j and 'ared_f' in j and float(j['ared_f']) < 0 and float(j.get('pred_v', '0')) >= 0: c['rej_fterm'] += 1
    else: c['rej_other'] += 1
    rows.append(f"| {k} | {tr:g} | {qs} {m['minors']} | {v} | {j.get('pred','—')} | {j.get('ared','—')} | {j.get('ratio','—')} | {j.get('pred_f','—')} | {j.get('ared_f','—')} | {j.get('pred_v','—')} | {j.get('v_new','—')} | {j.get('F_new','—')} | {j.get('omega','—')} |")
print(path.split('/')[-1], ' '.join(f"{a}={b}" for a, b in c.items()))
for l in L:
    if l.startswith(('R1RESTO_END', 'R1RESUME', 'R1END depth=0', 'R1WALLSTOP elapsed')) : print('  ', l[:260])
if table:
    print("| sub major | radius | QP | verdict | pred | ared | ratio | pred_f (ω·lin) | ared_f | pred_v = ared_v | v after | f after | ω |")
    print("|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
    print('\n'.join(rows))
