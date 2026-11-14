#!/usr/bin/env python3
# M8 DN-1d (../DECLARATION.txt D). Criterion (C280 ruling 1, C282 clarification), with E0's definitions:
#   every CAT and every EXACT factorization flagged (finite flag or exactly singular); OTHER (benign) flagged <= 70;
#   SCREEN-stage dsycon_rook calls <= 1 % of the raw dense factorizations (E0's 98,409 records). Screen-stage calls are
#   the detector's estimates on the dumped factorizations; a factorization E0 did not dump had a rook D-ratio >= 1e-15
#   (checked here from E0's records: every record below the screen or exactly singular is a dumped one) and buys none.
#   Response-stage estimates (the equilibrated refactors' own detector runs) are not in this count; the S leg reports
#   the refactors.
# usage: analyze.py MANIFEST_CLASS DET_CSV E0_FAC_DIR
import csv, glob, os, sys
from collections import Counter
cls = {r['path']: (r['cell'], r['seq'], r['class']) for r in csv.DictReader(open(sys.argv[1]))}
det = list(csv.DictReader(open(sys.argv[2])))
assert len(det) == len(cls), (len(det), len(cls))
c = Counter(); est = 0; missed = []
for r in det:
    cell, seq, k = cls[r['path']]
    fl = int(r['flagged']) or int(r['exact'])
    c[(k, fl)] += 1
    est += int(r['estimates'])
    if k in ('CAT', 'EXACT') and not fl:
        missed.append(f'{cell}/{seq}/{k}')
raw = 0; below = set()
for f in sorted(glob.glob(os.path.join(sys.argv[3], '*.fac.csv'))):
    cell = os.path.basename(f).split('.')[0]
    for r in csv.DictReader(open(f)):
        raw += 1
        if int(r['rook_info']) != 0 or int(r['rook_sing']) != 0 or float(r['rook_ratio']) < 1e-15:
            below.add((cell, r['seq']))
dumped = {(cell, seq) for cell, seq, _ in cls.values()}
undumped = sorted(below - dumped)
cat = (c[('CAT', 1)], c[('CAT', 1)] + c[('CAT', 0)])
exa = (c[('EXACT', 1)], c[('EXACT', 1)] + c[('EXACT', 0)])
ben = c[('OTHER', 1)]
share = est / raw if raw else float('nan')
print(f'dumps {len(det)}; CAT flagged {cat[0]}/{cat[1]}; EXACT flagged {exa[0]}/{exa[1]}; OTHER (benign) flagged {ben} '
      f'of {ben + c[("OTHER", 0)]}; missed {missed}')
print(f'screen-stage dsycon_rook calls {est} of {raw} raw dense factorizations = {100 * share:.3f} %; E0 records below '
      f'the screen or exactly singular {len(below)}, of them not dumped {len(undumped)} {undumped[:10]}')
ok = not missed and cat[1] == 106 and exa[1] == 53 and ben <= 70 and share <= 0.01 and not undumped
print('DETECTOR_PASS' if ok else 'DETECTOR_FAIL')
sys.exit(0 if ok else 1)
