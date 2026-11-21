#!/usr/bin/env python3
"""PF-2d re-measure analysis (RESULTS-PF-12's analyze.py extended with the solve-tally sidecars and the `off` arm).
usage: analyze.py   (reads ../measure/raw, ../measure/perf)"""
import csv, glob, os, re, statistics as st, sys
D = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "measure")
R = os.path.abspath(os.path.join(D, "..", "..", "..", "..", ".."))
HDR = [l for l in open(R + '/bench/baselines/2026-10-m7-close-corpus/walk_baseline.csv') if l.startswith('cell_id,')][0]
def rows(p):
    ls = [l for l in open(p) if not l.startswith('#')]
    if not ls[0].startswith('cell_id,'): ls = [HDR] + ls
    return {x['cell_id']: x for x in csv.DictReader(ls)}
U0 = rows(R + '/bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv')
def perf(p):
    d = {}
    for l in open(p):
        m = re.match(r'\s*([\d,]+)\s+(\S+)', l)
        if m: d[m.group(2).split(':')[0]] = int(m.group(1).replace(',', ''))
    return d
def tally(p):
    ls = open(p).read().split('\n')
    return [int(x) for x in ls[1].split(',')]
CNT = ['status', 'factorizations', 'qp_minors', 'escapes', 'qp_subproblems', 'qp_fact_per_qp']
NAMES = [f + '_' + b for f in ('solves', 'rhs', 'refine') for b in ('ordinary', 'detector', 'polish', 'border')]
cells = sorted({re.match(r'(?:u0|prev|newL|newF|newG)-r\d-(.*)\.csv$', os.path.basename(f)).group(1) for f in glob.glob(D + '/raw/*.csv')})
res = {}
print('## determinism and baseline checks')
for c in cells:
    for arm in ('u0', 'prev', 'newL', 'newF', 'newG'):
        rs, pf, ty = [], [], []
        for r in (1, 2, 3):
            f = f'{D}/raw/{arm}-r{r}-{c}.csv'
            if os.path.exists(f) and os.path.getsize(f):
                rs.append(rows(f)[c]); pf.append(perf(f'{D}/perf/{arm}-r{r}-{c}.txt'))
                if arm != 'u0': ty.append(tally(f + '.tally'))
        if not rs: continue
        ident = all(all(rs[0][k] == y[k] for k in CNT) for y in rs) and all(t == ty[0] for t in ty)
        m = int(rs[0]['qp_minors'])
        ipm = [p['instructions'] / m for p in pf]
        base = (U0.get(c) if arm == 'u0' else None)
        mb = all(base[k] == rs[0][k] for k in CNT) if base else None
        res[(c, arm)] = dict(ipm=st.median(ipm), minors=m, fact=int(rs[0]['factorizations']), status=rs[0]['status'],
                             instr=st.median(p['instructions'] for p in pf), tally=ty[0] if ty else None)
        print(f"{c} | {arm} | {rs[0]['status']} fact {rs[0]['factorizations']} minors {m} | instr/minor {st.median(ipm):.1f} | spread {(max(ipm)-min(ipm))/st.median(ipm)*100:.3f}% | rounds identical {ident}({len(rs)}) | = U0 baseline {mb}")
print('\n## PF-2d: per-minor instruction ratio vs U0 (instructions:u / qp_minors, medians), bar 1.15 on tier A')
print('cell | prev/U0 | newL/U0 | newF/U0 | newG/U0 | newL/prev | newL bar | minors prev/newL/newF/newG | status prev/newL/newF/newG')
for c in cells:
    if all((c, k) in res for k in ('u0', 'prev', 'newL', 'newF', 'newG')):
        u, p, l, f, g = (res[(c, k)] for k in ('u0', 'prev', 'newL', 'newF', 'newG'))
        print(f"{c} | {p['ipm']/u['ipm']:.3f} | {l['ipm']/u['ipm']:.3f} | {f['ipm']/u['ipm']:.3f} | {g['ipm']/u['ipm']:.3f} | {l['ipm']/p['ipm']:.3f} | {'MET' if l['ipm']/u['ipm'] <= 1.15 else 'not met'} | {p['minors']}/{l['minors']}/{f['minors']}/{g['minors']} | {p['status']}/{l['status']}/{f['status']}/{g['status']}")
print('\n## solves per qp minor by bucket (ordinary, detector, polish, border; total), designated solve only')
for c in cells:
    if not all((c, k) in res for k in ('prev', 'newL', 'newF', 'newG')): continue
    print(f'\n{c}:')
    for k in ('prev', 'newL', 'newF', 'newG'):
        r = res[(c, k)]
        t = [r['tally'][bi] / r['minors'] for bi in range(4)]
        print(f"  {k:5s} " + ' | '.join(f'{b} {x:.3f}' for b, x in zip(('ord', 'det', 'pol', 'bor'), t)) + f' | total {sum(t):.3f}')
