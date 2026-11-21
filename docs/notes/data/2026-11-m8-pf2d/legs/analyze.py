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
cells = sorted({re.match(r'(?:u0|prev|new|off)-r\d-(.*)\.csv$', os.path.basename(f)).group(1) for f in glob.glob(D + '/raw/*.csv')})
res = {}
print('## determinism and baseline checks')
for c in cells:
    for arm in ('u0', 'prev', 'new', 'off'):
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
print('\n## PF-2 per-minor instruction ratio vs U0 (instructions:u / qp_minors, medians)')
print('cell | U0 | new | new/U0 | off | off/U0 | new status fact minors | off status fact minors')
for c in cells:
    if (c, 'u0') in res and (c, 'new') in res:
        a, b = res[(c, 'u0')], res[(c, 'new')]; o = res.get((c, 'off'))
        print(f"{c} | {a['ipm']:.4g} | {b['ipm']:.4g} | {b['ipm']/a['ipm']:.3f} | " + (f"{o['ipm']:.4g} | {o['ipm']/a['ipm']:.3f}" if o else '- | -') +
              f" | {b['status']} {b['fact']} {b['minors']} | " + (f"{o['status']} {o['fact']} {o['minors']}" if o else '-'))
print('\n## solve tally per qp minor (designated solve only), off (U0-equivalent work) vs new')
for c in cells:
    if (c, 'new') not in res or (c, 'off') not in res: continue
    n, o = res[(c, 'new')], res[(c, 'off')]
    print(f'\n{c}: new minors {n["minors"]}, off minors {o["minors"]}')
    print('  bucket | solves/minor off | new | delta | rhs/minor off | new | refine/minor off | new')
    tot = [0, 0]
    for bi, b in enumerate(('ordinary', 'detector', 'polish', 'border')):
        vals = []
        for fi in range(3):
            i = fi * 4 + bi
            vals.append((o['tally'][i] / o['minors'], n['tally'][i] / n['minors']))
        print(f"  {b} | {vals[0][0]:.3f} | {vals[0][1]:.3f} | {vals[0][1]-vals[0][0]:+.3f} | {vals[1][0]:.3f} | {vals[1][1]:.3f} | {vals[2][0]:.3f} | {vals[2][1]:.3f}")
    to = [sum(o['tally'][f*4:(f+1)*4]) / o['minors'] for f in range(3)]
    tn = [sum(n['tally'][f*4:(f+1)*4]) / n['minors'] for f in range(3)]
    print(f"  total | {to[0]:.3f} | {tn[0]:.3f} | {tn[0]-to[0]:+.3f} | {to[1]:.3f} | {tn[1]:.3f} | {to[2]:.3f} | {tn[2]:.3f}")
    d_i = n['ipm'] - o['ipm']
    ds = tn[0] - to[0]
    print(f"  instr/minor new-off = {d_i:.4g} ; extra solves/minor {ds:.3f} ; implied instr per extra solve {d_i/ds if ds else float('nan'):.4g}")
    # share of the excess per component, by extra solve-columns (rhs) and by extra refinement steps
    ex = [(n['tally'][4+bi] / n['minors'] - o['tally'][4+bi] / o['minors']) if False else (n['tally'][4 + bi] / n['minors'] - o['tally'][4 + bi] / o['minors']) for bi in range(4)]
    exs = [(n['tally'][bi] / n['minors'] - o['tally'][bi] / o['minors']) for bi in range(4)]
    tots = sum(exs) or float('nan')
    print('  extra-solve share (of the sum of per-bucket deltas): ' + ', '.join(f'{b} {e/tots*100:.1f}%' for b, e in zip(('ordinary', 'detector', 'polish', 'border'), exs)))

print('\n## PF-2d: prev (pf2b head) vs new (levers) -- instr/minor ratios vs U0 and per-bucket solves per minor')
print('cell | new/U0 | prev/U0 | off/U0 | new/prev | bar(1.15) | new minors | prev minors | new status | prev status')
for c in cells:
    if (c, 'u0') in res and (c, 'new') in res and (c, 'prev') in res:
        a, b, p = res[(c, 'u0')], res[(c, 'new')], res[(c, 'prev')]; o = res.get((c, 'off'))
        print(f"{c} | {b['ipm']/a['ipm']:.3f} | {p['ipm']/a['ipm']:.3f} | " + (f"{o['ipm']/a['ipm']:.3f}" if o else '-') +
              f" | {b['ipm']/p['ipm']:.3f} | {'MET' if b['ipm']/a['ipm'] <= 1.15 else 'not met'} | {b['minors']} | {p['minors']} | {b['status']} | {p['status']}")
for c in cells:
    if (c, 'new') not in res or (c, 'prev') not in res: continue
    n, p = res[(c, 'new')], res[(c, 'prev')]
    print(f'\n{c}: solves/minor prev -> new (ordinary, detector, polish, border; total)')
    tp = [p['tally'][bi] / p['minors'] for bi in range(4)]; tn = [n['tally'][bi] / n['minors'] for bi in range(4)]
    print('  ' + ' | '.join(f'{b} {x:.3f}->{y:.3f}' for b, x, y in zip(('ord', 'det', 'pol', 'bor'), tp, tn)) + f' | total {sum(tp):.3f}->{sum(tn):.3f}')
