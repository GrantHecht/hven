#!/usr/bin/env python3
"""The cost of attaching the counting callback, per arm (M8 AP-5 G3 P2b).

usage: attach_delta.py <unattached analysis.txt> <attached-kAll analysis.txt> <attached-kScalars analysis.txt>

Reads each analysis' `filtered samples (CRITERION)  A=... B=...` line (the interior leg, 27 cells, one process; median of 3
rounds). Prints, for arm A (the base, no best tracker) and arm B (G3, tracker armed by the callback): attached / unattached, and
the B/A ratio in each of the three conditions. INFORMATIONAL beside the legs' own B/A band: the criterion is each leg's B/A.
"""
import re
import sys

PAT = re.compile(r'filtered samples \(CRITERION\)\s+A=([0-9.eE+-]+)\s+B=([0-9.eE+-]+)')


def read(path):
    for line in open(path):
        m = PAT.search(line)
        if m:
            return float(m.group(1)), float(m.group(2))
    sys.exit(f'NO_CRITERION_LINE {path}')


un, kall, ksc = (read(p) for p in sys.argv[1:4])
print('interior leg, filtered instructions:u samples (median of 3 rounds, summed over the 27 cells)')
for label, (a, b) in (('unattached', un), ('attached kAll', kall), ('attached kScalars', ksc)):
    print(f'  {label:18s} A={a:.6e} B={b:.6e} B/A={b / a:.6f}')
for label, (a, b) in (('kAll', kall), ('kScalars', ksc)):
    print(f'  attach cost at {label:9s} arm A: {a / un[0] - 1:+.4%}   arm B: {b / un[1] - 1:+.4%}   '
          f'(B minus A: {(b / un[1] - a / un[0]):+.4%})')
