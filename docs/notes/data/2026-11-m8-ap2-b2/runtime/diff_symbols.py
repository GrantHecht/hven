#!/usr/bin/env python3
"""Per-symbol instructions:u sample counts, arm B minus arm A, from two perf.data files.
usage: diff_symbols.py A.data B.data"""
import re
import subprocess
import sys


def load(path):
    out = subprocess.run(['perf', 'report', '-i', path, '--sort', 'symbol', '--stdio', '-n',
                          '--no-children'], capture_output=True, text=True).stdout
    d = {}
    for line in out.splitlines():
        m = re.match(r'\s*([\d.]+)%\s+(\d+)\s+\[\.\]\s+(.*\S)', line)
        if m:
            d[m.group(3)[:140]] = int(m.group(2))
    return d


a, b = load(sys.argv[1]), load(sys.argv[2])
ta, tb = sum(a.values()), sum(b.values())
print('samples A=%d B=%d B/A=%.5f' % (ta, tb, tb / ta))
rows = sorted(set(a) | set(b), key=lambda k: -(b.get(k, 0) - a.get(k, 0)))
print('largest increases (B-A samples):')
for k in rows[:25]:
    print('%+8d  A=%8d B=%8d  %s' % (b.get(k, 0) - a.get(k, 0), a.get(k, 0), b.get(k, 0), k))
print('largest decreases:')
for k in rows[-10:]:
    print('%+8d  A=%8d B=%8d  %s' % (b.get(k, 0) - a.get(k, 0), a.get(k, 0), b.get(k, 0), k))

# The explanatory reading (not a criterion): the same totals without the wall-clock
# spin symbols -- MKL's clock-frequency calibration and the time() polling it drives.
SPIN = ('difftime', 'difftime@plt', 'mkl_serv_get_clocks_frequency', '__vdso_time')
sa = sum(v for k, v in a.items() if k.split()[0] in SPIN)
sb = sum(v for k, v in b.items() if k.split()[0] in SPIN)
print('spin-symbol samples A=%d B=%d' % (sa, sb))
print('samples without the spin symbols A=%d B=%d B/A=%.6f' % (ta - sa, tb - sb, (tb - sb) / (ta - sa)))
