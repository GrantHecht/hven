#!/usr/bin/env python3
"""M8 AP-5 G4 r4 (DECLARATION-r4.md; copied from manifest-r3.py, roots re-pointed to .scratch/g4r4-*): MANIFEST of the raw dumps kept under the ignored .scratch/ (sha256, size, path relative to the worktree).
usage: manifest-r4.py <worktree> > MANIFEST-r4.txt"""
import hashlib
import os
import sys

wt = sys.argv[1]
roots = ['g4r4-runtime', 'g4r4-ic', 'g4r4-suites', 'g4r4-install-smoke', 'g4r4-tsan', 'g4r4-mutation']
print('# sha256  size  path (raw dumps under .scratch/, not committed)')
for r in roots:
    base = os.path.join(wt, '.scratch', r)
    for dirpath, _, files in sorted(os.walk(base)):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            h = hashlib.sha256(open(p, 'rb').read()).hexdigest()
            print('%s  %d  %s' % (h, os.path.getsize(p), os.path.relpath(p, wt)))
