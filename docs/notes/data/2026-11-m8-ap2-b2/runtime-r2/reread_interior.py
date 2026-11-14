#!/usr/bin/env python3
"""POST-HOC CORRECTED READING of the recorded interior leg (disclosed; not a re-run).
analyze_r2.py's symbol regex kept perf's two trailing '-' columns in the symbol name, so no
excluded symbol matched and its 'filtered' sum was the unfiltered one. This re-reads the
SAME tables with those columns stripped and applies the declared criterion unchanged."""
import os, re, statistics, sys
EXCLUDED = {'difftime', 'difftime@plt', 'mkl_serv_get_clocks_frequency', '__vdso_time',
            '__vdso_clock_gettime', 'time', 'clock_gettime'}
LINE = re.compile(r'\s*([\d.]+)%\s+(\d+)\s+\[(.)\]\s+(.*?)(?:\s+-\s+-)?\s*$')
leg = sys.argv[1]
per = {}
for arm in 'AB':
    runs = []
    for r in (1, 2, 3):
        s = {}
        for line in open(os.path.join(leg, 'syms', 'interior-r%d-%s-all.txt' % (r, arm))):
            m = LINE.match(line)
            if m:
                s[m.group(4)] = s.get(m.group(4), 0) + int(m.group(2))
        tot = sum(s.values()); ex = sum(v for k, v in s.items() if k in EXCLUDED)
        hit = sorted(k for k in s if k in EXCLUDED)
        runs.append((tot - ex, tot, ex))
        print('r%d %s total=%d excluded=%d filtered=%d matched=%s' % (r, arm, tot, ex, tot - ex, hit))
    per[arm] = runs
for i, n in enumerate(['filtered (CRITERION)', 'all samples', 'excluded']):
    a = statistics.median(x[i] for x in per['A']); b = statistics.median(x[i] for x in per['B'])
    print('%-22s A=%d B=%d B/A=%.6f' % (n, a, b, b / a))
ratio = statistics.median(x[0] for x in per['B']) / statistics.median(x[0] for x in per['A'])
for k in range(3):
    print('round %d filtered B/A=%.6f' % (k + 1, per['B'][k][0] / per['A'][k][0]))
print('CORRECTED READING interior FILTERED B/A = %.6f -> %s' % (ratio,
      'WITHIN [0.995, 1.005]' if 0.995 <= ratio <= 1.005 else 'OUTSIDE [0.995, 1.005]'))
