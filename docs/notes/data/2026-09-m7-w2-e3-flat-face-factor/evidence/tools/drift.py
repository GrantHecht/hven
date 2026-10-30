#!/usr/bin/env python3
"""NC2 scratch: the first pass at which |Ae x - be| exceeds THR after having been <= THR, and the
trace lines around it. usage: drift.py TRACE [THR=1e-8] [CONTEXT=12]"""
import re, sys

path = sys.argv[1]
thr = float(sys.argv[2]) if len(sys.argv) > 2 else 1e-8
ctx = int(sys.argv[3]) if len(sys.argv) > 3 else 12
lines = [l.rstrip('\n') for l in open(path) if l.startswith('NC2 ')]
ok = False
for i, l in enumerate(lines):
    m = re.search(r' eq=(\S+)', l)
    if not m:
        continue
    e = float(m.group(1))
    if e <= thr:
        ok = True
    elif ok:
        print(f'first drift above {thr} at trace line {i}:')
        for k in range(max(0, i - ctx), min(len(lines), i + 4)):
            print(('>> ' if k == i else '   ') + lines[k])
        break
else:
    print('no drift after satisfaction')
# growth profile: eq at every 500th PASS
passes = [l for l in lines if l.startswith('NC2 PASS')]
print('profile:', ' '.join(re.search(r' eq=(\S+)', p).group(1) for p in passes[::500]))
