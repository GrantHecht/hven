#!/usr/bin/env python3
"""E2-D4: one-screen summary of a D4T trace: pass count, moving/zero steps, adds, exchanges, drops,
detector removals, retreats, start repair, the terminal event, and the last 3 passes' key lines."""
import sys, re
for path in sys.argv[1:]:
    L = open(path).read().splitlines()
    c = dict(pass_=0, mov=0, zero=0, add=0, exch=0, drop=0, det=0, retreat=0, nodrop=0, terminal=0, ride=0, pass_over=0, rej=0)
    for l in L:
        if ' PASS ' in l: c['pass_'] += 1
        elif ' STEP ' in l:
            dx = float(re.search(r'dx=(\S+)', l).group(1)); c['mov' if abs(dx) > 1e-9 else 'zero'] += 1
            if 'kind=none' not in l: c['add'] += 1
            if 'exch=1' in l: c['exch'] += 1
        elif ' DROP ' in l: c['drop'] += 1
        elif 'DETECT_REMOVE' in l: c['det'] += 1
        elif 'BORDER_RETREAT' in l: c['retreat'] += 1
        elif 'NODROP' in l: c['nodrop'] += 1
        elif 'TERMINAL' in l: c['terminal'] += 1
        elif ' RIDE ' in l: c['ride'] += 1
        elif 'PASSOVER within=1' in l: c['pass_over'] += 1
    end = [l for l in L if 'ADD_EXIT' in l or 'TERMINAL' in l or ' end ' in l]
    print(path.split('/')[-1], ' '.join(f'{k}={v}' for k, v in c.items()))
    for l in end[-3:]: print('   ', l[:200])
