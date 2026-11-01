#!/usr/bin/env python3
"""E2-D4: tail analysis of a capped walk trace: the shortest period of the last ~400 decisions (add/exch/drop keys), the
share of zero-motion steps in the tail, the largest |multiplier| at a DROP in the tail (1/mu-sized = a leaking set), the f range."""
import sys, re
for path in sys.argv[1:]:
    L = open(path).read().splitlines()
    ev = []; drops = []; zero = 0; steps = 0; fs = []
    for l in L:
        if ' STEP ' in l:
            t = l.split(); ev.append(('A', t[3], t[4], 'x' if 'exch=1' in l else '')); steps += 1
            if abs(float(re.search(r'dx=(\S+)', l).group(1))) <= 1e-9: zero += 1
        elif ' DROP ' in l:
            t = l.split(); ev.append(('D', t[3], t[4], '')); drops.append(abs(float(re.search(r'mult=(\S+)', l).group(1))))
        elif ' PASS ' in l:
            m = re.search(r' f=(\S+)', l); fs.append(float(m.group(1)))
    tail = ev[-400:]
    per = None
    for p in range(1, 200):
        if len(tail) >= 2 * p and all(tail[-i] == tail[-i - p] for i in range(1, min(len(tail) - p, 3 * p) + 1)):
            per = p; break
    tz = sum(1 for l in L[-4000:] if ' STEP ' in l and abs(float(re.search(r'dx=(\S+)', l).group(1))) <= 1e-9)
    ts = sum(1 for l in L[-4000:] if ' STEP ' in l)
    print(f"{path.split('/')[-1]}: events={len(ev)} tail_period={per} tail_zero_steps={tz}/{ts} max|mult| at tail drops={max(drops[-200:]) if drops else 0:.3e} f_first={fs[0] if fs else 0:.6g} f_last={fs[-1] if fs else 0:.6g}")
    if per: print('   cycle:', ' '.join(''.join(e) for e in tail[-per:])[:300])
