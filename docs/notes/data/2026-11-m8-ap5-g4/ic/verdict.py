#!/usr/bin/env python3
"""M8 AP-5 G4 (../DECLARATION.md): the instruction-count verdict over passA-<mode>/analysis.txt.

PASS, per mode (walk, ssn, ipm, interior), requires all three:
  * counter check: 0 pairs with a difference or missing (else it is not an instruction measurement);
  * corpus instructions:u B/A in [0.999, 1.001];
  * every per-cell median instructions:u B/A in [0.995, 1.005] (interior: the one whole-leg row).
usage: verdict.py <ic-dir>
"""
import os
import re
import sys

ev = sys.argv[1]
ok = True
for mode in ('walk', 'ssn', 'ipm', 'interior'):
    path = os.path.join(ev, 'passA-%s' % mode, 'analysis.txt')
    if not os.path.exists(path):
        print('MODE %s MISSING' % mode)
        ok = False
        continue
    text = open(path).read()
    m = re.search(r'counter check: (\d+) \(round,cell\) pairs compared, (\d+) with', text)
    counters_ok = m is not None and m.group(2) == '0'
    m = re.search(r'CORPUS instructions B/A = ([0-9.]+)', text)
    corpus = float(m.group(1)) if m else float('nan')
    corpus_ok = 0.999 <= corpus <= 1.001
    cells = []
    in_table = False
    for line in text.splitlines():
        if line.startswith('cell '):
            in_table = True
            continue
        if in_table:
            if not line.strip():
                break
            parts = line.split()
            cells.append((parts[0], float(parts[-1])))
    worst = max(cells, key=lambda c: abs(c[1] - 1.0)) if cells else ('none', float('nan'))
    cells_ok = bool(cells) and all(0.995 <= r <= 1.005 for _, r in cells)
    verdict = counters_ok and corpus_ok and cells_ok
    ok = ok and verdict
    print('MODE %-8s counters %s  corpus B/A %.6f (%s)  worst cell %s %.5f (%s)  -> %s' % (
        mode, 'IDENTICAL' if counters_ok else 'MOVED', corpus, 'in' if corpus_ok else 'OUT',
        worst[0], worst[1], 'in' if cells_ok else 'OUT', 'PASS' if verdict else 'FAIL'))
print('IC_VERDICT %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
