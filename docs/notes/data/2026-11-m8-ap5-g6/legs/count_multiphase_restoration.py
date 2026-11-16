#!/usr/bin/env python3
# M8 AP-5 G6 (../DECLARATION.md): counts the interior-corpus rows that are multi-phase (phase_count > 1) AND run under a restoration strategy
# (variant header restoration != off). Only those could move under G6's whole-solve restoration counters. usage: count_multiphase_restoration.py <interior csv>
import sys
rest = {}
rows = []
hdr = None
for line in open(sys.argv[1]):
    line = line.rstrip('\n')
    if line.startswith('# variant:'):
        t = line.split()
        name = t[2]
        rest[name] = [x for x in t if x.startswith('restoration=')][0].split('=')[1]
    elif line and not line.startswith('#'):
        f = line.split(',')
        if hdr is None:
            hdr = f
            continue
        rows.append(dict(zip(hdr, f)))
n_multi = n_rest = n_both = 0
for r in rows:
    parts = r['cell_id'].split('/')
    variant = parts[-1] if parts[-1] in rest else 'base'
    on = rest.get(variant, 'off') != 'off'
    multi = int(r['phase_count']) > 1
    n_multi += multi; n_rest += on; n_both += multi and on
print(f'rows={len(rows)} multi_phase={n_multi} with_restoration={n_rest} multi_phase_and_restoration={n_both}')
sys.exit(0 if n_both == 0 else 1)
