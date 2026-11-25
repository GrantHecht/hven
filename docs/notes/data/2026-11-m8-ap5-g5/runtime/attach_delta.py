#!/usr/bin/env python3
"""The cost of attaching the counting callback, per arm and mode (M8 AP-5 G5 P2b; from G3's attach_delta.py).

usage: attach_delta.py <mode> <unattached> <attached-scalars> <attached-default> <attached-all>   (analysis.txt files)

Reads each analysis' `filtered samples (CRITERION)  A=... B=...` line (median of 3 rounds, summed over the 27 cells). Prints the
B/A ratio per condition, each arm's attach cost against its unattached leg, and the NEW kAll's cost on arm B (attached-all over
attached-default). Arm A's kAll is G4's kVectors|kDiagnostics. INFORMATIONAL beside the legs' own B/A band.
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


mode = sys.argv[1]
un, sc, df, al = (read(p) for p in sys.argv[2:6])
print(f'{mode} leg, filtered instructions:u samples (median of 3 rounds, summed over the 27 cells)')
for label, (a, b) in (('unattached', un), ('attached kScalars', sc), ('attached default', df),
                      ('attached kAll', al)):
    print(f'  {label:18s} A={a:.6e} B={b:.6e} B/A={b / a:.6f}')
for label, (a, b) in (('kScalars', sc), ('default', df), ('kAll', al)):
    print(f'  attach cost at {label:9s} arm A: {a / un[0] - 1:+.4%}   arm B: {b / un[1] - 1:+.4%}   '
          f'(B minus A: {(b / un[1] - a / un[0]):+.4%})')
print(f'  new kAll cost on arm B (attached kAll / attached default): {al[1] / df[1] - 1:+.4%}')
