#!/usr/bin/env python3
"""AP-2 B8: normalize an Accelerate sweep listing for comparison across the rename: drop every
line:col, read hven::solvers:: as hven::, and sort (the sweep's order follows line numbers).
The summary block (counts) is kept. usage: accel_normalize.py <sweep.txt>"""
import re, sys
out = []
for line in open(sys.argv[1]):
    line = re.sub(r':[0-9]+:[0-9]+:', ':', line)
    line = line.replace('hven::solvers::', 'hven::')
    out.append(line)
sys.stdout.write(''.join(sorted(out)))
