#!/usr/bin/env python3
"""M8 F1: read hs_measure.sh's raw/. Per problem: the engine columns (status, iterations, factorizations,
f) must be equal across EVERY process of both arms (STOP otherwise); then per arm the median and range of
perf-stat instructions:u, the first-solve wall (solve_wall_s, informational) and init_s (informational).
Criterion (DECLARATION P13): engine identity, and per problem median(A) - median(B) >= 1.0e8 instructions
and median(B) < 1.0e8 (the spin is gone from every arm B process: max(B) < 1.0e8).
usage: hs_analyze.py <out-dir>"""
import glob, os, re, statistics, sys
out = sys.argv[1]
PROBS = ['hs006', 'hs010', 'hs035', 'hs071']
ok = True
def stat(p):
    for l in open(p):
        x = l.strip().split(',')
        if len(x) >= 3 and x[2].startswith('instructions'):
            return float(x[0])
    raise SystemExit('NO STAT ' + p)
def row(p):
    t = open(p).read()
    m = re.search(r'status=(\S+) iterations=(\d+) factorizations=(\d+) f=(\S+) init_s=(\S+) total_s=(\S+) kkt_s=(\S+) solve_wall_s=(\S+)', t)
    if not m:
        raise SystemExit('NO ROW ' + p)
    return m.groups()
print('%-6s %-4s %3s %14s %14s %14s %12s %12s %12s %10s' % ('prob', 'arm', 'n', 'instr_median', 'instr_min', 'instr_max',
                                                     'wall_med_s', 'init_med_s', 'total_med_s', 'kkt_med_s'))
summary = []
for p in PROBS:
    eng = set()
    med = {}
    for arm in 'AB':
        files = sorted(glob.glob(os.path.join(out, 'raw', '%s-r*-%s.out' % (p, arm))))
        ins = [stat(f[:-4] + '.stat') for f in files]
        rows = [row(f) for f in files]
        for r in rows:
            eng.add(r[:4])
        wall = [float(r[7]) for r in rows]; init = [float(r[4]) for r in rows]; kkt = [float(r[6]) for r in rows]
        tot = [float(r[5]) for r in rows]
        med[arm] = (statistics.median(ins), max(ins), statistics.median(wall), statistics.median(init))
        print('%-6s %-4s %3d %14.0f %14.0f %14.0f %12.6f %12.6f %12.6f %10.6f' % (p, arm, len(files), statistics.median(ins),
              min(ins), max(ins), statistics.median(wall), statistics.median(init), statistics.median(tot),
              statistics.median(kkt)))
        if len(files) != 5:
            print('PROCESS COUNT', p, arm, len(files)); ok = False
    if len(eng) != 1:
        print('ENGINE COLUMNS DIFFER', p, eng); ok = False
    else:
        print('  engine columns identical across 10 processes:', ' '.join(next(iter(eng))))
    d = med['A'][0] - med['B'][0]
    print('  median instructions A-B = %.4e (B/A = %.6f); median first-solve wall A=%.4f s B=%.4f s' %
          (d, med['B'][0] / med['A'][0], med['A'][2], med['B'][2]))
    if d < 1.0e8 or med['B'][1] >= 1.0e8:
        print('  CRITERION MISS', p); ok = False
print('HS_AS_DECLARED' if ok else 'HS_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
