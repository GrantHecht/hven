#!/usr/bin/env python3
"""PF-2e P3 analysis (pf2d's analyze.py, two sources). Arms u0, prev, newL, newF, newG are read from PF-2d's measure/raw
(carried); newLc (this leg's newL control re-take) and newE from this measure/raw. Determinism STOP and CONTROL as
../DECLARATION-PF-2e.md section 4. usage: analyze.py"""
import csv, glob, os, re, statistics as st, sys
HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D_E = os.path.join(HERE, "measure")
D_D = os.path.abspath(os.path.join(HERE, "..", "2026-11-m8-pf2d", "measure"))
R = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
HDR = [l for l in open(R + '/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv') if l.startswith('cell_id,')][0]


def rows(p):
    ls = [l for l in open(p) if not l.startswith('#')]
    if not ls[0].startswith('cell_id,'):
        ls = [HDR] + ls
    return {x['cell_id']: x for x in csv.DictReader(ls)}


U0 = rows(R + '/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv')


def perf(p):
    d = {}
    for l in open(p):
        m = re.match(r'\s*([\d,]+)\s+(\S+)', l)
        if m:
            d[m.group(2).split(':')[0]] = int(m.group(1).replace(',', ''))
    return d


def tally(p):
    ls = open(p).read().split('\n')
    return [int(x) for x in ls[1].split(',')]


CNT = ['status', 'factorizations', 'qp_minors', 'escapes', 'qp_subproblems', 'qp_fact_per_qp']
SRC = {'u0': (D_D, 'u0'), 'prev': (D_D, 'prev'), 'newL': (D_D, 'newL'), 'newF': (D_D, 'newF'), 'newG': (D_D, 'newG'),
       'newLc': (D_E, 'newL'), 'newE': (D_E, 'newE')}
cells = sorted({re.match(r'newE-r\d-(.*)\.csv$', os.path.basename(f)).group(1) for f in glob.glob(D_E + '/raw/newE-*.csv')})
res = {}
stop = False
print('## determinism and baseline checks')
for c in cells:
    for arm, (dd, name) in SRC.items():
        rs, pf, ty = [], [], []
        for r in (1, 2, 3):
            f = f'{dd}/raw/{name}-r{r}-{c}.csv'
            if os.path.exists(f) and os.path.getsize(f):
                rs.append(rows(f)[c]); pf.append(perf(f'{dd}/perf/{name}-r{r}-{c}.txt'))
                if arm != 'u0':
                    ty.append(tally(f + '.tally'))
        if not rs:
            continue
        ident = all(all(rs[0][k] == y[k] for k in CNT) for y in rs) and all(t == ty[0] for t in ty)
        if arm in ('newLc', 'newE') and not (ident and len(rs) == 3):
            stop = True
        m = int(rs[0]['qp_minors'])
        ipm = [p['instructions'] / m for p in pf]
        res[(c, arm)] = dict(ipm=st.median(ipm), minors=m, fact=int(rs[0]['factorizations']), status=rs[0]['status'],
                             row=rs[0], instr=st.median(p['instructions'] for p in pf), tally=ty[0] if ty else None)
        print(f"{c} | {arm} | {rs[0]['status']} fact {rs[0]['factorizations']} minors {m} | instr/minor {st.median(ipm):.1f} | "
              f"spread {(max(ipm) - min(ipm)) / st.median(ipm) * 100:.3f}% | rounds identical {ident}({len(rs)})")
print('\n## CONTROL: newLc (this leg) vs PF-2d newL')
control_ok = True
for c in cells:
    a, b = res.get((c, 'newLc')), res.get((c, 'newL'))
    if not (a and b):
        control_ok = False; print(f'{c}: MISSING'); continue
    same = all(a['row'][k] == b['row'][k] for k in CNT) and a['tally'] == b['tally']
    rel = abs(a['instr'] - b['instr']) / b['instr']
    ok = same and rel <= 1e-3
    control_ok &= ok
    print(f'{c}: counters+tally equal {same}; instructions rel diff {rel * 100:.4f}% -> {"OK" if ok else "FAIL"}')
print('CONTROL_' + ('OK' if control_ok else 'FAILED') + ('' if not stop else ' (DETERMINISM_STOP)'))
print('\n## PF-2e: per-minor instruction ratio vs U0 (instructions:u / qp_minors, medians), bar 1.15 on tier A')
print('cell | prev | newL | newF | newG | **newE** | newE/newL | newE bar | minors newL/newE | status newL/newE')
for c in cells:
    if all((c, k) in res for k in ('u0', 'prev', 'newL', 'newF', 'newG', 'newE')):
        u = res[(c, 'u0')]['ipm']
        p, l, f, g, e = (res[(c, k)] for k in ('prev', 'newL', 'newF', 'newG', 'newE'))
        print(f"{c} | {p['ipm'] / u:.3f} | {l['ipm'] / u:.3f} | {f['ipm'] / u:.3f} | {g['ipm'] / u:.3f} | **{e['ipm'] / u:.3f}** | "
              f"{e['ipm'] / l['ipm']:.3f} | {'MET' if e['ipm'] / u <= 1.15 else 'not met'} | {l['minors']}/{e['minors']} | "
              f"{l['status']}/{e['status']}")
print('\n## share of G\'s per-minor saving (newL - newX) / (newL - newG)')
for c in cells:
    if all((c, k) in res for k in ('newL', 'newG', 'newE', 'newF')):
        l, g, e, f = (res[(c, k)]['ipm'] for k in ('newL', 'newG', 'newE', 'newF'))
        if l != g:
            print(f'{c}: newE {(l - e) / (l - g) * 100:.0f} %, newF {(l - f) / (l - g) * 100:.0f} %')
print('\n## solves per qp minor by bucket (ordinary, detector, polish, border; total), designated solve only')
for c in cells:
    if not all((c, k) in res for k in ('newL', 'newG', 'newE')):
        continue
    print(f'\n{c}:')
    for k in ('newL', 'newF', 'newG', 'newE'):
        r = res[(c, k)]
        t = [r['tally'][bi] / r['minors'] for bi in range(4)]
        print(f"  {k:5s} " + ' | '.join(f'{b} {x:.3f}' for b, x in zip(('ord', 'det', 'pol', 'bor'), t)) + f' | total {sum(t):.3f}')
sys.exit(0 if (control_ok and not stop) else 1)
